// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_PARSER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_PARSER_H__

#include <string>
#include <vector>
#include "chromeos/obsolete_logging.h"
#include "base/basictypes.h"
#include "update_engine/update_metadata.pb.h"

// The DeltaDiffParser class is used to parse a delta file on disk. It will
// copy the metadata into memory, but not the file data. This class can
// also be used to copy file data out to disk.

// The DeltaDiffParserIterator class is used to iterate through the
// metadata of a delta file. It will return directories before their
// children.

namespace chromeos_update_engine {

class DeltaDiffParser;

class DeltaDiffParserIterator {
  friend class DeltaDiffParser;
 public:
  void Increment();

  // Returns the full path for the current file, e.g. "/bin/bash".
  // Returns empty string for root.
  const std::string& path() const {
    return path_;
  }

  // Returns the basename for the current file. If path() returns
  // "/bin/bash", then GetName() returns "bash".
  // Returns empty string for root
  const std::string GetName() const;

  const DeltaArchiveManifest_File& GetFile() const;
  bool operator==(const DeltaDiffParserIterator& that) const {
    return path_indices_ == that.path_indices_ &&
        child_indices_ == that.child_indices_ &&
        path_ == that.path_ &&
        archive_ == that.archive_;
  }
  bool operator!=(const DeltaDiffParserIterator& that) const {
    return !(*this == that);
  }
 private:
  // Container of all the File messages. Each File message has an index
  // in archive_. The root directory is always stored at index 0.
  const DeltaArchiveManifest* archive_;
  
  // These variables are used to implement the common recursive depth-first
  // search algorithm (which we can't use here, since we need to walk the
  // tree incrementally).
  
  // Indices into 'archive_' of the current path components.  For example, if
  // the current path is "/bin/bash", 'path_stack_' will contain the archive
  // indices for "/", "/bin", and "/bin/bash", in that order.  This is
  // analogous to the call stack of the recursive algorithm.
  std::vector<int> path_indices_;

  // For each component in 'path_stack_', the currently-selected child in its
  // child vector.  In the previous example, if "/" has "abc" and "bin"
  // subdirectories and "/bin" contains only "bash", this will contain
  // [0, 1, 0], since we are using the 0th child at the root directory level
  // (there's only one child there), the first of the root dir's children
  // ("bin"), and the 0th child of /bin ("bash").  This is analogous to the
  // state of each function (in terms of which child it's currently
  // handling) in the call stack of the recursive algorithm.
  std::vector<int> child_indices_;

  std::string path_;
  // Instantiated by friend class DeltaDiffParser
  explicit DeltaDiffParserIterator(const DeltaArchiveManifest* archive)
      : archive_(archive) {}
  DeltaDiffParserIterator() {
    CHECK(false);  // Should never be called.
  }
};

class DeltaDiffParser {
 public:
  DeltaDiffParser(const std::string& delta_file);
  ~DeltaDiffParser();
  bool valid() const { return valid_; }
  bool ContainsPath(const std::string& path) const;
  const DeltaArchiveManifest_File& GetFileAtPath(const std::string& path) const;

  // Reads length bytes at offset of the delta file into the out string
  // or vector. Be careful not to call this with large length values,
  // since that much memory will have to be allocated to store the output.
  // Returns true on success.
  bool ReadDataVector(off_t offset, off_t length, std::vector<char>* out) const;

  // Copies length bytes of data from offset into a new file at path specified.
  // If should_decompress is true, will gzip decompress while writing to the
  // file. Returns true on success.
  bool CopyDataToFile(off_t offset, off_t length, bool should_decompress,
                      const std::string& path) const;

  typedef DeltaDiffParserIterator Iterator;
  const Iterator Begin();
  const Iterator End();

  // The identifier we expect at the beginning of a delta file.
  static const char* const kFileMagic;

 private:
  // (Binary) Searches the children of 'file' for one named child_name.
  // If found, returns the index into the archive. If not found, returns -1.
  int GetIndexOfFileChild(const DeltaArchiveManifest_File& file,
                            const std::string& child_name) const;

  // Returns -1 if not found, 0 for root
  int GetIndexForPath(const std::string& path) const;

  // We keep a filedescriptor open to the delta file.
  int fd_;

  DeltaArchiveManifest archive_;
  bool valid_;
  DISALLOW_COPY_AND_ASSIGN(DeltaDiffParser);
};

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_PARSER_H__
