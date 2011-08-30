// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_action.h"

#include <inttypes.h>

#include <sstream>

#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/time.h>
#include <base/logging.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "update_engine/action_pipe.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/utils.h"

using base::Time;
using base::TimeDelta;
using std::string;

namespace chromeos_update_engine {

namespace {

const string kGupdateVersion("ChromeOSUpdateEngine-0.1.0.0");

// This is handy for passing strings into libxml2
#define ConstXMLStr(x) (reinterpret_cast<const xmlChar*>(x))

// These are for scoped_ptr_malloc, which is like scoped_ptr, but allows
// a custom free() function to be specified.
class ScopedPtrXmlDocFree {
 public:
  inline void operator()(void* x) const {
    xmlFreeDoc(reinterpret_cast<xmlDoc*>(x));
  }
};
class ScopedPtrXmlFree {
 public:
  inline void operator()(void* x) const {
    xmlFree(x);
  }
};
class ScopedPtrXmlXPathObjectFree {
 public:
  inline void operator()(void* x) const {
    xmlXPathFreeObject(reinterpret_cast<xmlXPathObject*>(x));
  }
};
class ScopedPtrXmlXPathContextFree {
 public:
  inline void operator()(void* x) const {
    xmlXPathFreeContext(reinterpret_cast<xmlXPathContext*>(x));
  }
};

// Returns true if |ping_days| has a value that needs to be sent,
// false otherwise.
bool ShouldPing(int ping_days) {
  return ping_days > 0 || ping_days == OmahaRequestAction::kNeverPinged;
}

// Returns an XML ping element attribute assignment with attribute
// |name| and value |ping_days| if |ping_days| has a value that needs
// to be sent, or an empty string otherwise.
string GetPingAttribute(const string& name, int ping_days) {
  if (ShouldPing(ping_days)) {
    return StringPrintf(" %s=\"%d\"", name.c_str(), ping_days);
  }
  return "";
}

// Returns an XML ping element if any of the elapsed days need to be
// sent, or an empty string otherwise.
string GetPingBody(int ping_active_days, int ping_roll_call_days) {
  string ping_active = GetPingAttribute("a", ping_active_days);
  string ping_roll_call = GetPingAttribute("r", ping_roll_call_days);
  if (!ping_active.empty() || !ping_roll_call.empty()) {
    return StringPrintf("        <o:ping active=\"1\"%s%s></o:ping>\n",
                        ping_active.c_str(),
                        ping_roll_call.c_str());
  }
  return "";
}

string FormatRequest(const OmahaEvent* event,
                     const OmahaRequestParams& params,
                     bool ping_only,
                     int ping_active_days,
                     int ping_roll_call_days,
                     PrefsInterface* prefs) {
  string body;
  if (event == NULL) {
    body = GetPingBody(ping_active_days, ping_roll_call_days);
    if (!ping_only) {
      body += "        <o:updatecheck></o:updatecheck>\n";
      // If this is the first update check after a reboot following a previous
      // update, generate an event containing the previous version number. If
      // the previous version preference file doesn't exist the event is still
      // generated with a previous version of 0.0.0.0 -- this is relevant for
      // older clients or new installs. The previous version event is not sent
      // for ping-only requests because they come before the client has
      // rebooted.
      string prev_version;
      if (!prefs->GetString(kPrefsPreviousVersion, &prev_version)) {
        prev_version = "0.0.0.0";
      }
      if (!prev_version.empty()) {
        body += StringPrintf(
            "        <o:event eventtype=\"%d\" eventresult=\"%d\" "
            "previousversion=\"%s\"></o:event>\n",
            OmahaEvent::kTypeUpdateComplete,
            OmahaEvent::kResultSuccessReboot,
            XmlEncode(prev_version).c_str());
        LOG_IF(WARNING, !prefs->SetString(kPrefsPreviousVersion, ""))
            << "Unable to reset the previous version.";
      }
    }
  } else {
    // The error code is an optional attribute so append it only if the result
    // is not success.
    string error_code;
    if (event->result != OmahaEvent::kResultSuccess) {
      error_code = StringPrintf(" errorcode=\"%d\"", event->error_code);
    }
    body = StringPrintf(
        "        <o:event eventtype=\"%d\" eventresult=\"%d\"%s></o:event>\n",
        event->type, event->result, error_code.c_str());
  }
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" "
      "version=\"" + XmlEncode(kGupdateVersion) + "\" "
      "updaterversion=\"" + XmlEncode(kGupdateVersion) + "\" "
      "protocol=\"2.0\" ismachine=\"1\">\n"
      "    <o:os version=\"" + XmlEncode(params.os_version) + "\" platform=\"" +
      XmlEncode(params.os_platform) + "\" sp=\"" +
      XmlEncode(params.os_sp) + "\"></o:os>\n"
      "    <o:app appid=\"" + XmlEncode(params.app_id) + "\" version=\"" +
      XmlEncode(params.app_version) + "\" "
      "lang=\"" + XmlEncode(params.app_lang) + "\" track=\"" +
      XmlEncode(params.app_track) + "\" board=\"" +
      XmlEncode(params.os_board) + "\" hardware_class=\"" +
      XmlEncode(params.hardware_class) + "\" delta_okay=\"" +
      (params.delta_okay ? "true" : "false") + "\">\n" + body +
      "    </o:app>\n"
      "</o:gupdate>\n";
}

}  // namespace {}

// Encodes XML entities in a given string with libxml2. input must be
// UTF-8 formatted. Output will be UTF-8 formatted.
string XmlEncode(const string& input) {
  //  // TODO(adlr): if allocating a new xmlDoc each time is taking up too much
  //  // cpu, considering creating one and caching it.
  //  scoped_ptr_malloc<xmlDoc, ScopedPtrXmlDocFree> xml_doc(
  //      xmlNewDoc(ConstXMLStr("1.0")));
  //  if (!xml_doc.get()) {
  //    LOG(ERROR) << "Unable to create xmlDoc";
  //    return "";
  //  }
  scoped_ptr_malloc<xmlChar, ScopedPtrXmlFree> str(
      xmlEncodeEntitiesReentrant(NULL, ConstXMLStr(input.c_str())));
  return string(reinterpret_cast<const char *>(str.get()));
}

OmahaRequestAction::OmahaRequestAction(PrefsInterface* prefs,
                                       const OmahaRequestParams& params,
                                       OmahaEvent* event,
                                       HttpFetcher* http_fetcher,
                                       bool ping_only)
    : prefs_(prefs),
      params_(params),
      event_(event),
      http_fetcher_(http_fetcher),
      ping_only_(ping_only),
      ping_active_days_(0),
      ping_roll_call_days_(0) {}

OmahaRequestAction::~OmahaRequestAction() {}

// Calculates the value to use for the ping days parameter.
int OmahaRequestAction::CalculatePingDays(const string& key) {
  int days = kNeverPinged;
  int64_t last_ping = 0;
  if (prefs_->GetInt64(key, &last_ping) && last_ping >= 0) {
    days = (Time::Now() - Time::FromInternalValue(last_ping)).InDays();
    if (days < 0) {
      // If |days| is negative, then the system clock must have jumped
      // back in time since the ping was sent. Mark the value so that
      // it doesn't get sent to the server but we still update the
      // last ping daystart preference. This way the next ping time
      // will be correct, hopefully.
      days = kPingTimeJump;
      LOG(WARNING) <<
          "System clock jumped back in time. Resetting ping daystarts.";
    }
  }
  return days;
}

void OmahaRequestAction::InitPingDays() {
  // We send pings only along with update checks, not with events.
  if (IsEvent()) {
    return;
  }
  // TODO(petkov): Figure a way to distinguish active use pings
  // vs. roll call pings. Currently, the two pings are identical. A
  // fix needs to change this code as well as UpdateLastPingDays.
  ping_active_days_ = CalculatePingDays(kPrefsLastActivePingDay);
  ping_roll_call_days_ = CalculatePingDays(kPrefsLastRollCallPingDay);
}

void OmahaRequestAction::PerformAction() {
  http_fetcher_->set_delegate(this);
  InitPingDays();
  if (ping_only_ &&
      !ShouldPing(ping_active_days_) &&
      !ShouldPing(ping_roll_call_days_)) {
    processor_->ActionComplete(this, kActionCodeSuccess);
    return;
  }
  string request_post(FormatRequest(event_.get(),
                                    params_,
                                    ping_only_,
                                    ping_active_days_,
                                    ping_roll_call_days_,
                                    prefs_));
  http_fetcher_->SetPostData(request_post.data(), request_post.size());
  LOG(INFO) << "Posting an Omaha request to " << params_.update_url;
  LOG(INFO) << "Request: " << request_post;
  http_fetcher_->BeginTransfer(params_.update_url);
}

void OmahaRequestAction::TerminateProcessing() {
  http_fetcher_->TerminateTransfer();
}

// We just store the response in the buffer. Once we've received all bytes,
// we'll look in the buffer and decide what to do.
void OmahaRequestAction::ReceivedBytes(HttpFetcher *fetcher,
                                       const char* bytes,
                                       int length) {
  response_buffer_.reserve(response_buffer_.size() + length);
  response_buffer_.insert(response_buffer_.end(), bytes, bytes + length);
}

namespace {
// If non-NULL response, caller is responsible for calling xmlXPathFreeObject()
// on the returned object.
// This code is roughly based on the libxml tutorial at:
// http://xmlsoft.org/tutorial/apd.html
xmlXPathObject* GetNodeSet(xmlDoc* doc, const xmlChar* xpath,
                           const xmlChar* ns, const xmlChar* ns_url) {
  xmlXPathObject* result = NULL;

  scoped_ptr_malloc<xmlXPathContext, ScopedPtrXmlXPathContextFree> context(
      xmlXPathNewContext(doc));
  if (!context.get()) {
    LOG(ERROR) << "xmlXPathNewContext() returned NULL";
    return NULL;
  }
  if (xmlXPathRegisterNs(context.get(), ns, ns_url) < 0) {
    LOG(ERROR) << "xmlXPathRegisterNs() returned error";
    return NULL;
  }

  result = xmlXPathEvalExpression(xpath, context.get());

  if (result == NULL) {
    LOG(ERROR) << "xmlXPathEvalExpression returned error";
    return NULL;
  }
  if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
    LOG(INFO) << "xpath not found in doc";
    xmlXPathFreeObject(result);
    return NULL;
  }
  return result;
}

// Returns the string value of a named attribute on a node, or empty string
// if no such node exists. If the attribute exists and has a value of
// empty string, there's no way to distinguish that from the attribute
// not existing.
string XmlGetProperty(xmlNode* node, const char* name) {
  if (!xmlHasProp(node, ConstXMLStr(name)))
    return "";
  scoped_ptr_malloc<xmlChar, ScopedPtrXmlFree> str(
      xmlGetProp(node, ConstXMLStr(name)));
  string ret(reinterpret_cast<const char *>(str.get()));
  return ret;
}

// Parses a 64 bit base-10 int from a string and returns it. Returns 0
// on error. If the string contains "0", that's indistinguishable from
// error.
off_t ParseInt(const string& str) {
  off_t ret = 0;
  int rc = sscanf(str.c_str(), "%" PRIi64, &ret);
  if (rc < 1) {
    // failure
    return 0;
  }
  return ret;
}

// Update the last ping day preferences based on the server daystart
// response. Returns true on success, false otherwise.
bool UpdateLastPingDays(xmlDoc* doc, PrefsInterface* prefs) {
  static const char kNamespace[] = "x";
  static const char kDaystartNodeXpath[] = "/x:gupdate/x:daystart";
  static const char kNsUrl[] = "http://www.google.com/update2/response";

  scoped_ptr_malloc<xmlXPathObject, ScopedPtrXmlXPathObjectFree>
      xpath_nodeset(GetNodeSet(doc,
                               ConstXMLStr(kDaystartNodeXpath),
                               ConstXMLStr(kNamespace),
                               ConstXMLStr(kNsUrl)));
  TEST_AND_RETURN_FALSE(xpath_nodeset.get());
  xmlNodeSet* nodeset = xpath_nodeset->nodesetval;
  TEST_AND_RETURN_FALSE(nodeset && nodeset->nodeNr >= 1);
  xmlNode* daystart_node = nodeset->nodeTab[0];
  TEST_AND_RETURN_FALSE(xmlHasProp(daystart_node,
                                   ConstXMLStr("elapsed_seconds")));

  int64_t elapsed_seconds = 0;
  TEST_AND_RETURN_FALSE(base::StringToInt64(XmlGetProperty(daystart_node,
                                                           "elapsed_seconds"),
                                            &elapsed_seconds));
  TEST_AND_RETURN_FALSE(elapsed_seconds >= 0);

  // Remember the local time that matches the server's last midnight
  // time.
  Time daystart = Time::Now() - TimeDelta::FromSeconds(elapsed_seconds);
  prefs->SetInt64(kPrefsLastActivePingDay, daystart.ToInternalValue());
  prefs->SetInt64(kPrefsLastRollCallPingDay, daystart.ToInternalValue());
  return true;
}
}  // namespace {}

