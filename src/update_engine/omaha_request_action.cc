// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_action.h"

#include <inttypes.h>

#include <memory>
#include <sstream>
#include <string>

#include <glog/logging.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "strings/string_number_conversions.h"
#include "strings/string_printf.h"
#include "update_engine/action_pipe.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/payload_state_interface.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/utils.h"

using std::string;
using strings::StringPrintf;

namespace chromeos_update_engine {

// List of custom pair tags that we interpret in the Omaha Response:
static const char* kTagDeadline = "deadline";
static const char* kTagDisablePayloadBackoff = "DisablePayloadBackoff";
static const char* kTagDisplayVersion = "DisplayVersion";
// Deprecated: "IsDelta"
static const char* kTagIsDeltaPayload = "IsDeltaPayload";
static const char* kTagMaxFailureCountPerUrl = "MaxFailureCountPerUrl";
// Deprecated: "MaxDaysToScatter";
// Deprecated: "ManifestSignatureRsa"
// Deprecated: "ManifestSize"
// Deprecated: "MetadataSignatureRsa"
// Deprecated: "MetadataSize"
static const char* kTagMoreInfo = "MoreInfo";
static const char* kTagNeedsAdmin = "needsadmin";
static const char* kTagPrompt = "Prompt";
static const char* kTagSha256 = "sha256";

namespace {

const string kGupdateVersion("CoreOSUpdateEngine-0.1.0.0");

// This is handy for passing strings into libxml2
#define ConstXMLStr(x) (reinterpret_cast<const xmlChar*>(x))

// These are for std::unique_ptr
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

// Returns an XML ping element if any of the elapsed days need to be
// sent, or an empty string otherwise.
string GetPingXml(int ping_active_days, int ping_roll_call_days) {
  return StringPrintf("        <ping active=\"1\"></ping>\n");
}

// Returns an XML that goes into the body of the <app> element of the Omaha
// request based on the given parameters.
string GetAppBody(const OmahaEvent* event,
                  const OmahaRequestParams& params,
                  bool ping_only,
                  int ping_active_days,
                  int ping_roll_call_days,
                  PrefsInterface* prefs) {
  string app_body;
  if (event == NULL) {
    app_body = GetPingXml(ping_active_days, ping_roll_call_days);
    if (!ping_only) {
      app_body += "        <updatecheck></updatecheck>\n";

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

      app_body += StringPrintf(
          "        <event eventtype=\"%d\" eventresult=\"%d\" "
          "previousversion=\"%s\"></event>\n",
          OmahaEvent::kTypeUpdateComplete,
          OmahaEvent::kResultSuccessReboot,
          XmlEncode(prev_version).c_str());
      LOG_IF(WARNING, !prefs->SetString(kPrefsPreviousVersion, ""))
          << "Unable to reset the previous version.";
    }
  } else {
    // The error code is an optional attribute so append it only if the result
    // is not success.
    string error_code;
    if (event->result != OmahaEvent::kResultSuccess) {
      error_code = StringPrintf(" errorcode=\"%d\"", event->error_code);
    }
    app_body = StringPrintf(
        "        <event eventtype=\"%d\" eventresult=\"%d\"%s></event>\n",
        event->type, event->result, error_code.c_str());
  }

  return app_body;
}

// Returns an XML that corresponds to the entire <app> node of the Omaha
// request based on the given parameters.
string GetAppXml(const OmahaEvent* event,
                 const OmahaRequestParams& params,
                 bool ping_only,
                 int ping_active_days,
                 int ping_roll_call_days,
                 SystemState* system_state) {
  string app_body = GetAppBody(event, params, ping_only, ping_active_days,
                               ping_roll_call_days, system_state->prefs());

  string delta_okay_str = params.delta_okay() ? "true" : "false";

  string app_xml =
      "    <app appid=\"" + XmlEncode(params.app_id()) + "\" " +
                "version=\"" + XmlEncode(params.app_version()) + "\" " +
                "track=\"" + XmlEncode(params.app_channel()) + "\" " +
                "bootid=\"" + XmlEncode(params.bootid()) + "\" " +
                "oem=\"" + XmlEncode(params.oemid()) + "\" " +
                "oemversion=\"" + XmlEncode(params.oemversion()) + "\" " +
                "alephversion=\"" + XmlEncode(params.alephversion()) + "\" " +
                "machineid=\"" + XmlEncode(params.machineid()) + "\" " +
                "lang=\"" + XmlEncode(params.app_lang()) + "\" " +
                "board=\"" + XmlEncode(params.os_board()) + "\" " +
                "hardware_class=\"" + XmlEncode(params.hwid()) + "\" " +
                "delta_okay=\"" + delta_okay_str + "\" "
                ">\n" +
                   app_body +
      "    </app>\n";

  return app_xml;
}

// Returns an XML that corresponds to the entire <os> node of the Omaha
// request based on the given parameters.
string GetOsXml(const OmahaRequestParams& params) {
  string os_xml =
      "    <os version=\"" + XmlEncode(params.os_version()) + "\" " +
               "platform=\"" + XmlEncode(params.os_platform()) + "\" " +
               "sp=\"" + XmlEncode(params.os_sp()) + "\">"
          "</os>\n";
  return os_xml;
}

// Returns an XML that corresponds to the entire Omaha request based on the
// given parameters.
string GetRequestXml(const OmahaEvent* event,
                     const OmahaRequestParams& params,
                     bool ping_only,
                     int ping_active_days,
                     int ping_roll_call_days,
                     SystemState* system_state) {
  string os_xml = GetOsXml(params);
  string app_xml = GetAppXml(event, params, ping_only, ping_active_days,
                             ping_roll_call_days, system_state);

  string install_source = StringPrintf("installsource=\"%s\" ",
      (params.interactive() ? "ondemandupdate" : "scheduler"));

  string request_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<request protocol=\"3.0\" "
                "version=\"" + XmlEncode(kGupdateVersion) + "\" "
                "updaterversion=\"" + XmlEncode(kGupdateVersion) + "\" " +
                install_source +
                "ismachine=\"1\">\n" +
                  os_xml +
                  app_xml +
      "</request>\n";

  return request_xml;
}

}  // namespace {}

// Encodes XML entities in a given string with libxml2. input must be
// UTF-8 formatted. Output will be UTF-8 formatted.
string XmlEncode(const string& input) {
  std::unique_ptr<xmlChar, ScopedPtrXmlFree> str(
      xmlEncodeEntitiesReentrant(NULL, ConstXMLStr(input.c_str())));
  return string(reinterpret_cast<const char *>(str.get()));
}

OmahaRequestAction::OmahaRequestAction(SystemState* system_state,
                                       OmahaEvent* event,
                                       HttpFetcher* http_fetcher,
                                       bool ping_only)
    : system_state_(system_state),
      event_(event),
      http_fetcher_(http_fetcher),
      ping_only_(ping_only),
      ping_active_days_(0),
      ping_roll_call_days_(0) {
  params_ = system_state->request_params();
}

OmahaRequestAction::~OmahaRequestAction() {}

void OmahaRequestAction::PerformAction() {
  http_fetcher_->set_delegate(this);
  string request_post(GetRequestXml(event_.get(),
                                    *params_,
                                    ping_only_,
                                    ping_active_days_,
                                    ping_roll_call_days_,
                                    system_state_));

  http_fetcher_->SetPostData(request_post.data(), request_post.size(),
                             kHttpContentTypeTextXml);
  LOG(INFO) << "Posting an Omaha request to " << params_->update_url();
  LOG(INFO) << "Request: " << request_post;
  http_fetcher_->BeginTransfer(params_->update_url());
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
xmlXPathObject* GetNodeSet(xmlDoc* doc, const xmlChar* xpath) {
  xmlXPathObject* result = NULL;

  std::unique_ptr<xmlXPathContext, ScopedPtrXmlXPathContextFree> context(
      xmlXPathNewContext(doc));
  if (!context.get()) {
    LOG(ERROR) << "xmlXPathNewContext() returned NULL";
    return NULL;
  }

  result = xmlXPathEvalExpression(xpath, context.get());
  if (result == NULL) {
    LOG(ERROR) << "Unable to find " << xpath << " in XML document";
    return NULL;
  }
  if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
    LOG(INFO) << "Nodeset is empty for " << xpath;
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
  std::unique_ptr<xmlChar, ScopedPtrXmlFree> str(
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

}  // namespace {}

bool OmahaRequestAction::ParseResponse(xmlDoc* doc,
                                       OmahaResponse* output_object,
                                       ScopedActionCompleter* completer) {
  static const char* kUpdatecheckNodeXpath("/response/app/updatecheck");

  std::unique_ptr<xmlXPathObject, ScopedPtrXmlXPathObjectFree>
      xpath_nodeset(GetNodeSet(doc, ConstXMLStr(kUpdatecheckNodeXpath)));
  if (!xpath_nodeset.get()) {
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  xmlNodeSet* nodeset = xpath_nodeset->nodesetval;
  CHECK(nodeset) << "XPath missing UpdateCheck NodeSet";
  CHECK_GE(nodeset->nodeNr, 1);
  xmlNode* update_check_node = nodeset->nodeTab[0];

  // chromium-os:37289: The PollInterval is not supported by Omaha server
  // currently.  But still keeping this existing code in case we ever decide to
  // slow down the request rate from the server-side. Note that the
  // PollInterval is not persisted, so it has to be sent by the server on every
  // response to guarantee that the UpdateCheckScheduler uses this value
  // (otherwise, if the device got rebooted after the last server-indicated
  // value, it'll revert to the default value).
  // Note: The parsing for PollInterval happens even before parsing
  // of the status because we may want to specify the PollInterval even when
  // there's no update.
  if (!strings::StringToInt(XmlGetProperty(update_check_node, "PollInterval"),
                            &output_object->poll_interval))
    output_object->poll_interval = 0;

  if (!ParseStatus(update_check_node, output_object, completer))
    return false;

  // Note: ParseUrls MUST be called before ParsePackage as ParsePackage
  // appends the package name to the URLs populated in this method.
  if (!ParseUrls(doc, output_object, completer))
    return false;

  if (!ParsePackage(doc, output_object, completer))
    return false;

  if (!ParseParams(doc, output_object, completer))
    return false;

  output_object->update_exists = true;
  SetOutputObject(*output_object);
  completer->set_code(kActionCodeSuccess);

  return true;
}

bool OmahaRequestAction::ParseStatus(xmlNode* update_check_node,
                                     OmahaResponse* output_object,
                                     ScopedActionCompleter* completer) {
  // Get status.
  if (!xmlHasProp(update_check_node, ConstXMLStr("status"))) {
    LOG(ERROR) << "Omaha Response missing status";
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  const string status(XmlGetProperty(update_check_node, "status"));
  if (status == "noupdate") {
    LOG(INFO) << "No update.";
    output_object->update_exists = false;
    SetOutputObject(*output_object);
    completer->set_code(kActionCodeSuccess);
    return false;
  }

  if (status != "ok") {
    LOG(ERROR) << "Unknown Omaha response status: " << status;
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  return true;
}

bool OmahaRequestAction::ParseUrls(xmlDoc* doc,
                                   OmahaResponse* output_object,
                                   ScopedActionCompleter* completer) {
  // Get the update URL.
  static const char* kUpdateUrlNodeXPath("/response/app/updatecheck/urls/url");

  std::unique_ptr<xmlXPathObject, ScopedPtrXmlXPathObjectFree>
      xpath_nodeset(GetNodeSet(doc, ConstXMLStr(kUpdateUrlNodeXPath)));
  if (!xpath_nodeset.get()) {
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  xmlNodeSet* nodeset = xpath_nodeset->nodesetval;
  CHECK(nodeset) << "XPath missing " << kUpdateUrlNodeXPath;
  CHECK_GE(nodeset->nodeNr, 1);

  LOG(INFO) << "Found " << nodeset->nodeNr << " url(s)";
  output_object->payload_urls.clear();
  for (int i = 0; i < nodeset->nodeNr; i++) {
    xmlNode* url_node = nodeset->nodeTab[i];

    const string codebase(XmlGetProperty(url_node, "codebase"));
    if (codebase.empty()) {
      LOG(ERROR) << "Omaha Response URL has empty codebase";
      completer->set_code(kActionCodeOmahaResponseInvalid);
      return false;
    }
    output_object->payload_urls.push_back(codebase);
  }

  return true;
}

bool OmahaRequestAction::ParsePackage(xmlDoc* doc,
                                      OmahaResponse* output_object,
                                      ScopedActionCompleter* completer) {
  // Get the package node.
  static const char* kPackageNodeXPath(
      "/response/app/updatecheck/manifest/packages/package");

  std::unique_ptr<xmlXPathObject, ScopedPtrXmlXPathObjectFree>
      xpath_nodeset(GetNodeSet(doc, ConstXMLStr(kPackageNodeXPath)));
  if (!xpath_nodeset.get()) {
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  xmlNodeSet* nodeset = xpath_nodeset->nodesetval;
  CHECK(nodeset) << "XPath missing " << kPackageNodeXPath;
  CHECK_GE(nodeset->nodeNr, 1);

  // We only care about the first package.
  LOG(INFO) << "Processing first of " << nodeset->nodeNr << " package(s)";
  xmlNode* package_node = nodeset->nodeTab[0];

  // Get package properties one by one.

  // Parse the payload name to be appended to the base Url value.
  const string package_name(XmlGetProperty(package_node, "name"));
  LOG(INFO) << "Omaha Response package name = " << package_name;
  if (package_name.empty()) {
    LOG(ERROR) << "Omaha Response has empty package name";
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  // Append the package name to each URL in our list so that we don't
  // propagate the urlBase vs packageName distinctions beyond this point.
  // From now on, we only need to use payload_urls.
  for (size_t i = 0; i < output_object->payload_urls.size(); i++) {
    output_object->payload_urls[i] += package_name;
    LOG(INFO) << "Url" << i << ": " << output_object->payload_urls[i];
  }

  // Parse the payload size.
  off_t size = ParseInt(XmlGetProperty(package_node, "size"));
  if (size <= 0) {
    LOG(ERROR) << "Omaha Response has invalid payload size: " << size;
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }
  output_object->size = size;

  LOG(INFO) << "Payload size = " << output_object->size << " bytes";

  return true;
}

bool OmahaRequestAction::ParseParams(xmlDoc* doc,
                                     OmahaResponse* output_object,
                                     ScopedActionCompleter* completer) {
  // Get the action node where parameters are present.
  static const char* kActionNodeXPath(
      "/response/app/updatecheck/manifest/actions/action");

  std::unique_ptr<xmlXPathObject, ScopedPtrXmlXPathObjectFree>
      xpath_nodeset(GetNodeSet(doc, ConstXMLStr(kActionNodeXPath)));
  if (!xpath_nodeset.get()) {
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  xmlNodeSet* nodeset = xpath_nodeset->nodesetval;
  CHECK(nodeset) << "XPath missing " << kActionNodeXPath;

  // We only care about the action that has event "postinall", because this is
  // where Omaha puts all the generic name/value pairs in the rule.
  LOG(INFO) << "Found " << nodeset->nodeNr
            << " action(s). Processing the postinstall action.";

  // pie_action_node holds the action node corresponding to the
  // postinstall event action, if present.
  xmlNode* pie_action_node = NULL;
  for (int i = 0; i < nodeset->nodeNr; i++) {
    xmlNode* action_node = nodeset->nodeTab[i];
    if (XmlGetProperty(action_node, "event") == "postinstall") {
      pie_action_node = action_node;
      break;
    }
  }

  if (!pie_action_node) {
    LOG(ERROR) << "Omaha Response has no postinstall event action";
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  output_object->hash = XmlGetProperty(pie_action_node, kTagSha256);
  if (output_object->hash.empty()) {
    LOG(ERROR) << "Omaha Response has empty sha256 value";
    completer->set_code(kActionCodeOmahaResponseInvalid);
    return false;
  }

  // Get the optional properties one by one.
  output_object->display_version =
      XmlGetProperty(pie_action_node, kTagDisplayVersion);
  output_object->more_info_url = XmlGetProperty(pie_action_node, kTagMoreInfo);
  output_object->needs_admin =
      XmlGetProperty(pie_action_node, kTagNeedsAdmin) == "true";
  output_object->prompt = XmlGetProperty(pie_action_node, kTagPrompt) == "true";
  output_object->deadline = XmlGetProperty(pie_action_node, kTagDeadline);

  string max = XmlGetProperty(pie_action_node, kTagMaxFailureCountPerUrl);
  if (!strings::StringToUint(max, &output_object->max_failure_count_per_url))
    output_object->max_failure_count_per_url = kDefaultMaxFailureCountPerUrl;

  output_object->is_delta_payload =
      XmlGetProperty(pie_action_node, kTagIsDeltaPayload) == "true";

  output_object->disable_payload_backoff =
      XmlGetProperty(pie_action_node, kTagDisablePayloadBackoff) == "true";

  return true;
}

// If the transfer was successful, this uses libxml2 to parse the response
// and fill in the appropriate fields of the output object. Also, notifies
// the processor that we're done.
void OmahaRequestAction::TransferComplete(HttpFetcher *fetcher,
                                          bool successful) {
  ScopedActionCompleter completer(processor_, this);
  string current_response(response_buffer_.begin(), response_buffer_.end());
  LOG(INFO) << "Omaha request response: " << current_response;

  // Events are best effort transactions -- assume they always succeed.
  if (IsEvent()) {
    CHECK(!HasOutputPipe()) << "No output pipe allowed for event requests.";
    if (event_->result == OmahaEvent::kResultError && successful &&
        utils::IsOfficialBuild()) {
      LOG(ERROR) << "HTTP reported success but Omaha reports an error.";
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
  std::unique_ptr<xmlDoc, ScopedPtrXmlDocFree> doc(
      xmlParseMemory(&response_buffer_[0], response_buffer_.size()));
  if (!doc.get()) {
    LOG(ERROR) << "Omaha response not valid XML";
    completer.set_code(response_buffer_.empty() ?
                       kActionCodeOmahaRequestEmptyResponseError :
                       kActionCodeOmahaRequestXMLParseError);
    return;
  }

  if (!HasOutputPipe()) {
    // Just set success to whether or not the http transfer succeeded,
    // which must be true at this point in the code.
    completer.set_code(kActionCodeSuccess);
    return;
  }

  OmahaResponse output_object;
  if (!ParseResponse(doc.get(), &output_object, &completer))
    return;

  // Update the payload state with the current response. The payload state
  // will automatically reset all stale state if this response is different
  // from what's stored already. We are updating the payload state as late
  // as possible in this method so that if a new release gets pushed and then
  // got pulled back due to some issues, we don't want to clear our internal
  // state unnecessarily.
  PayloadStateInterface* payload_state = system_state_->payload_state();
  payload_state->SetResponse(output_object);

  if (payload_state->ShouldBackoffDownload()) {
    output_object.update_exists = false;
    LOG(INFO) << "Ignoring Omaha updates in order to backoff our retry "
                 "attempts";
    completer.set_code(kActionCodeOmahaUpdateDeferredForBackoff);
    return;
  }
}

}  // namespace chromeos_update_engine
