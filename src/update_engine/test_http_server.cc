// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a simple HTTP server. It can exhibit odd behavior
// that's useful for testing. For example, it's useful to test that
// the updater can continue a connection if it's dropped, or that it
// handles very slow data transfers.

// To use this, simply make an HTTP connection to localhost:port and
// GET a url.

#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <glog/logging.h>

#include "strings/string_printf.h"
#include "strings/string_split.h"
#include "update_engine/http_common.h"
#include "update_engine/http_fetcher_unittest.h"


// HTTP end-of-line delimiter; sorry, this needs to be a macro.
#define EOL "\r\n"

using std::min;
using std::string;
using std::vector;
using strings::StringPrintf;


namespace chromeos_update_engine {

namespace {

bool StartsWith(const string& str, const string& search) {
  return str.compare(0, search.length(), search) == 0;
}

bool EndsWith(const string& str, const string& search) {
  size_t str_length = str.length();
  size_t search_length = search.length();
  if (search_length > str_length)
    return false;
  return str.compare(str_length - search_length, search_length, search) == 0;
}

}  // namespace

struct HttpRequest {
  HttpRequest()
      : start_offset(0), end_offset(0), return_code(kHttpResponseOk) {}
  string host;
  string url;
  off_t start_offset;
  off_t end_offset;  // non-inclusive, zero indicates unspecified.
  HttpResponseCode return_code;
};

bool ParseRequest(int fd, HttpRequest* request) {
  string headers;
  do {
    char buf[1024];
    ssize_t r = read(fd, buf, sizeof(buf));
    if (r < 0) {
      perror("read");
      exit(1);
    }
    headers.append(buf, r);
  } while (!EndsWith(headers, EOL EOL));

  LOG(INFO) << "got headers:\n--8<------8<------8<------8<----\n"
            << headers
            << "\n--8<------8<------8<------8<----";

  // Break header into lines.
  std::vector<string> lines = strings::SplitAndTrim(
      headers.substr(0, headers.length() - strlen(EOL EOL)), EOL);

  // Decode URL line.
  std::vector<string> terms = strings::SplitWords(lines[0]);
  CHECK_EQ(terms.size(), static_cast<vector<string>::size_type>(3));
  CHECK_EQ(terms[0], "GET");
  request->url = terms[1];
  LOG(INFO) << "URL: " << request->url;

  // Decode remaining lines.
  size_t i;
  for (i = 1; i < lines.size(); i++) {
    std::vector<string> terms = strings::SplitWords(lines[i]);

    if (terms[0] == "Range:") {
      CHECK_EQ(terms.size(), static_cast<vector<string>::size_type>(2));
      string &range = terms[1];
      LOG(INFO) << "range attribute: " << range;
      CHECK(StartsWith(range, "bytes=") && range.find('-') != string::npos);
      request->start_offset = atoll(range.c_str() + strlen("bytes="));
      // Decode end offset and increment it by one (so it is non-inclusive).
      if (range.find('-') < range.length() - 1)
        request->end_offset = atoll(range.c_str() + range.find('-') + 1) + 1;
      request->return_code = kHttpResponsePartialContent;
      std::string tmp_str = StringPrintf("decoded range offsets: start=%jd "
                                         "end=", request->start_offset);
      if (request->end_offset > 0)
        tmp_str += StringPrintf("%jd (non-inclusive)", request->end_offset);
      else
        tmp_str += "unspecified";
      LOG(INFO) << tmp_str;
    } else if (terms[0] == "Host:") {
      CHECK_EQ(terms.size(), static_cast<vector<string>::size_type>(2));
      request->host = terms[1];
      LOG(INFO) << "host attribute: " << request->host;
    } else {
      LOG(WARNING) << "ignoring HTTP attribute: `" << lines[i] << "'";
    }
  }

  return true;
}

string Itoa(off_t num) {
  char buf[100] = {0};
  snprintf(buf, sizeof(buf), "%" PRIi64, num);
  return buf;
}

// Writes a string into a file. Returns total number of bytes written or -1 if a
// write error occurred.
ssize_t WriteString(int fd, const string& str) {
  const size_t total_size = str.size();
  size_t remaining_size = total_size;
  char const *data = str.data();

  while (remaining_size) {
    ssize_t written = write(fd, data, remaining_size);
    if (written < 0) {
      perror("write");
      LOG(INFO) << "write failed";
      return -1;
    }
    data += written;
    remaining_size -= written;
  }

  return total_size;
}

// Writes the headers of an HTTP response into a file.
ssize_t WriteHeaders(int fd, const off_t start_offset, const off_t end_offset,
                     HttpResponseCode return_code) {
  ssize_t written = 0, ret;

  ret = WriteString(fd,
                    string("HTTP/1.1 ") + Itoa(return_code) + " " +
                    GetHttpResponseDescription(return_code) +
                    EOL
                    "Content-Type: application/octet-stream" EOL);
  if (ret < 0)
    return -1;
  written += ret;

  // Compute content legnth.
  const off_t content_length = end_offset - start_offset;;

  // A start offset that equals the end offset indicates that the response
  // should contain the full range of bytes in the requested resource.
  if (start_offset || start_offset == end_offset) {
    ret = WriteString(fd,
                      string("Accept-Ranges: bytes" EOL
                             "Content-Range: bytes ") +
                      Itoa(start_offset == end_offset ? 0 : start_offset) +
                      "-" + Itoa(end_offset - 1) + "/" + Itoa(end_offset) +
                      EOL);
    if (ret < 0)
      return -1;
    written += ret;
  }

  ret = WriteString(fd, string("Content-Length: ") + Itoa(content_length) +
                    EOL EOL);
  if (ret < 0)
    return -1;
  written += ret;

  return written;
}

// Writes a predetermined payload of lines of ascending bytes to a file. The
// first byte of output is appropriately offset with respect to the request line
// length.  Returns the number of successfully written bytes.
size_t WritePayload(int fd, const off_t start_offset, const off_t end_offset,
                    const char first_byte, const size_t line_len) {
  CHECK_LE(start_offset, end_offset);
  CHECK_GT(line_len, static_cast<size_t>(0));

  LOG(INFO) << "writing payload: " << line_len << "-byte lines starting with `"
            << first_byte << "', offset range " << start_offset << " -> "
            << end_offset;

  // Populate line of ascending characters.
  string line;
  line.reserve(line_len);
  char byte = first_byte;
  size_t i;
  for (i = 0; i < line_len; i++)
    line += byte++;

  const size_t total_len = end_offset - start_offset;
  size_t remaining_len = total_len;
  bool success = true;

  // If start offset is not aligned with line boundary, output partial line up
  // to the first line boundary.
  size_t start_modulo = start_offset % line_len;
  if (start_modulo) {
    string partial = line.substr(start_modulo, remaining_len);
    ssize_t ret = WriteString(fd, partial);
    if ((success = (ret >= 0 && (size_t) ret == partial.length())))
      remaining_len -= partial.length();
  }

  // Output full lines up to the maximal line boundary below the end offset.
  while (success && remaining_len >= line_len) {
    ssize_t ret = WriteString(fd, line);
    if ((success = (ret >= 0 && (size_t) ret == line_len)))
      remaining_len -= line_len;
  }

  // Output a partial line up to the end offset.
  if (success && remaining_len) {
    string partial = line.substr(0, remaining_len);
    ssize_t ret = WriteString(fd, partial);
    if ((success = (ret >= 0 && (size_t) ret == partial.length())))
      remaining_len -= partial.length();
  }

  return (total_len - remaining_len);
}

// Write default payload lines of the form 'abcdefghij'.
inline size_t WritePayload(int fd, const off_t start_offset,
                           const off_t end_offset) {
  return WritePayload(fd, start_offset, end_offset, 'a', 10);
}

// Send an empty response, then kill the server.
void HandleQuit(int fd) {
  WriteHeaders(fd, 0, 0, kHttpResponseOk);
  LOG(INFO) << "pid(" << getpid() <<  "): HTTP server exiting ...";
  exit(0);
}


// Generates an HTTP response with payload corresponding to requested offsets
// and length.  Optionally, truncate the payload at a given length and add a
// pause midway through the transfer.  Returns the total number of bytes
// delivered or -1 for error.
ssize_t HandleGet(int fd, const HttpRequest& request, const size_t total_length,
                  const size_t truncate_length, const int sleep_every,
                  const int sleep_secs) {
  ssize_t ret;
  size_t written = 0;

  // Obtain start offset, make sure it is within total payload length.
  const size_t start_offset = request.start_offset;
  if (start_offset >= total_length) {
    LOG(WARNING) << "start offset (" << start_offset
                 << ") exceeds total length (" << total_length
                 << "), generating error response ("
                 << kHttpResponseReqRangeNotSat << ")";
    return WriteHeaders(fd, total_length, total_length,
                        kHttpResponseReqRangeNotSat);
  }

  // Obtain end offset, adjust to fit in total payload length and ensure it does
  // not preceded the start offset.
  size_t end_offset = (request.end_offset > 0 ?
                       request.end_offset : total_length);
  if (end_offset < start_offset) {
    LOG(WARNING) << "end offset (" << end_offset << ") precedes start offset ("
                 << start_offset << "), generating error response";
    return WriteHeaders(fd, 0, 0, kHttpResponseBadRequest);
  }
  if (end_offset > total_length) {
    LOG(INFO) << "requested end offset (" << end_offset
              << ") exceeds total length (" << total_length << "), adjusting";
    end_offset = total_length;
  }

  // Generate headers
  LOG(INFO) << "generating response header: range=" << start_offset << "-"
            << (end_offset - 1) << "/" << (end_offset - start_offset)
            << ", return code=" << request.return_code;
  if ((ret = WriteHeaders(fd, start_offset, end_offset,
                          request.return_code)) < 0)
    return -1;
  LOG(INFO) << ret << " header bytes written";
  written += ret;

  // Compute payload length, truncate as necessary.
  size_t payload_length = end_offset - start_offset;
  if (truncate_length > 0 && truncate_length < payload_length) {
    LOG(INFO) << "truncating request payload length (" << payload_length
              << ") at " << truncate_length;
    payload_length = truncate_length;
    end_offset = start_offset + payload_length;
  }

  LOG(INFO) << "generating response payload: range=" << start_offset << "-"
            << (end_offset - 1) << "/" << (end_offset - start_offset);

  // Decide about optional midway delay.
  if (truncate_length > 0 && sleep_every > 0 && sleep_secs >= 0 &&
      start_offset % (truncate_length * sleep_every) == 0) {
    const off_t midway_offset = start_offset + payload_length / 2;

    if ((ret = WritePayload(fd, start_offset, midway_offset)) < 0)
      return -1;
    LOG(INFO) << ret << " payload bytes written (first chunk)";
    written += ret;

    LOG(INFO) << "sleeping for " << sleep_secs << " seconds...";
    sleep(sleep_secs);

    if ((ret = WritePayload(fd, midway_offset, end_offset)) < 0)
      return -1;
    LOG(INFO) << ret << " payload bytes written (second chunk)";
    written += ret;
  } else {
    if ((ret = WritePayload(fd, start_offset, end_offset)) < 0)
      return -1;
    LOG(INFO) << ret << " payload bytes written";
    written += ret;
  }

  LOG(INFO) << "response generation complete, " << written
            << " total bytes written";
  return written;
}

ssize_t HandleGet(int fd, const HttpRequest& request,
                  const size_t total_length) {
  return HandleGet(fd, request, total_length, 0, 0, 0);
}

// Handles /redirect/<code>/<url> requests by returning the specified
// redirect <code> with a location pointing to /<url>.
void HandleRedirect(int fd, const HttpRequest& request) {
  LOG(INFO) << "Redirecting...";
  string url = request.url;
  CHECK_EQ(static_cast<size_t>(0), url.find("/redirect/"));
  url.erase(0, strlen("/redirect/"));
  string::size_type url_start = url.find('/');
  CHECK_NE(url_start, string::npos);
  HttpResponseCode code = StringToHttpResponseCode(url.c_str());
  url.erase(0, url_start);
  url = "http://" + request.host + url;
  const char *status = GetHttpResponseDescription(code);
  if (!status)
    CHECK(false) << "Unrecognized redirection code: " << code;
  LOG(INFO) << "Code: " << code << " " << status;
  LOG(INFO) << "New URL: " << url;

  ssize_t ret;
  if ((ret = WriteString(fd, "HTTP/1.1 " + Itoa(code) + " " +
                         status + EOL)) < 0)
    return;
  WriteString(fd, "Location: " + url + EOL);
}

// Generate a page not found error response with actual text payload. Return
// number of bytes written or -1 for error.
ssize_t HandleError(int fd, const HttpRequest& request) {
  LOG(INFO) << "Generating error HTTP response";

  ssize_t ret;
  size_t written = 0;

  const string data("This is an error page.");

  if ((ret = WriteHeaders(fd, 0, data.size(), kHttpResponseNotFound)) < 0)
    return -1;
  written += ret;

  if ((ret = WriteString(fd, data)) < 0)
    return -1;
  written += ret;

  return written;
}

// Generate an error response if the requested offset is nonzero, up to a given
// maximal number of successive failures.  The error generated is an "Internal
// Server Error" (500).
ssize_t HandleErrorIfOffset(int fd, const HttpRequest& request,
                            size_t end_offset, int max_fails) {
  static int num_fails = 0;

  if (request.start_offset > 0 && num_fails < max_fails) {
    LOG(INFO) << "Generating error HTTP response";

    ssize_t ret;
    size_t written = 0;

    const string data("This is an error page.");

    if ((ret = WriteHeaders(fd, 0, data.size(),
                            kHttpResponseInternalServerError)) < 0)
      return -1;
    written += ret;

    if ((ret = WriteString(fd, data)) < 0)
      return -1;
    written += ret;

    num_fails++;
    return written;
  } else {
    num_fails = 0;
    return HandleGet(fd, request, end_offset);
  }
}

void HandleDefault(int fd, const HttpRequest& request) {
  const off_t start_offset = request.start_offset;
  const string data("unhandled path");
  const size_t size = data.size();
  ssize_t ret;

  if ((ret = WriteHeaders(fd, start_offset, size, request.return_code)) < 0)
    return;
  WriteString(fd, (start_offset < static_cast<off_t>(size) ?
                   data.substr(start_offset) : ""));
}


// Break a URL into terms delimited by slashes.
class UrlTerms {
 public:
  UrlTerms(string &url, size_t num_terms) {
    // URL must be non-empty and start with a slash.
    CHECK_GT(url.size(), static_cast<size_t>(0));
    CHECK_EQ(url[0], '/');

    // Split it into terms delimited by slashes, omitting the preceeding slash.
    terms = strings::SplitDontTrim(url.substr(1), '/');

    // Ensure expected length.
    CHECK_EQ(terms.size(), num_terms);
  }