// If the transfer was successful, this uses libxml2 to parse the response
// and fill in the appropriate fields of the output object. Also, notifies
// the processor that we're done.
void OmahaRequestAction::TransferComplete(HttpFetcher *fetcher,
                                          bool successful) {
  ScopedActionCompleter completer(processor_, this);
  LOG(INFO) << "Omaha request response: " << string(response_buffer_.begin(),
                                                    response_buffer_.end());

  // Events are best effort transactions -- assume they always succeed.
  if (IsEvent()) {
    CHECK(!HasOutputPipe()) << "No output pipe allowed for event requests.";
    if (event_->result == OmahaEvent::kResultError && successful &&
        utils::IsOfficialBuild()) {
      LOG(INFO) << "Signalling Crash Reporter.";
      utils::ScheduleCrashReporterUpload();
    }
    completer.set_code(kActionCodeSuccess);
    return;
  }

  if (!successful) {
    LOG(ERROR) << "Omaha request network transfer failed.";
    int code = GetHTTPResponseCode();
    // Makes sure we send sane error values.
    if (code < 0 || code >= 1000) {
      code = 999;
    }
    completer.set_code(static_cast<ActionExitCode>(
        kActionCodeOmahaRequestHTTPResponseBase + code));
    return;
  }

  // parse our response and fill the fields in the output object
  scoped_ptr_malloc<xmlDoc, ScopedPtrXmlDocFree> doc(
      xmlParseMemory(&response_buffer_[0], response_buffer_.size()));
  if (!doc.get()) {
    LOG(ERROR) << "Omaha response not valid XML";
    completer.set_code(response_buffer_.empty() ?
                       kActionCodeOmahaRequestEmptyResponseError :
                       kActionCodeOmahaRequestXMLParseError);
    return;
  }

  // If a ping was sent, update the last ping day preferences based on
  // the server daystart response.
  if (ShouldPing(ping_active_days_) ||
      ShouldPing(ping_roll_call_days_) ||
      ping_active_days_ == kPingTimeJump ||
      ping_roll_call_days_ == kPingTimeJump) {
    LOG_IF(ERROR, !UpdateLastPingDays(doc.get(), prefs_))
        << "Failed to update the last ping day preferences!";
  }

  if (!HasOutputPipe()) {
    // Just set success to whether or not the http transfer succeeded,
    // which must be true at this point in the code.
    completer.set_code(kActionCodeSuccess);
    return;
  }

  static const char* kNamespace("x");
  static const char* kUpdatecheckNodeXpath("/x:gupdate/x:app/x:updatecheck");
  static const char* kNsUrl("http://www.google.com/update2/response");

  scoped_ptr_malloc<xmlXPathObject, ScopedPtrXmlXPathObjectFree>
      xpath_nodeset(GetNodeSet(doc.get(),
                               ConstXMLStr(kUpdatecheckNodeXpath),
                               ConstXMLStr(kNamespace),
                               ConstXMLStr(kNsUrl)));
  if (!xpath_nodeset.get()) {
    completer.set_code(kActionCodeOmahaRequestNoUpdateCheckNode);
    return;
  }
  xmlNodeSet* nodeset = xpath_nodeset->nodesetval;
  CHECK(nodeset) << "XPath missing NodeSet";
  CHECK_GE(nodeset->nodeNr, 1);

  xmlNode* updatecheck_node = nodeset->nodeTab[0];
  // get status
  if (!xmlHasProp(updatecheck_node, ConstXMLStr("status"))) {
    LOG(ERROR) << "Response missing status";
    completer.set_code(kActionCodeOmahaRequestNoUpdateCheckStatus);
    return;
  }

  OmahaResponse output_object;
  base::StringToInt(XmlGetProperty(updatecheck_node, "PollInterval"),
                    &output_object.poll_interval);
  const string status(XmlGetProperty(updatecheck_node, "status"));
  if (status == "noupdate") {
    LOG(INFO) << "No update.";
    output_object.update_exists = false;
    SetOutputObject(output_object);
    completer.set_code(kActionCodeSuccess);
    return;
  }

  if (status != "ok") {
    LOG(ERROR) << "Unknown status: " << status;
    completer.set_code(kActionCodeOmahaRequestBadUpdateCheckStatus);
    return;
  }

  // In best-effort fashion, fetch the rest of the expected attributes
  // from the updatecheck node, then return the object
  output_object.update_exists = true;
  completer.set_code(kActionCodeSuccess);

  output_object.display_version =
      XmlGetProperty(updatecheck_node, "DisplayVersion");
  output_object.codebase = XmlGetProperty(updatecheck_node, "codebase");
  output_object.more_info_url = XmlGetProperty(updatecheck_node, "MoreInfo");
  output_object.hash = XmlGetProperty(updatecheck_node, "sha256");
  output_object.size = ParseInt(XmlGetProperty(updatecheck_node, "size"));
  output_object.needs_admin =
      XmlGetProperty(updatecheck_node, "needsadmin") == "true";
  output_object.prompt = XmlGetProperty(updatecheck_node, "Prompt") == "true";
  output_object.is_delta =
      XmlGetProperty(updatecheck_node, "IsDelta") == "true";
  output_object.deadline = XmlGetProperty(updatecheck_node, "deadline");
  SetOutputObject(output_object);
}

};  // namespace chromeos_update_engine
