// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/full_update_generator.h"

#include <inttypes.h>
#include <fcntl.h>

#include <tr1/memory>

#include <base/string_util.h>

#include "strings/string_printf.h"
#include "update_engine/bzip.h"
#include "update_engine/utils.h"

using std::deque;
using std::min;
using std::max;
using std::string;
using std::tr1::shared_ptr;
using std::vector;
using strings::StringPrintf;

namespace chromeos_update_engine {

namespace {

// This class encapsulates a full update chunk processing thread. The processor
// reads a chunk of data from the input file descriptor and compresses it. The
// processor needs to be started through Start() then waited on through Wait().
class ChunkProcessor {
 public:
  // Read a chunk of |size| bytes from |fd| starting at offset |offset|.
  ChunkProcessor(int fd, off_t offset, size_t size)
      : thread_(NULL),
        fd_(fd),
        offset_(offset),
        buffer_in_(size) {}
  ~ChunkProcessor() { Wait(); }

  off_t offset() const { return offset_; }
  const vector<char>& buffer_in() const { return buffer_in_; }
  const vector<char>& buffer_compressed() const { return buffer_compressed_; }

  // Starts the processor. Returns true on success, false on failure.
  bool Start();

  // Waits for the processor to complete. Returns true on success, false on
  // failure.
  bool Wait();

  bool ShouldCompress() const {
    return buffer_compressed_.size() < buffer_in_.size();
  }

 private:
  // Reads the input data into |buffer_in_| and compresses it into
  // |buffer_compressed_|. Returns true on success, false otherwise.
  bool ReadAndCompress();
  static gpointer ReadAndCompressThread(gpointer data);

  GThread* thread_;
  int fd_;
  off_t offset_;
  vector<char> buffer_in_;
  vector<char> buffer_compressed_;

  DISALLOW_COPY_AND_ASSIGN(ChunkProcessor);
};

bool ChunkProcessor::Start() {
  // g_thread_create is deprecated since glib 2.32. Use
  // g_thread_new instead.
  thread_ = g_thread_try_new("chunk_proc", ReadAndCompressThread, this, NULL);
  TEST_AND_RETURN_FALSE(thread_ != NULL);
  return true;
}

bool ChunkProcessor::Wait() {
  if (!thread_) {
    return false;
  }
  gpointer result = g_thread_join(thread_);
  thread_ = NULL;
  TEST_AND_RETURN_FALSE(result == this);
  return true;
}

gpointer ChunkProcessor::ReadAndCompressThread(gpointer data) {
  return
      reinterpret_cast<ChunkProcessor*>(data)->ReadAndCompress() ? data : NULL;
}

bool ChunkProcessor::ReadAndCompress() {
  ssize_t bytes_read = -1;
  TEST_AND_RETURN_FALSE(utils::PReadAll(fd_,
                                        buffer_in_.data(),
                                        buffer_in_.size(),
                                        offset_,
                                        &bytes_read));
  TEST_AND_RETURN_FALSE(bytes_read == static_cast<ssize_t>(buffer_in_.size()));
  TEST_AND_RETURN_FALSE(BzipCompress(buffer_in_, &buffer_compressed_));
  return true;
}

}  // namespace

bool FullUpdateGenerator::Run(
    Graph* graph,
    const std::string& new_kernel_part,
    const std::string& new_image,
    off_t image_size,
    int fd,
    off_t* data_file_size,
    off_t chunk_size,
    off_t block_size,
    vector<DeltaArchiveManifest_InstallOperation>* kernel_ops,
    std::vector<Vertex::Index>* final_order) {
  TEST_AND_RETURN_FALSE(chunk_size > 0);
  TEST_AND_RETURN_FALSE((chunk_size % block_size) == 0);

  size_t max_threads = max(sysconf(_SC_NPROCESSORS_ONLN), 4L);
  LOG(INFO) << "Max threads: " << max_threads;

  // Get the sizes early in the function, so we can fail fast if the user
  // passed us bad paths.
  TEST_AND_RETURN_FALSE(image_size >= 0 &&
                        image_size <= utils::FileSize(new_image));
  // Always do the root image, the kernel may not be used in all cases
  int partitions = 1;

  off_t kernel_size = 0;
  if (!new_kernel_part.empty()) {
    kernel_size = utils::FileSize(new_kernel_part);
    partitions++;
  }

  off_t part_sizes[] = { image_size, kernel_size };
  string paths[] = { new_image, new_kernel_part };

  for (int partition = 0; partition < partitions; ++partition) {
    const string& path = paths[partition];
    LOG(INFO) << "compressing " << path;
    int in_fd = open(path.c_str(), O_RDONLY, 0);
    TEST_AND_RETURN_FALSE(in_fd >= 0);
    ScopedFdCloser in_fd_closer(&in_fd);
    deque<shared_ptr<ChunkProcessor> > threads;
    int last_progress_update = INT_MIN;
    off_t bytes_left = part_sizes[partition], counter = 0, offset = 0;
    while (bytes_left > 0 || !threads.empty()) {
      // Check and start new chunk processors if possible.
      while (threads.size() < max_threads && bytes_left > 0) {
        shared_ptr<ChunkProcessor> processor(
            new ChunkProcessor(in_fd, offset, min(bytes_left, chunk_size)));
        threads.push_back(processor);
        TEST_AND_RETURN_FALSE(processor->Start());
        bytes_left -= chunk_size;
        offset += chunk_size;
      }

      // Need to wait for a chunk processor to complete and process its ouput
      // before spawning new processors.
      shared_ptr<ChunkProcessor> processor = threads.front();
      threads.pop_front();
      TEST_AND_RETURN_FALSE(processor->Wait());

      DeltaArchiveManifest_InstallOperation* op = NULL;
      if (partition == 0) {
        graph->resize(graph->size() + 1);
        graph->back().file_name =
            StringPrintf("<rootfs-operation-%" PRIi64 ">", counter++);
        op = &graph->back().op;
        final_order->push_back(graph->size() - 1);
      } else {
        kernel_ops->resize(kernel_ops->size() + 1);
        op = &kernel_ops->back();
      }

      const bool compress = processor->ShouldCompress();
      const vector<char>& use_buf =
          compress ? processor->buffer_compressed() : processor->buffer_in();
      op->set_type(compress ?
                   DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ :
                   DeltaArchiveManifest_InstallOperation_Type_REPLACE);
      op->set_data_offset(*data_file_size);
      TEST_AND_RETURN_FALSE(utils::WriteAll(fd, &use_buf[0], use_buf.size()));
      *data_file_size += use_buf.size();
      op->set_data_length(use_buf.size());
      Extent* dst_extent = op->add_dst_extents();
      dst_extent->set_start_block(processor->offset() / block_size);
      dst_extent->set_num_blocks(chunk_size / block_size);

      int progress = static_cast<int>(
          (processor->offset() + processor->buffer_in().size()) * 100.0 /
          part_sizes[partition]);
      if (last_progress_update < progress &&
          (last_progress_update + 10 <= progress || progress == 100)) {
        LOG(INFO) << progress << "% complete (output size: "
                  << *data_file_size << ")";
        last_progress_update = progress;
      }
    }
  }

  return true;
}

}  // namespace chromeos_update_engine
