#!/usr/bin/env python

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a simple HTTP server that's used by the
# libcurl_http_fetcher_unittest, though it could be used by others. In
# general, you can fork off this server, repeatedly request /test or
# some URL until that URL succeeds; then you know the server is
# running. The url /big returns 100,000 bytes of predictable data. The
# url /quitquitquit causes the server to exit.

import SimpleHTTPServer, BaseHTTPServer, httplib

class TestHttpRequestHandler (SimpleHTTPServer.SimpleHTTPRequestHandler):
  def do_GET(self):
    # Exit the server
    if self.path == '/quitquitquit':
      self.server.stop = True

    # Write 100,000 bytes out
    if self.path == '/big':
      self.send_response(200, 'OK')
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      for i in range(0, 10000):
        try:
          self.wfile.write('abcdefghij');  # 10 characters
        except IOError:
          return
      return

    # Everything else
    self.send_response(200, 'OK')
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('unhandled path')

class TestHttpServer (BaseHTTPServer.HTTPServer):
  def serve_forever(self):
    self.stop = False
    while not self.stop:
      self.handle_request()

def main():
  # TODO(adlr): Choose a port that works with build bots and report it to
  # caller.
  # WARNING, if you update this, you must also update http_fetcher_unittest.cc
  port = 8080
  server = TestHttpServer(('', 8080), TestHttpRequestHandler)
  server.serve_forever()

if __name__ == '__main__':
  main()
