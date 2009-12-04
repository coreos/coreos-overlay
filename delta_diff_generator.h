// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "update_engine/file_writer.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class DeltaDiffGenerator {
 public:
  // Encodes the metadata at new_path recursively into a DeltaArchiveManifest
  // protobuf object. This will only read the filesystem. Children will
  // be recorded recursively iff they are on the same device as their
  // parent.
  // This will set all fields in the DeltaArchiveManifest except for
  // DeltaArchiveManifest_File_data_* as those are set only when writing
  // the actual delta file to disk.
  // Caller is responsible for freeing the returned value.
  // Returns NULL on failure.
  static DeltaArchiveManifest* EncodeMetadataToProtoBuffer(
      const char* new_path);

  // Takes a DeltaArchiveManifest as given from EncodeMetadataToProtoBuffer(),
  // fill in the missing fields (DeltaArchiveManifest_File_data_*), and
  // write the full delta out to the output file.
  // Returns true on success.
  static bool EncodeDataToDeltaFile(DeltaArchiveManifest* archive,
                                    const std::string& old_path,
                                    const std::string& new_path,
                                    const std::string& out_file);
 private:
  // These functions encode all the data about a file that's not already
  // stored in the DeltaArchiveManifest message into the vector 'out'.
  // They all return true on success.

  // EncodeLink stores the path the symlink points to.
  static bool EncodeLink(const std::string& path, std::vector<char>* out);
  // EncodeDev stores the major and minor device numbers.
  // Specifically it writes a LinuxDevice message.
  static bool EncodeDev(const struct stat& stbuf, std::vector<char>* out);
  // EncodeFile stores the full data, gzipped data, or a binary diff from
  // the old data. out_data_format will be set to the method used.
  static bool EncodeFile(const std::string& old_dir,
                         const std::string& new_dir,
                         const std::string& file_name,
                         DeltaArchiveManifest_File_DataFormat* out_data_format,
                         std::vector<char>* out);

  static bool WriteFileDiffsToDeltaFile(DeltaArchiveManifest* archive,
                                        DeltaArchiveManifest_File* file,
                                        const std::string& file_name,
                                        const std::string& old_path,
                                        const std::string& new_path,
                                        FileWriter* out_file_writer,
                                        int* out_file_length);

  // This should never be constructed
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeltaDiffGenerator);
};

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__
