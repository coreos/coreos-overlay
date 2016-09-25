// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains general definitions used in implementing, testing and
// emulating communication over HTTP.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_HTTP_COMMON_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_HTTP_COMMON_H__

#include <stdlib.h>

// Enumeration type for HTTP response codes.
enum HttpResponseCode {
  kHttpResponseUndefined           = 0,
  kHttpResponseOk                  = 200,
  kHttpResponseCreated             = 201,
  kHttpResponseAccepted            = 202,
  kHttpResponseNonAuthInfo         = 203,
  kHttpResponseNoContent           = 204,
  kHttpResponseResetContent        = 205,
  kHttpResponsePartialContent      = 206,
  kHttpResponseMultipleChoices     = 300,
  kHttpResponseMovedPermanently    = 301,
  kHttpResponseFound               = 302,
  kHttpResponseSeeOther            = 303,
  kHttpResponseNotModified         = 304,
  kHttpResponseUseProxy            = 305,
  kHttpResponseTempRedirect        = 307,
  kHttpResponseBadRequest          = 400,
  kHttpResponseUnauth              = 401,
  kHttpResponseForbidden           = 403,
  kHttpResponseNotFound            = 404,
  kHttpResponseRequestTimeout      = 408,
  kHttpResponseReqRangeNotSat      = 416,
  kHttpResponseInternalServerError = 500,
  kHttpResponseNotImplemented      = 501,
  kHttpResponseServiceUnavailable  = 503,
  kHttpResponseVersionNotSupported = 505,
};

// Returns a standard HTTP status line string for a given response code.
const char *GetHttpResponseDescription(HttpResponseCode code);

// Converts a string beginning with an HTTP error code into numerical value.
HttpResponseCode StringToHttpResponseCode(const char *s);


// Enumeration type for HTTP Content-Type.
enum HttpContentType {
  kHttpContentTypeUnspecified = 0,
  kHttpContentTypeTextXml,
  kHttpContentTypeOctetStream,
};

// Returns a standard HTTP Content-Type string.
const char *GetHttpContentTypeString(HttpContentType type);

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_HTTP_COMMON_H__
