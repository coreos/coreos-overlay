// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_diff_parser.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "base/scoped_ptr.h"
#include "update_engine/decompressing_file_writer.h"
#include "update_engine/gzip.h"
#include "update_engine/utils.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const int kCopyFileBufferSize = 4096;
}

const char* const DeltaDiffParser::kFileMagic("CrAU");

// The iterator returns a directory before returning its children.
// Steps taken in Increment():
//   - See if the current item has children. If so, the child becomes
//     the new current item and we return.
//   - If current item has no children, we loop. Each loop iteration
//     considers an item (first the current item, then its parent,
//     then grand parent, and so on). Each loop iteration, we see if there
//     are any siblings we haven't iterated on yet. If so, we're done.
//     If not, keep looping to parents.
void DeltaDiffParserIterator::Increment() {
  // See if we have any children.
  const DeltaArchiveManifest_File& file = GetFile();
  if (file.children_size() > 0) {
    path_indices_.push_back(file.children(0).index());
    path_ += "/";
    path_ += file.children(0).name();
    child_indices_.push_back(0);
    return;
  }
  // Look in my parent for the next child, then try grandparent, etc.

  path_indices_.pop_back();
  path_.resize(path_.rfind('/'));

  while (!child_indices_.empty()) {
    // Try to bump the last entry
    CHECK_EQ(path_indices_.size(), child_indices_.size());
    child_indices_.back()++;
    const DeltaArchiveManifest_File& parent =
        archive_->files(path_indices_.back());
    if (parent.children_size() > child_indices_.back()) {
      // we found a new child!
      path_indices_.push_back(parent.children(child_indices_.back()).index());
      path_ += "/";
      path_ += parent.children(child_indices_.back()).name();
      return;
    }
    path_indices_.pop_back();
    child_indices_.pop_back();
    if (!path_.empty())
      path_.resize(path_.rfind('/'));
  }
}

const string DeltaDiffParserIterator::GetName() const {
  if (path_.empty())
    return "";
  CHECK_NE(path_.rfind('/'), string::npos);
  return string(path_, path_.rfind('/') + 1);
}

const DeltaArchiveManifest_File& DeltaDiffParserIterator::GetFile() const {
  CHECK(!path_indices_.empty());
  return archive_->files(path_indices_.back());
}


DeltaDiffParser::DeltaDiffParser(const string& delta_file)
    : fd_(-1),
      valid_(false) {
  fd_ = open(delta_file.c_str(), O_RDONLY, 0);
  if (fd_ < 0) {
    LOG(ERROR) << "Unable to open delta file: " << delta_file;
    return;
  }
  ScopedFdCloser fd_closer(&fd_);
  scoped_array<char> magic(new char[strlen(kFileMagic)]);
  if (strlen(kFileMagic) != read(fd_, magic.get(), strlen(kFileMagic))) {
    LOG(ERROR) << "delta file too short";
    return;
  }
  if (strncmp(magic.get(), kFileMagic, strlen(kFileMagic))) {
    LOG(ERROR) << "Incorrect magic at beginning of delta file";
    return;
  }

  int64 proto_offset = 0;
  COMPILE_ASSERT(sizeof(proto_offset) == sizeof(off_t), off_t_wrong_size);
  if (sizeof(proto_offset) != read(fd_, &proto_offset, sizeof(proto_offset))) {
    LOG(ERROR) << "delta file too short";
    return;
  }
  proto_offset = be64toh(proto_offset);  // switch from big-endian to host

  int64 proto_length = 0;
  if (sizeof(proto_length) != read(fd_, &proto_length, sizeof(proto_length))) {
    LOG(ERROR) << "delta file too short";
    return;
  }
  proto_length = be64toh(proto_length);  // switch from big-endian to host

  vector<char> proto(proto_length);
  size_t bytes_read = 0;
  while (bytes_read < proto_length) {
    ssize_t r = pread(fd_, &proto[bytes_read], proto_length - bytes_read,
                      proto_offset + bytes_read);
    TEST_AND_RETURN(r >= 0);
    bytes_read += r;
  }
  {
    vector<char> decompressed_proto;
    TEST_AND_RETURN(GzipDecompress(proto, &decompressed_proto));
    proto.swap(decompressed_proto);
  }

  valid_ = archive_.ParseFromArray(&proto[0], proto.size());
  if (valid_) {
    fd_closer.set_should_close(false);
  } else {
    LOG(ERROR) << "load from file failed";
  }
}