  inline string Get(const off_t index) const {
    return terms[index];
  }
  inline const char *GetCStr(const off_t index) const {
    return Get(index).c_str();
  }
  inline int GetInt(const off_t index) const {
    return atoi(GetCStr(index));
  }
  inline long GetLong(const off_t index) const {
    return atol(GetCStr(index));
  }

 private:
  std::vector<string> terms;
};

void HandleConnection(int fd) {
  HttpRequest request;
  ParseRequest(fd, &request);

  string &url = request.url;
  LOG(INFO) << "pid(" << getpid() <<  "): handling url " << url;
  if (url == "/quitquitquit") {
    HandleQuit(fd);
  } else if (StartsWith(url, "/download/")) {
    const UrlTerms terms(url, 2);
    HandleGet(fd, request, terms.GetLong(1));
  } else if (StartsWith(url, "/flaky/")) {
    const UrlTerms terms(url, 5);
    HandleGet(fd, request, terms.GetLong(1), terms.GetLong(2), terms.GetLong(3),
              terms.GetLong(4));
  } else if (url.find("/redirect/") == 0) {
    HandleRedirect(fd, request);
  } else if (url == "/error") {
    HandleError(fd, request);
  } else if (StartsWith(url, "/error-if-offset/")) {
    const UrlTerms terms(url, 3);
    HandleErrorIfOffset(fd, request, terms.GetLong(1), terms.GetInt(2));
  } else {
    HandleDefault(fd, request);
  }

  close(fd);
}

}  // namespace chromeos_update_engine

