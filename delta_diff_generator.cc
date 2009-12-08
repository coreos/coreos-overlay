// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_diff_generator.h"
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <tr1/memory>
#include <zlib.h>
#include "chromeos/obsolete_logging.h"
#include "base/scoped_ptr.h"
#include "update_engine/delta_diff_parser.h"
#include "update_engine/gzip.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::vector;
using std::tr1::shared_ptr;
using chromeos_update_engine::DeltaArchiveManifest;

namespace chromeos_update_engine {

namespace {
// These structs and methods are helpers for EncodeDataToDeltaFile()

// Before moving the data into a proto buffer, the data is stored in
// memory in these Node and Child structures.

// Each Node struct represents a file on disk (which can be regular file,
// directory, fifo, socket, symlink, etc). Nodes that contain children
// (just directories) will have a vector of Child objects. Each child
// object has a filename and a pointer to the associated Node. Thus,
// filenames for files are stored with their parents, not as part of
// the file itself.

// These structures are easier to work with than the protobuf format
// when adding files. When generating a delta file, we add an entry
// for each file to a root Node object. Then, we sort each Node's
// children vector so the children are stored alphabetically. Then,
// we assign an index value to the idx field of each Node by a preorder
// tree traversal. The index value assigned to a Node is the index it
// will have in the DeltaArchiveManifest protobuf.
// Finally, we add each Node to a DeltaArchiveManifest protobuf.

struct Node;

struct Child {
  Child(const string& the_name,
        Node* the_node)
      : name(the_name),
        node(the_node) {}
  string name;
  // Use shared_ptr here rather than scoped_ptr b/c this struct will be copied
  // in stl containers
  scoped_ptr<Node> node;
};

// For the C++ sort() function.
struct ChildLessThan {
  bool operator()(const shared_ptr<Child>& a, const shared_ptr<Child>& b) {
    return a->name < b->name;
  }
};

struct Node {
  Node()
      : mode(0),
        uid(0),
        gid(0),
        nlink(0),
        inode(0),
        compressed(false),
        offset(-1),
        length(0),
        idx(0) {}

  mode_t mode;
  uid_t uid;
  gid_t gid;

  // a file may be a potential hardlink if it's not a directory
  // and it has a link count > 1.
  bool IsPotentialHardlink() const {
    return !S_ISDIR(mode) && nlink > 1;
  }
  nlink_t nlink;  // number of hard links
  ino_t inode;

  // data
  bool compressed;
  int offset;  // -1 means no data
  int length;

