// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of common HTTP related functions.

#include "macros.h"
#include "update_engine/http_common.h"


const char *GetHttpResponseDescription(HttpResponseCode code) {
  static const struct {
    HttpResponseCode code;
    const char* description;
  } http_response_table[] = {
    { kHttpResponseOk,                  "OK" },
    { kHttpResponseCreated,             "Created" },
    { kHttpResponseAccepted,            "Accepted" },
    { kHttpResponseNonAuthInfo,         "Non-Authoritative Information" },
    { kHttpResponseNoContent,           "No Content" },
    { kHttpResponseResetContent,        "Reset Content" },
    { kHttpResponsePartialContent,      "Partial Content" },
    { kHttpResponseMultipleChoices,     "Multiple Choices" },
    { kHttpResponseMovedPermanently,    "Moved Permanently" },
    { kHttpResponseFound,               "Found" },
    { kHttpResponseSeeOther,            "See Other" },
    { kHttpResponseNotModified,         "Not Modified" },
    { kHttpResponseUseProxy,            "Use Proxy" },
    { kHttpResponseTempRedirect,        "Temporary Redirect" },
    { kHttpResponseBadRequest,          "Bad Request" },
    { kHttpResponseUnauth,              "Unauthorized" },
    { kHttpResponseForbidden,           "Forbidden" },
    { kHttpResponseNotFound,            "Not Found" },
    { kHttpResponseRequestTimeout,      "Request Timeout" },
    { kHttpResponseInternalServerError, "Internal Server Error" },
    { kHttpResponseNotImplemented,      "Not Implemented" },
    { kHttpResponseServiceUnavailable,  "Service Unavailable" },
    { kHttpResponseVersionNotSupported, "HTTP Version Not Supported" },
  };

  bool is_found = false;
  size_t i;
  for (i = 0; i < arraysize(http_response_table); i++)
    if ((is_found = (http_response_table[i].code == code)))
      break;

  return (is_found ? http_response_table[i].description : "(unsupported)");
}

HttpResponseCode StringToHttpResponseCode(const char *s) {
  return static_cast<HttpResponseCode>(strtoul(s, NULL, 10));
}


const char *GetHttpContentTypeString(HttpContentType type) {
  static const struct {
    HttpContentType type;
    const char* str;
  } http_content_type_table[] = {
    { kHttpContentTypeTextXml, "text/xml" },
    { kHttpContentTypeOctetStream, "application/octet-stream" },
  };

  bool is_found = false;
  size_t i;
  for (i = 0; i < arraysize(http_content_type_table); i++)
    if ((is_found = (http_content_type_table[i].type == type)))
      break;

  return (is_found ? http_content_type_table[i].str : NULL);
}
