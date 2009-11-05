// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "update_engine/test_utils.h"
#include "base/logging.h"

#include "update_engine/file_writer.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

vector<char> ReadFile(const string& path) {
  vector<char> ret;
  FILE* fp = fopen(path.c_str(), "r");
  if (!fp)
    return ret;
  const size_t kChunkSize(1024);
  size_t read_size;
  do {
    char buf[kChunkSize];
    read_size = fread(buf, 1, kChunkSize, fp);
    ret.insert(ret.end(), buf, buf + read_size);
  } while (read_size == kChunkSize);
  fclose(fp);
  return ret;
}

bool WriteFile(const std::string& path, const std::vector<char>& data) {
  DirectFileWriter writer;
  if (0 != writer.Open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)) {
    return false;
  }
  if (static_cast<int>(data.size()) != writer.Write(&data[0], data.size())) {
    writer.Close();
    return false;
  }
  writer.Close();
  return true;
}

off_t FileSize(const string& path) {
  struct stat stbuf;
  int rc = stat(path.c_str(), &stbuf);
  CHECK_EQ(0, rc);
  if (rc < 0)
    return rc;
  return stbuf.st_size;
}

std::vector<char> GzipCompressData(const std::vector<char>& data) {
  const char fname[] = "/tmp/GzipCompressDataTemp";
  if (!WriteFile(fname, data)) {
    system((string("rm ") + fname).c_str());
    return vector<char>();
  }
  system((string("cat ") + fname + "|gzip>" + fname + ".gz").c_str());
  system((string("rm ") + fname).c_str());
  vector<char> ret = ReadFile(string(fname) + ".gz");
  system((string("rm ") + fname + ".gz").c_str());
  return ret;
}

}  // namespace chromeos_update_engine
