// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/install_action.h"
#include <errno.h>
#include <vector>
#include <gflags/gflags.h>
#include "update_engine/filesystem_iterator.h"
#include "update_engine/gzip.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

DEFINE_string(mount_install_path, "",
              "If set, the path to use when mounting the "
              "destination device during install");

using std::vector;

namespace chromeos_update_engine {

namespace {
const string kBspatchPath = "/usr/bin/bspatch";
}

void InstallAction::PerformAction() {
  ScopedActionCompleter completer(processor_, this);
  // For now, do nothing other than pass what we need to to the output pipe
  CHECK(HasInputObject());
  const InstallPlan install_plan = GetInputObject();
  if (HasOutputPipe())
    SetOutputObject(install_plan.install_path);
  if (install_plan.is_full_update) {
    // No need to perform an install
    completer.set_success(true);
    return;
  }
  // We have a delta update.

  // Open delta file
  DeltaDiffParser parser(install_plan.download_path);
  if (!parser.valid()) {
    LOG(ERROR) << "Unable to open delta file";
    return;
  }

  // Mount install fs
  string mountpoint = FLAGS_mount_install_path;
  if (mountpoint.empty()) {
    // Set up dest_path_
    char *mountpoint_temp = strdup("/tmp/install_mnt.XXXXXX");
    CHECK(mountpoint_temp);
    CHECK_EQ(mountpoint_temp, mkdtemp(mountpoint_temp));
    CHECK_NE('\0', mountpoint_temp[0]);
    mountpoint = mountpoint_temp;
    free(mountpoint_temp);
  }

  TEST_AND_RETURN(utils::MountFilesystem(install_plan.install_path,
                                         mountpoint));

  // Automatically unmount the fs when this goes out of scope:
  ScopedFilesystemUnmounter filesystem_unmounter(mountpoint);

  {
    // Iterate through existing fs, deleting unneeded files
    // Delete files that don't exist in the update, or exist but are
    // hard links.
    FilesystemIterator iter(mountpoint,
                            utils::SetWithValue<string>("/lost+found"));
    for (; !iter.IsEnd(); iter.Increment()) {
      if (!parser.ContainsPath(iter.GetPartialPath()) ||
          parser.GetFileAtPath(iter.GetPartialPath()).has_hardlink_path()) {
        VLOG(1) << "install removing local path: " << iter.GetFullPath();
        TEST_AND_RETURN(utils::RecursiveUnlinkDir(iter.GetFullPath()));
      }
    }
    TEST_AND_RETURN(!iter.IsErr());
  }

  // iterate through delta metadata, writing files
  DeltaDiffParserIterator iter = parser.Begin();
  for (; iter != parser.End(); iter.Increment()) {
    const DeltaArchiveManifest_File& file = iter.GetFile();
    VLOG(1) << "Installing file: " << iter.path();
    TEST_AND_RETURN(InstallFile(mountpoint, file, iter.path(), parser));
  }

  completer.set_success(true);
}

bool InstallAction::InstallFile(const std::string& mountpoint,
                                const DeltaArchiveManifest_File& file,
                                const std::string& path,
                                const DeltaDiffParser& parser) const {
  // See what's already there
  struct stat existing_stbuf;
  int result = lstat((mountpoint + path).c_str(), &existing_stbuf);
  TEST_AND_RETURN_FALSE_ERRNO((result == 0) || (errno == ENOENT));
  bool exists = (result == 0);
  // Create the proper file
  if (file.has_hardlink_path()) {
    TEST_AND_RETURN_FALSE(file.has_hardlink_path());
    TEST_AND_RETURN_FALSE_ERRNO(link(
        (mountpoint + file.hardlink_path()).c_str(),
        (mountpoint + path).c_str()) == 0);
  } else if (S_ISDIR(file.mode())) {
    if (!exists) {
      TEST_AND_RETURN_FALSE_ERRNO(
          (mkdir((mountpoint + path).c_str(), file.mode())) == 0);
    }
  } else if (S_ISLNK(file.mode())) {
    InstallFileSymlink(mountpoint, file, path, parser, exists);
  } else if (S_ISCHR(file.mode()) ||
             S_ISBLK(file.mode()) ||
             S_ISFIFO(file.mode()) ||
             S_ISSOCK(file.mode())) {
    InstallFileSpecialFile(mountpoint, file, path, parser, exists);
  } else if (S_ISREG(file.mode())) {
    InstallFileRegularFile(mountpoint, file, path, parser, exists);
  } else {
    // unknown mode type
    TEST_AND_RETURN_FALSE(false);
  }

  // chmod/chown new file
  if (!S_ISLNK(file.mode()))
    TEST_AND_RETURN_FALSE_ERRNO(chmod((mountpoint + path).c_str(), file.mode())
                                == 0);
  TEST_AND_RETURN_FALSE(file.has_uid() && file.has_gid());
  TEST_AND_RETURN_FALSE_ERRNO(lchown((mountpoint + path).c_str(),
                                     file.uid(), file.gid()) == 0);
  return true;
}

bool InstallAction::InstallFileRegularFile(
    const std::string& mountpoint,
    const DeltaArchiveManifest_File& file,
    const std::string& path,
    const DeltaDiffParser& parser,
    const bool exists) const {
  if (!file.has_data_format())
    return true;
  TEST_AND_RETURN_FALSE(file.has_data_offset() && file.has_data_length());
  if (file.data_format() == DeltaArchiveManifest_File_DataFormat_BSDIFF) {
    // Expand with bspatch
    string patch_path = utils::TempFilename(mountpoint + path + ".XXXXXX");
    TEST_AND_RETURN_FALSE(file.has_data_length());
    TEST_AND_RETURN_FALSE(parser.CopyDataToFile(
        file.data_offset(),
        static_cast<off_t>(file.data_length()), false,
        patch_path));
    string output_path = utils::TempFilename(mountpoint + path + ".XXXXXX");
    int rc = 1;
    vector<string> cmd;
    cmd.push_back(kBspatchPath);
    cmd.push_back(mountpoint + path);
    cmd.push_back(output_path);
    cmd.push_back(patch_path);
    TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &rc));
    TEST_AND_RETURN_FALSE(rc == 0);
    TEST_AND_RETURN_FALSE_ERRNO(rename(output_path.c_str(),
                                       (mountpoint + path).c_str()) == 0);
    TEST_AND_RETURN_FALSE_ERRNO(unlink(patch_path.c_str()) == 0);
  } else {
    // Expand full data, decompressing if necessary
    TEST_AND_RETURN_FALSE((file.data_format() ==
                           DeltaArchiveManifest_File_DataFormat_FULL) ||
                          (file.data_format() ==
                           DeltaArchiveManifest_File_DataFormat_FULL_GZ));
    if (exists)
      TEST_AND_RETURN_FALSE_ERRNO(unlink((mountpoint + path).c_str()) == 0);
    TEST_AND_RETURN_FALSE(file.has_data_length());
    const bool gzipped = file.data_format() ==
        DeltaArchiveManifest_File_DataFormat_FULL_GZ;
    bool success =
        parser.CopyDataToFile(file.data_offset(), file.data_length(),
                              gzipped,
                              mountpoint + path);
    TEST_AND_RETURN_FALSE(success);
  }
  return true;
}