  vector<shared_ptr<Child> > children;
  int idx;
};

// This function sets *node's variables to match what's at path.
// This includes calling this function recursively on all children. Children
// not on the same device as the original node will not be considered.
// Returns true on success.
bool UpdateNodeFromPath(const string& path, Node* node) {
  // Set metadata
  struct stat stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(lstat(path.c_str(), &stbuf) == 0);
  const dev_t dev = stbuf.st_dev;
  node->mode = stbuf.st_mode;
  node->uid = stbuf.st_uid;
  node->gid = stbuf.st_gid;
  node->nlink = stbuf.st_nlink;
  node->inode = stbuf.st_ino;
  if (!S_ISDIR(node->mode)) {
    return true;
  }

  DIR* dir = opendir(path.c_str());
  TEST_AND_RETURN_FALSE(dir);

  struct dirent entry;
  struct dirent* dir_entry;

  for (;;) {
    TEST_AND_RETURN_FALSE_ERRNO(readdir_r(dir, &entry, &dir_entry) == 0);
    if (!dir_entry) {
      // done
      break;
    }
    if (!strcmp(".", dir_entry->d_name))
      continue;
    if (!strcmp("..", dir_entry->d_name))
      continue;

    string child_path = path + "/" + dir_entry->d_name;
    struct stat child_stbuf;
    TEST_AND_RETURN_FALSE_ERRNO(lstat(child_path.c_str(), &child_stbuf) == 0);
    // make sure it's on the same dev
    if (child_stbuf.st_dev != dev)
      continue;
    shared_ptr<Child> child(new Child(dir_entry->d_name, new Node));
    node->children.push_back(child);
    TEST_AND_RETURN_FALSE(UpdateNodeFromPath(path + "/" + child->name,
                                             child->node.get()));
  }
  TEST_AND_RETURN_FALSE_ERRNO(closedir(dir) == 0);
  // Done with all subdirs. sort children.
  sort(node->children.begin(), node->children.end(), ChildLessThan());
  return true;
}

// We go through n setting the index value of each Node to
// *next_index_value, then increment next_index_value.
// We then recursively assign index values to children.
// The first caller should call this with *next_index_value == 0 and
// the root Node, thus setting the root Node's index to 0.
void PopulateChildIndexes(Node* n, int* next_index_value) {
  n->idx = (*next_index_value)++;
  for (unsigned int i = 0; i < n->children.size(); i++) {
    PopulateChildIndexes(n->children[i]->node.get(), next_index_value);
  }
}

// This converts a Node tree rooted at n into a DeltaArchiveManifest.
void NodeToDeltaArchiveManifest(Node* n, DeltaArchiveManifest* archive,
                                map<ino_t, string>* hard_links,
                                const string& path) {
  DeltaArchiveManifest_File *f = archive->add_files();
  f->set_mode(n->mode);
  f->set_uid(n->uid);
  f->set_gid(n->gid);
  if (utils::MapContainsKey(*hard_links, n->inode)) {
    // We have a hard link
    CHECK(!S_ISDIR(n->mode));
    f->set_hardlink_path((*hard_links)[n->inode]);
  } else if (n->IsPotentialHardlink()) {
    (*hard_links)[n->inode] = path;
  }
  if (!S_ISDIR(n->mode))
    return;
  for (unsigned int i = 0; i < n->children.size(); i++) {
    DeltaArchiveManifest_File_Child* child = f->add_children();
    child->set_name(n->children[i]->name);
    child->set_index(n->children[i]->node->idx);
  }
  for (unsigned int i = 0; i < n->children.size(); i++) {
    NodeToDeltaArchiveManifest(n->children[i]->node.get(), archive, hard_links,
                               path + "/" + n->children[i]->name);
  }
}

}  // namespace {}

// For each file in archive, write a delta for it into out_file
// and update 'file' to refer to the delta.
// This is a recursive function. Returns true on success.
bool DeltaDiffGenerator::WriteFileDiffsToDeltaFile(
    DeltaArchiveManifest* archive,
    DeltaArchiveManifest_File* file,
    const std::string& file_name,
    const std::string& old_path,
    const std::string& new_path,
    FileWriter* out_file_writer,
    int* out_file_length,
    const std::string& force_compress_dev_path) {
  TEST_AND_RETURN_FALSE(file->has_mode());

  // Stat the actual file, too
  struct stat stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(lstat((new_path + "/" + file_name).c_str(),
                                    &stbuf) == 0);
  TEST_AND_RETURN_FALSE(stbuf.st_mode == file->mode());

  // See if we're a directory or not
  if (S_ISDIR(file->mode())) {
    for (int i = 0; i < file->children_size(); i++) {
      DeltaArchiveManifest_File_Child* child = file->mutable_children(i);
      DeltaArchiveManifest_File* child_file =
          archive->mutable_files(child->index());
      TEST_AND_RETURN_FALSE(WriteFileDiffsToDeltaFile(
          archive,
          child_file,
          child->name(),
          old_path + "/" + file_name,
          new_path + "/" + file_name,
          out_file_writer,
          out_file_length,
          force_compress_dev_path));
    }
    return true;
  }

  if (S_ISFIFO(file->mode()) || S_ISSOCK(file->mode()) ||
      file->has_hardlink_path()) {
    // These don't store any additional data
    return true;
  }

  vector<char> data;
  bool should_compress = true;
  bool format_set = false;
  DeltaArchiveManifest_File_DataFormat format;
  if (S_ISLNK(file->mode())) {
    TEST_AND_RETURN_FALSE(EncodeLink(new_path + "/" + file_name, &data));
  } else if (S_ISCHR(file->mode()) || S_ISBLK(file->mode())) {
    TEST_AND_RETURN_FALSE(EncodeDev(stbuf, &data, &format,
                                    new_path + "/" + file_name ==
                                    force_compress_dev_path));
    format_set = true;
  } else if (S_ISREG(file->mode())) {
    // regular file. We may use a delta here.
    TEST_AND_RETURN_FALSE(EncodeFile(old_path, new_path, file_name, &format,
                                     &data));
    should_compress = false;
    format_set = true;
    if ((format == DeltaArchiveManifest_File_DataFormat_BSDIFF) ||
        (format == DeltaArchiveManifest_File_DataFormat_FULL_GZ))
      TEST_AND_RETURN_FALSE(!data.empty());
  } else {
    // Should never get here; unhandled mode type.
    LOG(ERROR) << "Unhandled mode type: " << file->mode();
    return false;
  }

  if (!format_set) {
    // Pick a format now
    vector<char> compressed_data;
    TEST_AND_RETURN_FALSE(GzipCompress(data, &compressed_data));
    if (compressed_data.size() < data.size()) {
      format = DeltaArchiveManifest_File_DataFormat_FULL_GZ;
      data.swap(compressed_data);
    } else {
      format = DeltaArchiveManifest_File_DataFormat_FULL;
    }
    format_set = true;
  }

  if (!data.empty()) {
    TEST_AND_RETURN_FALSE(format_set);
    file->set_data_format(format);
    file->set_data_offset(*out_file_length);
    TEST_AND_RETURN_FALSE(static_cast<ssize_t>(data.size()) ==
                          out_file_writer->Write(&data[0], data.size()));
    file->set_data_length(data.size());
    *out_file_length += data.size();
  }
  return true;
}

bool DeltaDiffGenerator::EncodeLink(const std::string& path,
                                    std::vector<char>* out) {
  // Store symlink path as file data
  vector<char> link_data(4096);
  int rc = readlink(path.c_str(), &link_data[0], link_data.size());
  TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
  link_data.resize(rc);
  out->swap(link_data);
  return true;
}

bool DeltaDiffGenerator::EncodeDev(
    const struct stat& stbuf,
    vector<char>* out,
    DeltaArchiveManifest_File_DataFormat* format,
    bool force_compression) {
  LinuxDevice dev;
  dev.set_major(major(stbuf.st_rdev));
  dev.set_minor(minor(stbuf.st_rdev));
  out->resize(dev.ByteSize());
  TEST_AND_RETURN_FALSE(dev.SerializeToArray(&(*out)[0], out->size()));
  if (force_compression) {
    vector<char> compressed;
    TEST_AND_RETURN_FALSE(GzipCompress(*out, &compressed));
    out->swap(compressed);
    *format = DeltaArchiveManifest_File_DataFormat_FULL_GZ;
  } else {
    *format = DeltaArchiveManifest_File_DataFormat_FULL;
  }
  return true;
}

// Encode the file at new_path + "/" + file_name. It may be a binary diff
// based on old_path + "/" + file_name. out_data_format will be set to
// the format used. out_data_format may not be NULL.
bool DeltaDiffGenerator::EncodeFile(
    const string& old_dir,
    const string& new_dir,
    const string& file_name,
    DeltaArchiveManifest_File_DataFormat* out_data_format,
    vector<char>* out) {
  TEST_AND_RETURN_FALSE(out_data_format);
  // First, see the full length:
  vector<char> full_data;
  TEST_AND_RETURN_FALSE(utils::ReadFile(new_dir + "/" + file_name, &full_data));
  vector<char> gz_data;
  if (!full_data.empty()) {
    TEST_AND_RETURN_FALSE(GzipCompress(full_data, &gz_data));
  }
  vector<char> *ret = NULL;

  if (gz_data.size() < full_data.size()) {
    *out_data_format = DeltaArchiveManifest_File_DataFormat_FULL_GZ;
    ret = &gz_data;
  } else {
    *out_data_format = DeltaArchiveManifest_File_DataFormat_FULL;
    ret = &full_data;
  }

  struct stat old_stbuf;
  if ((stat((old_dir + "/" + file_name).c_str(), &old_stbuf) < 0) ||
      (!S_ISREG(old_stbuf.st_mode))) {
    // stat() failed or old file is not a regular file. Just send back the full
    // contents
    *out = *ret;
    return true;
  }
  // We have an old file. Do a binary diff. For now use bsdiff.
  const string kPatchFile = "/tmp/delta.patch";

  vector<string> cmd;
  cmd.push_back("/usr/bin/bsdiff");
  cmd.push_back(old_dir + "/" + file_name);
  cmd.push_back(new_dir + "/" + file_name);
  cmd.push_back(kPatchFile);

  int rc = 1;
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &rc));
  TEST_AND_RETURN_FALSE(rc == 0);
  vector<char> patch_file;
  TEST_AND_RETURN_FALSE(utils::ReadFile(kPatchFile, &patch_file));
  unlink(kPatchFile.c_str());

  if (patch_file.size() < ret->size()) {
    *out_data_format = DeltaArchiveManifest_File_DataFormat_BSDIFF;
    ret = &patch_file;
  }

  *out = *ret;
  return true;
}

