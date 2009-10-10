// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <glib.h>
#include <glog/logging.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

// This code runs inside the main loop
gboolean EntryPoint(gpointer data) {
  GMainLoop *loop = reinterpret_cast<GMainLoop*>(data);
  LOG(INFO) << "Chrome OS Update Engine beginning";
  g_main_loop_quit(loop);
  return FALSE;
}

int main(int argc, char** argv) {
  xmlDocPtr doc = xmlNewDoc((const xmlChar*)"1.0");
  CHECK(doc);
  CHECK_EQ(argc, 2);
  printf("enc: [%s]\n", xmlEncodeEntitiesReentrant(doc, (const xmlChar*)argv[1]));
  return 0;
  
  
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  g_timeout_add(0, &EntryPoint, loop);
  g_main_loop_run(loop);
  return 0;
}
