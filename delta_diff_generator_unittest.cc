// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "base/string_util.h"
#include <gtest/gtest.h>
#include "chromeos/obsolete_logging.h"
#include "update_engine/decompressing_file_writer.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_diff_parser.h"
#include "update_engine/gzip.h"
#include "update_engine/mock_file_writer.h"
#include "update_engine/subprocess.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using std::string;
using std::vector;

class DeltaDiffGeneratorTest : public ::testing::Test {};

namespace {
void DumpProto(const DeltaArchiveManifest* archive) {
  for (int i = 0; i < archive->files_size(); i++) {
    printf("Node %d\n", i);
    const DeltaArchiveManifest_File& file = archive->files(i);
    for (int j = 0; j < file.children_size(); j++) {
      const DeltaArchiveManifest_File_Child& child = file.children(j);
      printf("  %d %s\n", child.index(), child.name().c_str());
    }
  }
}

// The following files are generated at the path 'base':
// /
//  cdev (c 2 1)
//  dir/
//      bdev (b 3 1)
//      emptydir/ (owner:group = 501:503)
//      hello ("hello")
//      newempty ("")
//      subdir/
//             fifo
//             link -> /target
//  encoding/
//           long_new
//           long_small_change
//           nochange
//           onebyte
//  hi ("hi")
void GenerateFilesAtPath(const string& base) {
  const char* base_c = base.c_str();
  EXPECT_EQ(0, System(StringPrintf("echo hi > '%s/hi'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("mkdir -p '%s/dir'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("rm -f '%s/dir/bdev'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("mknod '%s/dir/bdev' b 3 1", base_c)));
  EXPECT_EQ(0, System(StringPrintf("rm -f '%s/cdev'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("mknod '%s/cdev' c 2 1", base_c)));
  EXPECT_EQ(0, System(StringPrintf("mkdir -p '%s/dir/subdir'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("mkdir -p '%s/dir/emptydir'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("chown 501:503 '%s/dir/emptydir'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("rm -f '%s/dir/subdir/fifo'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("mkfifo '%s/dir/subdir/fifo'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("ln -f -s /target '%s/dir/subdir/link'",
                                   base_c)));

  // Things that will encode differently:
  EXPECT_EQ(0, System(StringPrintf("mkdir -p '%s/encoding'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("echo nochange > '%s/encoding/nochange'",
                                   base_c)));
  EXPECT_EQ(0, System(StringPrintf("echo -n > '%s/encoding/onebyte'", base_c)));
  EXPECT_EQ(0, System(StringPrintf("echo -n > '%s/encoding/long_new'",
                                   base_c)));
  // Random 1 MiB byte length file
  EXPECT_TRUE(WriteFile((base +
                         "/encoding/long_small_change").c_str(),
                        reinterpret_cast<const char*>(kRandomString),
                        sizeof(kRandomString)));
}
// base points to a folder that was passed to GenerateFilesAtPath().
// This edits some, so that one can make a diff from the original data
// and the edited data.
void EditFilesAtPath(const string& base) {
  CHECK_EQ(0, System(string("echo hello > ") + base + "/dir/hello"));
  CHECK_EQ(0, System(string("echo -n > ") + base + "/dir/newempty"));
  CHECK_EQ(0, System(string("echo newhi > ") + base + "/hi"));
  CHECK_EQ(0, System(string("echo -n h >> ") + base +
                     "/encoding/onebyte"));
  CHECK_EQ(0, System(string("echo -n h >> ") + base +
                     "/encoding/long_small_change"));
  CHECK_EQ(0, System(string("echo -n This is a pice of text that should "
                            "compress well since it is just ascii and it "
                            "has repetition xxxxxxxxxxxxxxxxxxxxx"
                            "xxxxxxxxxxxxxxxxxxxx > ") + base +
                     "/encoding/long_new"));
}

}  // namespace {}

TEST_F(DeltaDiffGeneratorTest, FakerootEncodeMetadataToProtoBufferTest) {
  char cwd[1000];
  ASSERT_EQ(cwd, getcwd(cwd, sizeof(cwd))) << "cwd buf possibly too small";
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test"));
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test/old"));
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test/new"));
  GenerateFilesAtPath(string(cwd) + "/diff-gen-test/old");
  GenerateFilesAtPath(string(cwd) + "/diff-gen-test/new");
  EditFilesAtPath(string(cwd) + "/diff-gen-test/new");

  DeltaArchiveManifest* archive =
      DeltaDiffGenerator::EncodeMetadataToProtoBuffer(
          (string(cwd) + "/diff-gen-test/new").c_str());
  EXPECT_TRUE(NULL != archive);

  EXPECT_EQ(16, archive->files_size());
  //DumpProto(archive);
  const DeltaArchiveManifest_File& root = archive->files(0);
  EXPECT_TRUE(S_ISDIR(root.mode()));
  EXPECT_EQ(0, root.uid());
  EXPECT_EQ(0, root.gid());
  ASSERT_EQ(4, root.children_size());
  EXPECT_EQ("cdev", root.children(0).name());
  EXPECT_EQ("dir", root.children(1).name());
  EXPECT_EQ("encoding", root.children(2).name());
  EXPECT_EQ("hi", root.children(3).name());
  EXPECT_FALSE(root.has_data_format());
  EXPECT_FALSE(root.has_data_offset());
  EXPECT_FALSE(root.has_data_length());

  const DeltaArchiveManifest_File& cdev =
      archive->files(root.children(0).index());
  EXPECT_EQ(0, cdev.children_size());
  EXPECT_TRUE(S_ISCHR(cdev.mode()));
  EXPECT_EQ(0, cdev.uid());
  EXPECT_EQ(0, cdev.gid());
  EXPECT_FALSE(cdev.has_data_format());
  EXPECT_FALSE(cdev.has_data_offset());
  EXPECT_FALSE(cdev.has_data_length());

  const DeltaArchiveManifest_File& hi =
      archive->files(root.children(3).index());
  EXPECT_EQ(0, hi.children_size());
  EXPECT_TRUE(S_ISREG(hi.mode()));
  EXPECT_EQ(0, hi.uid());
  EXPECT_EQ(0, hi.gid());
  EXPECT_FALSE(hi.has_data_format());
  EXPECT_FALSE(hi.has_data_offset());
  EXPECT_FALSE(hi.has_data_length());

  const DeltaArchiveManifest_File& encoding =
      archive->files(root.children(2).index());
  EXPECT_TRUE(S_ISDIR(encoding.mode()));
  EXPECT_EQ(0, encoding.uid());
  EXPECT_EQ(0, encoding.gid());
  EXPECT_EQ(4, encoding.children_size());
  EXPECT_EQ("long_new", encoding.children(0).name());
  EXPECT_EQ("long_small_change", encoding.children(1).name());
  EXPECT_EQ("nochange", encoding.children(2).name());
  EXPECT_EQ("onebyte", encoding.children(3).name());
  EXPECT_FALSE(encoding.has_data_format());
  EXPECT_FALSE(encoding.has_data_offset());
  EXPECT_FALSE(encoding.has_data_length());

  const DeltaArchiveManifest_File& long_new =
      archive->files(encoding.children(0).index());
  EXPECT_EQ(0, long_new.children_size());
  EXPECT_TRUE(S_ISREG(long_new.mode()));
  EXPECT_EQ(0, long_new.uid());
  EXPECT_EQ(0, long_new.gid());
  EXPECT_FALSE(long_new.has_data_format());
  EXPECT_FALSE(long_new.has_data_offset());
  EXPECT_FALSE(long_new.has_data_length());

  const DeltaArchiveManifest_File& long_small_change =
      archive->files(encoding.children(1).index());
  EXPECT_EQ(0, long_small_change.children_size());
  EXPECT_TRUE(S_ISREG(long_small_change.mode()));
  EXPECT_EQ(0, long_small_change.uid());
  EXPECT_EQ(0, long_small_change.gid());
  EXPECT_FALSE(long_small_change.has_data_format());
  EXPECT_FALSE(long_small_change.has_data_offset());
  EXPECT_FALSE(long_small_change.has_data_length());

  const DeltaArchiveManifest_File& nochange =
      archive->files(encoding.children(2).index());
  EXPECT_EQ(0, nochange.children_size());
  EXPECT_TRUE(S_ISREG(nochange.mode()));
  EXPECT_EQ(0, nochange.uid());
  EXPECT_EQ(0, nochange.gid());
  EXPECT_FALSE(nochange.has_data_format());
  EXPECT_FALSE(nochange.has_data_offset());
  EXPECT_FALSE(nochange.has_data_length());

  const DeltaArchiveManifest_File& onebyte =
      archive->files(encoding.children(3).index());
  EXPECT_EQ(0, onebyte.children_size());
  EXPECT_TRUE(S_ISREG(onebyte.mode()));
  EXPECT_EQ(0, onebyte.uid());
  EXPECT_EQ(0, onebyte.gid());
  EXPECT_FALSE(onebyte.has_data_format());
  EXPECT_FALSE(onebyte.has_data_offset());
  EXPECT_FALSE(onebyte.has_data_length());

  const DeltaArchiveManifest_File& dir =
      archive->files(root.children(1).index());
  EXPECT_TRUE(S_ISDIR(dir.mode()));
  EXPECT_EQ(0, dir.uid());
  EXPECT_EQ(0, dir.gid());
  ASSERT_EQ(5, dir.children_size());
  EXPECT_EQ("bdev", dir.children(0).name());
  EXPECT_EQ("emptydir", dir.children(1).name());
  EXPECT_EQ("hello", dir.children(2).name());
  EXPECT_EQ("newempty", dir.children(3).name());
  EXPECT_EQ("subdir", dir.children(4).name());
  EXPECT_FALSE(dir.has_data_format());
  EXPECT_FALSE(dir.has_data_offset());
  EXPECT_FALSE(dir.has_data_length());

  const DeltaArchiveManifest_File& bdev =
      archive->files(dir.children(0).index());
  EXPECT_EQ(0, bdev.children_size());
  EXPECT_TRUE(S_ISBLK(bdev.mode()));
  EXPECT_EQ(0, bdev.uid());
  EXPECT_EQ(0, bdev.gid());
  EXPECT_FALSE(bdev.has_data_format());
  EXPECT_FALSE(bdev.has_data_offset());
  EXPECT_FALSE(bdev.has_data_length());

  const DeltaArchiveManifest_File& emptydir =
      archive->files(dir.children(1).index());
  EXPECT_EQ(0, emptydir.children_size());
  EXPECT_TRUE(S_ISDIR(emptydir.mode()));
  EXPECT_EQ(501, emptydir.uid());
  EXPECT_EQ(503, emptydir.gid());
  EXPECT_FALSE(emptydir.has_data_format());
  EXPECT_FALSE(emptydir.has_data_offset());
  EXPECT_FALSE(emptydir.has_data_length());

  const DeltaArchiveManifest_File& hello =
      archive->files(dir.children(2).index());
  EXPECT_EQ(0, hello.children_size());
  EXPECT_TRUE(S_ISREG(hello.mode()));
  EXPECT_EQ(0, hello.uid());
  EXPECT_EQ(0, hello.gid());
  EXPECT_FALSE(hello.has_data_format());
  EXPECT_FALSE(hello.has_data_offset());
  EXPECT_FALSE(hello.has_data_length());

  const DeltaArchiveManifest_File& newempty =
      archive->files(dir.children(3).index());
  EXPECT_EQ(0, newempty.children_size());
  EXPECT_TRUE(S_ISREG(newempty.mode()));
  EXPECT_EQ(0, newempty.uid());
  EXPECT_EQ(0, newempty.gid());
  EXPECT_FALSE(newempty.has_data_format());
  EXPECT_FALSE(newempty.has_data_offset());
  EXPECT_FALSE(newempty.has_data_length());

  const DeltaArchiveManifest_File& subdir =
      archive->files(dir.children(4).index());
  EXPECT_EQ(2, subdir.children_size());
  EXPECT_EQ("fifo", subdir.children(0).name());
  EXPECT_EQ("link", subdir.children(1).name());
  EXPECT_TRUE(S_ISDIR(subdir.mode()));
  EXPECT_EQ(0, subdir.uid());
  EXPECT_EQ(0, subdir.gid());
  EXPECT_FALSE(subdir.has_data_format());
  EXPECT_FALSE(subdir.has_data_offset());
  EXPECT_FALSE(subdir.has_data_length());

  const DeltaArchiveManifest_File& fifo =
      archive->files(subdir.children(0).index());
  EXPECT_EQ(0, fifo.children_size());
  EXPECT_TRUE(S_ISFIFO(fifo.mode()));
  EXPECT_EQ(0, fifo.uid());
  EXPECT_EQ(0, fifo.gid());
  EXPECT_FALSE(fifo.has_data_format());
  EXPECT_FALSE(fifo.has_data_offset());
  EXPECT_FALSE(fifo.has_data_length());

  const DeltaArchiveManifest_File& link =
      archive->files(subdir.children(1).index());
  EXPECT_EQ(0, link.children_size());
  EXPECT_TRUE(S_ISLNK(link.mode()));
  EXPECT_EQ(0, link.uid());
  EXPECT_EQ(0, link.gid());
  EXPECT_FALSE(link.has_data_format());
  EXPECT_FALSE(link.has_data_offset());
  EXPECT_FALSE(link.has_data_length());
}

TEST_F(DeltaDiffGeneratorTest, FakerootEncodeDataToDeltaFileTest) {
  char cwd[1000];
  ASSERT_EQ(cwd, getcwd(cwd, sizeof(cwd))) << "cwd buf possibly too small";
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test"));
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test/old"));
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test/new"));
  GenerateFilesAtPath(string(cwd) + "/diff-gen-test/old");
  GenerateFilesAtPath(string(cwd) + "/diff-gen-test/new");
  EditFilesAtPath(string(cwd) + "/diff-gen-test/new");

  DeltaArchiveManifest* archive =
      DeltaDiffGenerator::EncodeMetadataToProtoBuffer(
          (string(cwd) + "/diff-gen-test/new").c_str());
  EXPECT_TRUE(NULL != archive);

  EXPECT_TRUE(DeltaDiffGenerator::EncodeDataToDeltaFile(
      archive,
      string(cwd) + "/diff-gen-test/old",
      string(cwd) + "/diff-gen-test/new",
      string(cwd) + "/diff-gen-test/out.dat"));

  EXPECT_EQ(16, archive->files_size());

  const DeltaArchiveManifest_File& root = archive->files(0);
  EXPECT_TRUE(S_ISDIR(root.mode()));
  EXPECT_EQ(0, root.uid());
  EXPECT_EQ(0, root.gid());
  ASSERT_EQ(4, root.children_size());
  EXPECT_EQ("cdev", root.children(0).name());
  EXPECT_EQ("dir", root.children(1).name());
  EXPECT_EQ("encoding", root.children(2).name());
  EXPECT_EQ("hi", root.children(3).name());
  EXPECT_FALSE(root.has_data_format());
  EXPECT_FALSE(root.has_data_offset());
  EXPECT_FALSE(root.has_data_length());

  const DeltaArchiveManifest_File& cdev =
      archive->files(root.children(0).index());
  EXPECT_EQ(0, cdev.children_size());
  EXPECT_TRUE(S_ISCHR(cdev.mode()));
  EXPECT_EQ(0, cdev.uid());
  EXPECT_EQ(0, cdev.gid());
  ASSERT_TRUE(cdev.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, cdev.data_format());
  EXPECT_TRUE(cdev.has_data_offset());
  ASSERT_TRUE(cdev.has_data_length());
  EXPECT_GT(cdev.data_length(), 0);

  const DeltaArchiveManifest_File& hi =
      archive->files(root.children(3).index());
  EXPECT_EQ(0, hi.children_size());
  EXPECT_TRUE(S_ISREG(hi.mode()));
  EXPECT_EQ(0, hi.uid());
  EXPECT_EQ(0, hi.gid());
  ASSERT_TRUE(hi.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, hi.data_format());
  EXPECT_TRUE(hi.has_data_offset());
  ASSERT_TRUE(hi.has_data_length());
  EXPECT_GT(hi.data_length(), 0);

  const DeltaArchiveManifest_File& encoding =
      archive->files(root.children(2).index());
  EXPECT_TRUE(S_ISDIR(encoding.mode()));
  EXPECT_EQ(0, encoding.uid());
  EXPECT_EQ(0, encoding.gid());
  EXPECT_EQ(4, encoding.children_size());
  EXPECT_EQ("long_new", encoding.children(0).name());
  EXPECT_EQ("long_small_change", encoding.children(1).name());
  EXPECT_EQ("nochange", encoding.children(2).name());
  EXPECT_EQ("onebyte", encoding.children(3).name());
  EXPECT_FALSE(encoding.has_data_format());
  EXPECT_FALSE(encoding.has_data_offset());
  EXPECT_FALSE(encoding.has_data_length());

  const DeltaArchiveManifest_File& long_new =
      archive->files(encoding.children(0).index());
  EXPECT_EQ(0, long_new.children_size());
  EXPECT_TRUE(S_ISREG(long_new.mode()));
  EXPECT_EQ(0, long_new.uid());
  EXPECT_EQ(0, long_new.gid());
  EXPECT_TRUE(long_new.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL_GZ,
            long_new.data_format());
  EXPECT_TRUE(long_new.has_data_offset());
  EXPECT_TRUE(long_new.has_data_length());

  const DeltaArchiveManifest_File& long_small_change =
      archive->files(encoding.children(1).index());
  EXPECT_EQ(0, long_small_change.children_size());
  EXPECT_TRUE(S_ISREG(long_small_change.mode()));
  EXPECT_EQ(0, long_small_change.uid());
  EXPECT_EQ(0, long_small_change.gid());
  EXPECT_TRUE(long_small_change.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_BSDIFF,
            long_small_change.data_format());
  EXPECT_TRUE(long_small_change.has_data_offset());
  EXPECT_TRUE(long_small_change.has_data_length());

  const DeltaArchiveManifest_File& nochange =
      archive->files(encoding.children(2).index());
  EXPECT_EQ(0, nochange.children_size());
  EXPECT_TRUE(S_ISREG(nochange.mode()));
  EXPECT_EQ(0, nochange.uid());
  EXPECT_EQ(0, nochange.gid());
  EXPECT_TRUE(nochange.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, nochange.data_format());
  EXPECT_TRUE(nochange.has_data_offset());
  EXPECT_TRUE(nochange.has_data_length());

  const DeltaArchiveManifest_File& onebyte =
      archive->files(encoding.children(3).index());
  EXPECT_EQ(0, onebyte.children_size());
  EXPECT_TRUE(S_ISREG(onebyte.mode()));
  EXPECT_EQ(0, onebyte.uid());
  EXPECT_EQ(0, onebyte.gid());
  EXPECT_TRUE(onebyte.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, onebyte.data_format());
  EXPECT_TRUE(onebyte.has_data_offset());
  EXPECT_TRUE(onebyte.has_data_length());
  EXPECT_EQ(1, onebyte.data_length());

  const DeltaArchiveManifest_File& dir =
      archive->files(root.children(1).index());
  EXPECT_TRUE(S_ISDIR(dir.mode()));
  EXPECT_EQ(0, dir.uid());
  EXPECT_EQ(0, dir.gid());
  ASSERT_EQ(5, dir.children_size());
  EXPECT_EQ("bdev", dir.children(0).name());
  EXPECT_EQ("emptydir", dir.children(1).name());
  EXPECT_EQ("hello", dir.children(2).name());
  EXPECT_EQ("newempty", dir.children(3).name());
  EXPECT_EQ("subdir", dir.children(4).name());
  EXPECT_FALSE(dir.has_data_format());
  EXPECT_FALSE(dir.has_data_offset());
  EXPECT_FALSE(dir.has_data_length());

  const DeltaArchiveManifest_File& bdev =
      archive->files(dir.children(0).index());
  EXPECT_EQ(0, bdev.children_size());
  EXPECT_TRUE(S_ISBLK(bdev.mode()));
  EXPECT_EQ(0, bdev.uid());
  EXPECT_EQ(0, bdev.gid());
  ASSERT_TRUE(bdev.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, bdev.data_format());
  EXPECT_TRUE(bdev.has_data_offset());
  ASSERT_TRUE(bdev.has_data_length());
  EXPECT_GT(bdev.data_length(), 0);

  const DeltaArchiveManifest_File& emptydir =
      archive->files(dir.children(1).index());
  EXPECT_EQ(0, emptydir.children_size());
  EXPECT_TRUE(S_ISDIR(emptydir.mode()));
  EXPECT_EQ(501, emptydir.uid());
  EXPECT_EQ(503, emptydir.gid());
  EXPECT_FALSE(emptydir.has_data_format());
  EXPECT_FALSE(emptydir.has_data_offset());
  EXPECT_FALSE(emptydir.has_data_length());

  const DeltaArchiveManifest_File& hello =
      archive->files(dir.children(2).index());
  EXPECT_EQ(0, hello.children_size());
  EXPECT_TRUE(S_ISREG(hello.mode()));
  EXPECT_EQ(0, hello.uid());
  EXPECT_EQ(0, hello.gid());
  ASSERT_TRUE(hello.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, hello.data_format());
  EXPECT_TRUE(hello.has_data_offset());
  ASSERT_TRUE(hello.has_data_length());
  EXPECT_GT(hello.data_length(), 0);

  const DeltaArchiveManifest_File& newempty =
      archive->files(dir.children(3).index());
  EXPECT_EQ(0, newempty.children_size());
  EXPECT_TRUE(S_ISREG(newempty.mode()));
  EXPECT_EQ(0, newempty.uid());
  EXPECT_EQ(0, newempty.gid());
  EXPECT_FALSE(newempty.has_data_format());
  EXPECT_FALSE(newempty.has_data_offset());
  EXPECT_FALSE(newempty.has_data_length());

  const DeltaArchiveManifest_File& subdir =
      archive->files(dir.children(4).index());
  EXPECT_EQ(2, subdir.children_size());
  EXPECT_EQ("fifo", subdir.children(0).name());
  EXPECT_EQ("link", subdir.children(1).name());
  EXPECT_TRUE(S_ISDIR(subdir.mode()));
  EXPECT_EQ(0, subdir.uid());
  EXPECT_EQ(0, subdir.gid());
  EXPECT_FALSE(subdir.has_data_format());
  EXPECT_FALSE(subdir.has_data_offset());
  EXPECT_FALSE(subdir.has_data_length());

  const DeltaArchiveManifest_File& fifo =
      archive->files(subdir.children(0).index());
  EXPECT_EQ(0, fifo.children_size());
  EXPECT_TRUE(S_ISFIFO(fifo.mode()));
  EXPECT_EQ(0, fifo.uid());
  EXPECT_EQ(0, fifo.gid());
  EXPECT_FALSE(fifo.has_data_format());
  EXPECT_FALSE(fifo.has_data_offset());
  EXPECT_FALSE(fifo.has_data_length());

  const DeltaArchiveManifest_File& link =
      archive->files(subdir.children(1).index());
  EXPECT_EQ(0, link.children_size());
  EXPECT_TRUE(S_ISLNK(link.mode()));
  EXPECT_EQ(0, link.uid());
  EXPECT_EQ(0, link.gid());
  ASSERT_TRUE(link.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, link.data_format());
  EXPECT_TRUE(link.has_data_offset());
  ASSERT_TRUE(link.has_data_length());
  EXPECT_GT(link.data_length(), 0);
}

class DeltaDiffParserTest : public ::testing::Test {
  virtual void TearDown() {
    EXPECT_EQ(0, system("rm -rf diff-gen-test"));
  }
};

namespace {
// Reads part of a file into memory
vector<char> ReadFilePart(const string& path, off_t start, off_t size) {
  vector<char> ret;
  int fd = open(path.c_str(), O_RDONLY, 0);
  if (fd < 0)
    return ret;
  ret.resize(size);
  EXPECT_EQ(size, pread(fd, &ret[0], size, start));
  close(fd);
  return ret;
}

string ReadFilePartToString(const string& path, off_t start, off_t size) {
  vector<char> bytes = ReadFilePart(path, start, size);
  string ret;
  ret.append(&bytes[0], bytes.size());
  return ret;
}

string StringFromVectorChar(const vector<char>& in) {
  return string(&in[0], in.size());
}

string GzipDecompressToString(const vector<char>& in) {
  vector<char> out;
  EXPECT_TRUE(GzipDecompress(in, &out));
  return StringFromVectorChar(out);
}

}

TEST_F(DeltaDiffParserTest, FakerootDecodeDataFromDeltaFileTest) {
  char cwd[1000];
  ASSERT_EQ(cwd, getcwd(cwd, sizeof(cwd))) << "cwd buf possibly too small";
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test"));
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test/old"));
  ASSERT_EQ(0, System(string("mkdir -p ") + cwd + "/diff-gen-test/new"));
  GenerateFilesAtPath(string(cwd) + "/diff-gen-test/old");
  GenerateFilesAtPath(string(cwd) + "/diff-gen-test/new");
  EditFilesAtPath(string(cwd) + "/diff-gen-test/new");

  DeltaArchiveManifest* archive =
      DeltaDiffGenerator::EncodeMetadataToProtoBuffer(
          (string(cwd) + "/diff-gen-test/new").c_str());
  EXPECT_TRUE(NULL != archive);

  EXPECT_TRUE(DeltaDiffGenerator::EncodeDataToDeltaFile(
      archive,
      string(cwd) + "/diff-gen-test/old",
      string(cwd) + "/diff-gen-test/new",
      string(cwd) + "/diff-gen-test/out.dat"));
  // parse the file

  DeltaDiffParser parser(string(cwd) + "/diff-gen-test/out.dat");
  ASSERT_TRUE(parser.valid());
  DeltaDiffParser::Iterator it = parser.Begin();
  string expected_paths[] = {
    "",
    "/cdev",
    "/dir",
    "/dir/bdev",
    "/dir/emptydir",
    "/dir/hello",
    "/dir/newempty",
    "/dir/subdir",
    "/dir/subdir/fifo",
    "/dir/subdir/link",
    "/encoding",
    "/encoding/long_new",
    "/encoding/long_small_change",
    "/encoding/nochange",
    "/encoding/onebyte",
    "/hi"
  };
  for (unsigned int i = 0;
       i < (sizeof(expected_paths)/sizeof(expected_paths[0])); i++) {
    ASSERT_TRUE(it != parser.End());
    ASSERT_TRUE(parser.ContainsPath(expected_paths[i]));
    EXPECT_EQ(expected_paths[i], it.path());
    EXPECT_EQ(expected_paths[i].substr(expected_paths[i].find_last_of('/') + 1),
              it.GetName());
    DeltaArchiveManifest_File f1 = parser.GetFileAtPath(expected_paths[i]);
    DeltaArchiveManifest_File f2 = it.GetFile();
    EXPECT_EQ(f1.mode(), f2.mode()) << it.path();
    EXPECT_EQ(f1.uid(), f2.uid());
    EXPECT_EQ(f1.gid(), f2.gid());
    EXPECT_EQ(f1.has_data_format(), f2.has_data_format());
    if (f1.has_data_format()) {
      EXPECT_EQ(f1.data_format(), f2.data_format());
      EXPECT_TRUE(f1.has_data_offset());
      EXPECT_TRUE(f2.has_data_offset());
      EXPECT_EQ(f1.data_offset(), f2.data_offset());
    } else {
      EXPECT_FALSE(f2.has_data_format());
      EXPECT_FALSE(f1.has_data_offset());
      EXPECT_FALSE(f2.has_data_offset());
    }
    EXPECT_EQ(f1.children_size(), f2.children_size());
    for (int j = 0; j < f1.children_size(); j++) {
      EXPECT_EQ(f1.children(j).name(), f2.children(j).name());
      EXPECT_EQ(f1.children(j).index(), f2.children(j).index());
    }
    it.Increment();
  }
  EXPECT_TRUE(it == parser.End());
  EXPECT_FALSE(parser.ContainsPath("/cdew"));
  EXPECT_FALSE(parser.ContainsPath("/hi/hi"));
  EXPECT_FALSE(parser.ContainsPath("/dir/newempty/hi"));
  EXPECT_TRUE(parser.ContainsPath("/dir/"));

  // Check the data
  // root
  DeltaArchiveManifest_File file = parser.GetFileAtPath("");
  EXPECT_TRUE(S_ISDIR(file.mode()));
  EXPECT_FALSE(file.has_data_format());

  // cdev
  file = parser.GetFileAtPath("/cdev");
  EXPECT_TRUE(S_ISCHR(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  vector<char> data = ReadFilePart(string(cwd) + "/diff-gen-test/out.dat",
                                   file.data_offset(), file.data_length());
  LinuxDevice linux_device;
  linux_device.ParseFromArray(&data[0], data.size());
  EXPECT_EQ(linux_device.major(), 2);
  EXPECT_EQ(linux_device.minor(), 1);

  // dir
  file = parser.GetFileAtPath("/dir");
  EXPECT_TRUE(S_ISDIR(file.mode()));
  EXPECT_FALSE(file.has_data_format());

  // bdev
  file = parser.GetFileAtPath("/dir/bdev");
  EXPECT_TRUE(S_ISBLK(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  data = ReadFilePart(string(cwd) + "/diff-gen-test/out.dat",
                      file.data_offset(), file.data_length());
  linux_device.ParseFromArray(&data[0], data.size());
  EXPECT_EQ(linux_device.major(), 3);
  EXPECT_EQ(linux_device.minor(), 1);

  // emptydir
  file = parser.GetFileAtPath("/dir/emptydir");
  EXPECT_TRUE(S_ISDIR(file.mode()));
  EXPECT_FALSE(file.has_data_format());

  // hello
  file = parser.GetFileAtPath("/dir/hello");
  EXPECT_TRUE(S_ISREG(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, file.data_format());
  EXPECT_EQ("hello\n", ReadFilePartToString(string(cwd) +
                                            "/diff-gen-test/out.dat",
                                            file.data_offset(),
                                            file.data_length()));

  // newempty
  file = parser.GetFileAtPath("/dir/newempty");
  EXPECT_TRUE(S_ISREG(file.mode()));
  EXPECT_FALSE(file.has_data_format());

  // subdir
  file = parser.GetFileAtPath("/dir/subdir");
  EXPECT_TRUE(S_ISDIR(file.mode()));
  EXPECT_FALSE(file.has_data_format());

  // fifo
  file = parser.GetFileAtPath("/dir/subdir/fifo");
  EXPECT_TRUE(S_ISFIFO(file.mode()));
  EXPECT_FALSE(file.has_data_format());

  // link
  file = parser.GetFileAtPath("/dir/subdir/link");
  EXPECT_TRUE(S_ISLNK(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, file.data_format());
  EXPECT_EQ("/target", ReadFilePartToString(string(cwd) +
                                            "/diff-gen-test/out.dat",
                                            file.data_offset(),
                                            file.data_length()));

  // encoding
  file = parser.GetFileAtPath("/encoding");
  EXPECT_TRUE(S_ISDIR(file.mode()));
  EXPECT_FALSE(file.has_data_format());

  // long_new
  file = parser.GetFileAtPath("/encoding/long_new");
  EXPECT_TRUE(S_ISREG(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL_GZ, file.data_format());
  EXPECT_EQ("This is a pice of text that should "
            "compress well since it is just ascii and it "
            "has repetition xxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxx",
            GzipDecompressToString(ReadFilePart(string(cwd) +
                                                "/diff-gen-test/out.dat",
                                                file.data_offset(),
                                                file.data_length())));

  // long_small_change
  file = parser.GetFileAtPath("/encoding/long_small_change");
  EXPECT_TRUE(S_ISREG(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_BSDIFF, file.data_format());
  data = ReadFilePart(string(cwd) + "/diff-gen-test/out.dat",
                      file.data_offset(), file.data_length());
  WriteFileVector(string(cwd) + "/diff-gen-test/patch", data);
  int rc = 1;
  vector<string> cmd;
  cmd.push_back("/usr/bin/bspatch");
  cmd.push_back(string(cwd) + "/diff-gen-test/old/encoding/long_small_change");
  cmd.push_back(string(cwd) + "/diff-gen-test/patch_result");
  cmd.push_back(string(cwd) + "/diff-gen-test/patch");
  Subprocess::SynchronousExec(cmd, &rc);
  EXPECT_EQ(0, rc);
  vector<char> patch_result;
  EXPECT_TRUE(utils::ReadFile(string(cwd) + "/diff-gen-test/patch_result",
                              &patch_result));
  vector<char> expected_data(sizeof(kRandomString) + 1);
  memcpy(&expected_data[0], kRandomString, sizeof(kRandomString));
  expected_data[expected_data.size() - 1] = 'h';
  ExpectVectorsEq(expected_data, patch_result);

  // nochange
  file = parser.GetFileAtPath("/encoding/nochange");
  EXPECT_TRUE(S_ISREG(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, file.data_format());
  EXPECT_EQ("nochange\n", ReadFilePartToString(string(cwd) +
                                               "/diff-gen-test/out.dat",
                                               file.data_offset(),
                                               file.data_length()));

  // onebyte
  file = parser.GetFileAtPath("/encoding/onebyte");
  EXPECT_TRUE(S_ISREG(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, file.data_format());
  EXPECT_EQ("h", ReadFilePartToString(string(cwd) +
                                      "/diff-gen-test/out.dat",
                                      file.data_offset(),
                                      file.data_length()));

  // hi
  file = parser.GetFileAtPath("/hi");
  EXPECT_TRUE(S_ISREG(file.mode()));
  EXPECT_TRUE(file.has_data_format());
  EXPECT_EQ(DeltaArchiveManifest_File_DataFormat_FULL, file.data_format());
  EXPECT_EQ("newhi\n", ReadFilePartToString(string(cwd) +
                                            "/diff-gen-test/out.dat",
                                            file.data_offset(),
                                            file.data_length()));
}

TEST_F(DeltaDiffParserTest, FakerootInvalidTest) {
  ASSERT_EQ(0, mkdir("diff-gen-test", 0777));
  {
    DeltaDiffParser parser("/no/such/file");
    EXPECT_FALSE(parser.valid());
  }
  {
    vector<char> data(3);
    memcpy(&data[0], "CrA", 3);
    WriteFileVector("diff-gen-test/baddelta", data);
    DeltaDiffParser parser("diff-gen-test/baddelta");
    EXPECT_FALSE(parser.valid());
  }
  {
    vector<char> data(5);
    memcpy(&data[0], "CrAPx", 5);
    WriteFileVector("diff-gen-test/baddelta", data);
    DeltaDiffParser parser("diff-gen-test/baddelta");
    EXPECT_FALSE(parser.valid());
  }
  {
    vector<char> data(5);
    memcpy(&data[0], "CrAU\0", 5);
    WriteFileVector("diff-gen-test/baddelta", data);
    DeltaDiffParser parser("diff-gen-test/baddelta");
    EXPECT_FALSE(parser.valid());
  }
  {
    vector<char> data(14);
    memcpy(&data[0], "CrAU\0\0\0\0\0\0\0\x0cxx", 12);
    WriteFileVector("diff-gen-test/baddelta", data);
    DeltaDiffParser parser("diff-gen-test/baddelta");
    EXPECT_FALSE(parser.valid());
  }
}

}  // namespace chromeos_update_engine