DeltaArchiveManifest* DeltaDiffGenerator::EncodeMetadataToProtoBuffer(
    const char* new_path) {
  Node node;
  if (!UpdateNodeFromPath(new_path, &node))
    return NULL;
  int index = 0;
  PopulateChildIndexes(&node, &index);
  DeltaArchiveManifest *ret = new DeltaArchiveManifest;
  map<ino_t, string> hard_links;  // inode -> first found path for inode
  NodeToDeltaArchiveManifest(&node, ret, &hard_links, "");
  return ret;
}

bool DeltaDiffGenerator::EncodeDataToDeltaFile(
    DeltaArchiveManifest* archive,
    const std::string& old_path,
    const std::string& new_path,
    const std::string& out_file,
    const std::string& force_compress_dev_path) {
  DirectFileWriter out_writer;
  int r = out_writer.Open(out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  TEST_AND_RETURN_FALSE_ERRNO(r >= 0);
  ScopedFileWriterCloser closer(&out_writer);
  TEST_AND_RETURN_FALSE(out_writer.Write(DeltaDiffParser::kFileMagic,
                                         strlen(DeltaDiffParser::kFileMagic))
                        == static_cast<ssize_t>(
                            strlen(DeltaDiffParser::kFileMagic)));
  // Write 8 null bytes. This will be filled in w/ the offset of
  // the protobuf.
  TEST_AND_RETURN_FALSE(out_writer.Write("\0\0\0\0\0\0\0\0", 8) == 8);
  // 8 more bytes will be filled w/ the protobuf length.
  TEST_AND_RETURN_FALSE(out_writer.Write("\0\0\0\0\0\0\0\0", 8) == 8);
  int out_file_length = strlen(DeltaDiffParser::kFileMagic) + 16;

  TEST_AND_RETURN_FALSE(archive->files_size() > 0);
  DeltaArchiveManifest_File* file = archive->mutable_files(0);

  TEST_AND_RETURN_FALSE(WriteFileDiffsToDeltaFile(archive,
                                                  file,
                                                  "",
                                                  old_path,
                                                  new_path,
                                                  &out_writer,
                                                  &out_file_length,
                                                  force_compress_dev_path));

  // Finally, write the protobuf to the end of the file
  string encoded_archive;
  TEST_AND_RETURN_FALSE(archive->SerializeToString(&encoded_archive));

  // Compress the protobuf (which contains filenames)
  vector<char> compressed_encoded_archive;
  TEST_AND_RETURN_FALSE(GzipCompressString(encoded_archive,
                                           &compressed_encoded_archive));

  TEST_AND_RETURN_FALSE(out_writer.Write(compressed_encoded_archive.data(),
                                         compressed_encoded_archive.size()) ==
                        static_cast<ssize_t>(
                            compressed_encoded_archive.size()));

  // write offset of protobut to just after the file magic
  int64 big_endian_protobuf_offset = htobe64(out_file_length);
  TEST_AND_RETURN_FALSE(pwrite(out_writer.fd(),
                               &big_endian_protobuf_offset,
                               sizeof(big_endian_protobuf_offset),
                               strlen(DeltaDiffParser::kFileMagic)) ==
                        sizeof(big_endian_protobuf_offset));
  // Write the size just after the offset
  int64 pb_length = htobe64(compressed_encoded_archive.size());
  TEST_AND_RETURN_FALSE(pwrite(out_writer.fd(),
                               &pb_length,
                               sizeof(pb_length),
                               strlen(DeltaDiffParser::kFileMagic) +
                               sizeof(big_endian_protobuf_offset)) ==
                        sizeof(pb_length));
  return true;
}

}  // namespace chromeos_update_engine
