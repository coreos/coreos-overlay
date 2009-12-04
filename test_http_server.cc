// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a simple HTTP server. It can exhibit odd behavior
// that's useful for testing. For example, it's useful to test that
// the updater can continue a connection if it's dropped, or that it
// handles very slow data transfers.

// To use this, simply make an HTTP connection to localhost:port and
// GET a url.

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>
#include "chromeos/obsolete_logging.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

struct HttpRequest {
  string url;
  off_t offset;
};

namespace {
const int kPort = 8080;  // hardcoded to 8080 for now
const int kBigLength = 100000;
}

bool ParseRequest(int fd, HttpRequest* request) {
  string headers;
  while(headers.find("\r\n\r\n") == string::npos) {
    vector<char> buf(1024);
    memset(&buf[0], 0, buf.size());
    ssize_t r = read(fd, &buf[0], buf.size() - 1);
    if (r < 0) {
      perror("read");
      exit(1);
    }
    buf.resize(r);

    headers.insert(headers.end(), buf.begin(), buf.end());
  }
  LOG(INFO) << "got headers: " << headers;

  string::size_type url_start, url_end;
  CHECK_NE(headers.find("GET "), string::npos);
  url_start = headers.find("GET ") + strlen("GET ");
  url_end = headers.find(' ', url_start);
  CHECK_NE(string::npos, url_end);
  string url = headers.substr(url_start, url_end - url_start);
  LOG(INFO) << "URL: " << url;

  string::size_type range_start, range_end;
  if (headers.find("\r\nRange: ") == string::npos) {
    request->offset = 0;
  } else {
    range_start = headers.find("\r\nRange: ") + strlen("\r\nRange: ");
    range_end = headers.find('\r', range_start);
    CHECK_NE(string::npos, range_end);
    string range_header = headers.substr(range_start, range_end - range_start);

    LOG(INFO) << "Range: " << range_header;
    CHECK(*range_header.rbegin() == '-');
    request->offset = atoll(range_header.c_str() + strlen("bytes="));
    LOG(INFO) << "Offset: " << request->offset;
  }
  request->url = url;
  return true;
}

void WriteString(int fd, const string& str) {
  unsigned int bytes_written = 0;
  while (bytes_written < str.size()) {
    ssize_t r = write(fd, str.c_str() + bytes_written,
                      str.size() - bytes_written);
    LOG(INFO) << "write() wrote " << r << " bytes";
    if (r < 0) {
      perror("write");
      return;
    }
    bytes_written += r;
  }
  LOG(INFO) << "WriteString wrote " << bytes_written << " bytes";
}

string Itoa(off_t num) {
  char buf[100] = {0};
  snprintf(buf, sizeof(buf), "%lld", num);
  return buf;
}

void WriteHeaders(int fd, bool support_range, off_t full_size,
                  off_t start_offset) {
  LOG(INFO) << "writing headers";
  WriteString(fd, "HTTP/1.1 200 OK\r\n");
  WriteString(fd, "Content-Type: application/octet-stream\r\n");
  if (support_range) {
    WriteString(fd, "Accept-Ranges: bytes\r\n");
    WriteString(fd, string("Content-Range: bytes ") + Itoa(start_offset) +
                "-" + Itoa(full_size - 1) + "/" + Itoa(full_size) + "\r\n");
  }
  off_t content_length = full_size;
  if (support_range)
    content_length -= start_offset;
  WriteString(fd, string("Content-Length: ") + Itoa(content_length) + "\r\n");
  WriteString(fd, "\r\n");
}

void HandleQuitQuitQuit(int fd) {
  exit(0);
}

void HandleBig(int fd, const HttpRequest& request) {
  const off_t full_length = kBigLength;
  WriteHeaders(fd, true, full_length, request.offset);
  const off_t content_length = full_length - request.offset;
  int i = request.offset;
  for (; i % 10; i++)
    WriteString(fd, string(1, 'a' + (i % 10)));
  CHECK_EQ(i % 10, 0);
  for (; i < content_length; i += 10)
    WriteString(fd, "abcdefghij");
  CHECK_EQ(i, full_length);
}

// This is like /big, but it writes at most 9000 bytes. Also,
// half way through it sleeps for 70 seconds
// (technically, when (offset % (9000 * 7)) == 0).
void HandleFlaky(int fd, const HttpRequest& request) {
  const off_t full_length = kBigLength;
  WriteHeaders(fd, true, full_length, request.offset);
  const off_t content_length = min(9000LL, full_length - request.offset);
  const bool should_sleep = (request.offset % (9000 * 7)) == 0;

  string buf;

  for (int i = request.offset; i % 10; i++)
    buf.append(1, 'a' + (i % 10));
  while (buf.size() < content_length)
    buf.append("abcdefghij");
  buf.resize(content_length);

  if (!should_sleep) {
    LOG(INFO) << "writing data blob of size " << buf.size();
    WriteString(fd, buf);
  } else {
    string::size_type half_way_point = buf.size() / 2;
    LOG(INFO) << "writing small data blob of size " << half_way_point;
    WriteString(fd, buf.substr(0, half_way_point));
    sleep(10);
    LOG(INFO) << "writing small data blob of size "
              << (buf.size() - half_way_point);
    WriteString(fd, buf.substr(half_way_point, buf.size() - half_way_point));
  }
}

void HandleDefault(int fd, const HttpRequest& request) {
  const string data("unhandled path");
  WriteHeaders(fd, true, data.size(), request.offset);
  const string data_to_write(data.substr(request.offset,
                                         data.size() - request.offset));
  WriteString(fd, data_to_write);
}

void HandleConnection(int fd) {
  HttpRequest request;
  ParseRequest(fd, &request);

  if (request.url == "/quitquitquit")
    HandleQuitQuitQuit(fd);
  else if (request.url == "/big")
    HandleBig(fd, request);
  else if (request.url == "/flaky")
    HandleFlaky(fd, request);
  else
    HandleDefault(fd, request);

  close(fd);
}

}  // namespace chromeos_update_engine

using namespace chromeos_update_engine;

int main(int argc, char** argv) {
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
  server_addr.sin_port = htons(kPort);

  {
    // Get rid of "Address in use" error
    int tr = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &tr,
                   sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }
  }

  if (bind(listen_fd, reinterpret_cast<struct sockaddr *>(&server_addr),
           sizeof(server_addr)) < 0) {
    perror("bind");
    exit(1);
  }
  CHECK_EQ(listen(listen_fd,5), 0);
  while (1) {
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