using namespace chromeos_update_engine;

int main(int argc, char** argv) {
  // Disable glog's default behavior of logging to files.
  FLAGS_logtostderr = true;
  google::InitGoogleLogging(argv[0]);

  // Ignore SIGPIPE on write() to sockets.
  signal(SIGPIPE, SIG_IGN);

  socklen_t clilen;
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  memset(&client_addr, 0, sizeof(client_addr));

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0)
    LOG(FATAL) << "socket() failed";

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(kServerPort);

  {
    // Get rid of "Address in use" error
    int tr = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &tr,
                   sizeof(int)) == -1) {
      perror("setsockopt");
      exit(2);
    }
  }

  if (bind(listen_fd, reinterpret_cast<struct sockaddr *>(&server_addr),
           sizeof(server_addr)) < 0) {
    perror("bind");
    exit(3);
  }
  CHECK_EQ(listen(listen_fd,5), 0);
  while (1) {
    LOG(INFO) << "pid(" << getpid() <<  "): waiting to accept new connection";
    clilen = sizeof(client_addr);
    int client_fd = accept(listen_fd,
                           (struct sockaddr *) &client_addr,
                           &clilen);
    LOG(INFO) << "got past accept";
    if (client_fd < 0)
      LOG(FATAL) << "ERROR on accept";
    HandleConnection(client_fd);
  }
  return 0;
}