// char/block devices, fifos, and sockets:
bool InstallAction::InstallFileSpecialFile(
    const std::string& mountpoint,
    const DeltaArchiveManifest_File& file,
    const std::string& path,
    const DeltaDiffParser& parser,
    const bool exists) const {
  if (exists)
    TEST_AND_RETURN_FALSE(unlink((mountpoint + path).c_str()) == 0);
  dev_t dev = 0;
  if (S_ISCHR(file.mode()) || S_ISBLK(file.mode())) {
    vector<char> dev_proto;
    TEST_AND_RETURN_FALSE(file.has_data_format());
    TEST_AND_RETURN_FALSE(parser.ReadDataVector(file.data_offset(),
                                                file.data_length(),
                                                &dev_proto));
    if (file.data_format() == DeltaArchiveManifest_File_DataFormat_FULL_GZ) {
      TEST_AND_RETURN_FALSE(file.has_data_length());
      {
        vector<char> decompressed_dev_proto;
        TEST_AND_RETURN_FALSE(GzipDecompress(dev_proto,
                                             &decompressed_dev_proto));
        dev_proto = decompressed_dev_proto;
      }
    } else {
      TEST_AND_RETURN_FALSE(file.data_format() ==
                            DeltaArchiveManifest_File_DataFormat_FULL);
    }
    LinuxDevice linux_device;
    utils::HexDumpVector(dev_proto);
    TEST_AND_RETURN_FALSE(linux_device.ParseFromArray(&dev_proto[0],
                                                      dev_proto.size()));
    dev = makedev(linux_device.major(), linux_device.minor());
  }
  TEST_AND_RETURN_FALSE_ERRNO(mknod((mountpoint + path).c_str(),
                                    file.mode(), dev) == 0);
  return true;
}
// symlinks:
bool InstallAction::InstallFileSymlink(const std::string& mountpoint,
                                       const DeltaArchiveManifest_File& file,
                                       const std::string& path,
                                       const DeltaDiffParser& parser,
                                       const bool exists) const {
  // If there's no data, we leave the symlink as is
  if (!file.has_data_format())
    return true;  // No changes needed
  TEST_AND_RETURN_FALSE((file.data_format() ==
                         DeltaArchiveManifest_File_DataFormat_FULL) ||
                        (file.data_format() ==
                         DeltaArchiveManifest_File_DataFormat_FULL_GZ));
  TEST_AND_RETURN_FALSE(file.has_data_offset() && file.has_data_length());
  // We have data, and thus use it to create a symlink.
  // First delete any existing symlink:
  if (exists)
    TEST_AND_RETURN_FALSE_ERRNO(unlink((mountpoint + path).c_str()) == 0);
  vector<char> symlink_data;
  TEST_AND_RETURN_FALSE(parser.ReadDataVector(file.data_offset(),
                                              file.data_length(),
                                              &symlink_data));
  if (file.data_format() == DeltaArchiveManifest_File_DataFormat_FULL_GZ) {
    vector<char> decompressed_symlink_data;
    TEST_AND_RETURN_FALSE(GzipDecompress(symlink_data,
                                         &decompressed_symlink_data));
    symlink_data = decompressed_symlink_data;
  }
  symlink_data.push_back('\0');
  TEST_AND_RETURN_FALSE_ERRNO(symlink(&symlink_data[0],
                                      (mountpoint + path).c_str()) == 0);
  return true;
}


}  //   namespace chromeos_update_engine
