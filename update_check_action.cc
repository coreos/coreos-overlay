// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_check_action.h"
#include <sstream>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "chromeos/obsolete_logging.h"
#include "update_engine/action_pipe.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

const char* const UpdateCheckParams::kAppId(
    "{87efface-864d-49a5-9bb3-4b050a7c227a}");
const char* const UpdateCheckParams::kOsPlatform("Chrome OS");
const char* const UpdateCheckParams::kOsVersion("Indy");

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

// Returns a properly formatted omaha request for an update check
string FormatRequest(const UpdateCheckParams& params) {
  return string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" "
      "version=\"" + XmlEncode(kGupdateVersion) + "\" protocol=\"2.0\" "
      "machineid=\"") + XmlEncode(params.machine_id) + "\" ismachine=\"1\" "
      "userid=\"" + XmlEncode(params.user_id) + "\">\n"
      "    <o:os version=\"" + XmlEncode(params.os_version) + "\" platform=\"" +
      XmlEncode(params.os_platform) + "\" sp=\"" +
      XmlEncode(params.os_sp) + "\"></o:os>\n"
      "    <o:app appid=\"" + XmlEncode(params.app_id) + "\" version=\"" +
      XmlEncode(params.app_version) + "\" "
      "lang=\"" + XmlEncode(params.app_lang) + "\" track=\"" +
      XmlEncode(params.app_track) + "\">\n"
      "        <o:ping active=\"0\"></o:ping>\n"
      "        <o:updatecheck></o:updatecheck>\n"
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

UpdateCheckAction::UpdateCheckAction(HttpFetcher* http_fetcher)
    : http_fetcher_(http_fetcher) {}

UpdateCheckAction::~UpdateCheckAction() {}

void UpdateCheckAction::PerformAction() {
  CHECK(HasInputObject());
  params_ = GetInputObject();
  http_fetcher_->set_delegate(this);
  string request_post(FormatRequest(params_));
  http_fetcher_->SetPostData(request_post.data(), request_post.size());
  http_fetcher_->BeginTransfer("https://tools.google.com/service/update2");
}

void UpdateCheckAction::TerminateProcessing() {
  http_fetcher_->TerminateTransfer();
}

// We just store the response in the buffer. Once we've received all bytes,
// we'll look in the buffer and decide what to do.
void UpdateCheckAction::ReceivedBytes(HttpFetcher *fetcher,
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

  int rc = sscanf(str.c_str(), "%lld", &ret);
  if (rc < 1) {
    // failure
    return 0;
  }
  return ret;
}
}  // namespace {}

// If the transfer was successful, this uses libxml2 to parse the response
// and fill in the appropriate fields of the output object. Also, notifies
// the processor that we're done.
void UpdateCheckAction::TransferComplete(HttpFetcher *fetcher,
                                         bool successful) {
  ScopedActionCompleter completer(processor_, this);
  if (!successful)
    return;
  if (!HasOutputPipe()) {
    // Just set success to whether or not the http transfer succeeded,
    // which must be true at this point in the code.
    completer.set_success(true);
    return;
  }

  // parse our response and fill the fields in the output object
  scoped_ptr_malloc<xmlDoc, ScopedPtrXmlDocFree> doc(
      xmlParseMemory(&response_buffer_[0], response_buffer_.size()));
  if (!doc.get()) {
    LOG(ERROR) << "Omaha response not valid XML";
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
    return;
  }
  xmlNodeSet* nodeset = xpath_nodeset->nodesetval;
  CHECK(nodeset) << "XPath missing NodeSet";
  CHECK_GE(nodeset->nodeNr, 1);

  xmlNode* updatecheck_node = nodeset->nodeTab[0];
  // get status
  if (!xmlHasProp(updatecheck_node, ConstXMLStr("status"))) {
    LOG(ERROR) << "Response missing status";
    return;
  }

  const string status(XmlGetProperty(updatecheck_node, "status"));
  UpdateCheckResponse output_object;
  if (status == "noupdate") {
    LOG(INFO) << "No update.";
    output_object.update_exists = false;
    SetOutputObject(output_object);
    completer.set_success(true);
    return;
  }

  if (status != "ok") {
    LOG(ERROR) << "Unknown status: " << status;
    return;
  }

  // In best-effort fashion, fetch the rest of the expected attributes
  // from the updatecheck node, then return the object
  output_object.update_exists = true;
  completer.set_success(true);

  output_object.display_version =
      XmlGetProperty(updatecheck_node, "DisplayVersion");
  output_object.codebase = XmlGetProperty(updatecheck_node, "codebase");
  output_object.more_info_url = XmlGetProperty(updatecheck_node, "MoreInfo");
  output_object.hash = XmlGetProperty(updatecheck_node, "hash");
  output_object.size = ParseInt(XmlGetProperty(updatecheck_node, "size"));
  output_object.needs_admin =
      XmlGetProperty(updatecheck_node, "needsadmin") == "true";
  output_object.prompt = XmlGetProperty(updatecheck_node, "Prompt") == "true";
  SetOutputObject(output_object);
  return;
}

};  // namespace chromeos_update_engine