DeltaDiffParser::~DeltaDiffParser() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

bool DeltaDiffParser::ContainsPath(const string& path) const {
  return GetIndexForPath(path) >= 0;
}

const DeltaArchiveManifest_File& DeltaDiffParser::GetFileAtPath(
    const string& path) const {
  int idx = GetIndexForPath(path);
  CHECK_GE(idx, 0) << path;
  return archive_.files(idx);
}

// Returns -1 if not found.
int DeltaDiffParser::GetIndexOfFileChild(
    const DeltaArchiveManifest_File& file, const string& child_name) const {
  if (file.children_size() == 0)
    return -1;
  int begin = 0;
  int end = file.children_size();
  while (begin < end) {
    int middle = (begin + end) / 2;
    const string& middle_name = file.children(middle).name();
    int cmp_result = strcmp(middle_name.c_str(), child_name.c_str());
    if (cmp_result == 0)
      return file.children(middle).index();
    if (cmp_result < 0)
      begin = middle + 1;
    else
      end = middle;
  }
  return -1;
}

// Converts a path to an index in archive_. It does this by separating
// the path components and going from root to leaf, finding the
// File message for each component. Index values for children are
// stored in File messages.
int DeltaDiffParser::GetIndexForPath(const string& path) const {
  string cleaned_path = utils::NormalizePath(path, true);
  // strip leading slash
  if (cleaned_path[0] == '/')
    cleaned_path = cleaned_path.c_str() + 1;
  if (cleaned_path.empty())
    return 0;
  string::size_type begin = 0;
  string::size_type end = cleaned_path.find_first_of('/', begin + 1);
  const DeltaArchiveManifest_File* file = &archive_.files(0);
  int file_idx = -1;
  for (;;) {
    string component = cleaned_path.substr(begin, end - begin);
    if (component.empty())
      break;
    // search for component in 'file'
    file_idx = GetIndexOfFileChild(*file, component);
    if (file_idx < 0)
      return file_idx;
    file = &archive_.files(file_idx);
    if (end == string::npos)
      break;
    begin = end + 1;
    end = cleaned_path.find_first_of('/', begin + 1);
  }
  return file_idx;
}

bool DeltaDiffParser::ReadDataVector(off_t offset, off_t length,
                                     std::vector<char>* out) const {
  out->resize(static_cast<vector<char>::size_type>(length));
  int r = pread(fd_, &((*out)[0]), length, offset);
  TEST_AND_RETURN_FALSE_ERRNO(r >= 0);
  return true;
}

bool DeltaDiffParser::CopyDataToFile(off_t offset, off_t length,
                                     bool should_decompress,
                                     const std::string& path) const {
  DirectFileWriter direct_writer;
  GzipDecompressingFileWriter decompressing_writer(&direct_writer);
  FileWriter* writer = NULL;  // will point to one of the two writers above

  writer = (should_decompress ?
            static_cast<FileWriter*>(&decompressing_writer) :
            static_cast<FileWriter*>(&direct_writer));
  ScopedFileWriterCloser closer(writer);

  int r = writer->Open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
  TEST_AND_RETURN_FALSE(r == 0);

  off_t bytes_transferred = 0;

  while (bytes_transferred < length) {
    char buf[kCopyFileBufferSize];
    size_t bytes_to_read = min(length - bytes_transferred,
                               static_cast<off_t>(sizeof(buf)));
    ssize_t bytes_read = pread(fd_, buf, bytes_to_read,
                               offset + bytes_transferred);
    if (bytes_read == 0)
      break;  // EOF
    TEST_AND_RETURN_FALSE_ERRNO(bytes_read > 0);
    int bytes_written = writer->Write(buf, bytes_read);
    TEST_AND_RETURN_FALSE(bytes_written == bytes_read);
    bytes_transferred += bytes_written;
  }
  TEST_AND_RETURN_FALSE(bytes_transferred == length);
  LOG_IF(ERROR, bytes_transferred > length) << "Wrote too many bytes(?)";
  return true;
}


const DeltaDiffParser::Iterator DeltaDiffParser::Begin() {
  DeltaDiffParserIterator ret(&archive_);
  ret.path_indices_.push_back(0);
  return ret;
}

const DeltaDiffParser::Iterator DeltaDiffParser::End() {
  return DeltaDiffParserIterator(&archive_);
}

}  // namespace chromeos_update_engine
