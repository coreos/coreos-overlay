// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__

#include "base/file_path.h"

#include "update_engine/action_processor.h"
#include "update_engine/prefs_interface.h"

namespace chromeos_update_engine {

// Forward declaration here because we get a circular dependency if
// we include omaha_request_action.h directly.
struct OmahaResponse;

// Encapsulates all the payload state required for download. This includes the
// state necessary for handling multiple URLs in Omaha response, the back-off
// state, etc. All state is persisted so that we use the most recently saved
// value when resuming the update_engine process. All state is also cached in
// memory so that we ensure we always make progress based on last known good
// state even when there's any issue in reading/writing from the file system.
class PayloadState {
 public:

  PayloadState() : prefs_(NULL), num_urls_(0), url_index_(0) {}

  // Initializes a payload state object using |prefs| for storing the
  // persisted state. It also performs the initial loading of all persisted
  // state into memory and dumps the initial state for debugging purposes.
  // Note: the other methods should be called only after calling Initialize
  // on this object.
  bool Initialize(PrefsInterface* prefs);

  // Logs the current payload state.
  void LogPayloadState();

  // Sets the internal payload state based on the given Omaha response. This
  // response could be the same or different from the one for which we've stored
  // the internal state. If it's different, then this method resets all the
  // internal state corresponding to the old response. Since the Omaha response
  // has a lot of fields that are not related to payload state, it uses only
  // a subset of the fields in the Omaha response to compare equality.
  void SetResponse(const OmahaResponse& response);

  // Updates the payload state when the current update attempt has failed.
  void UpdateFailed(ActionExitCode error);

  // Returns the internally stored subset of the response state as a string.
  // This is logically a private method, but exposed here for unit tests.
  std::string GetResponse() {
    return response_;
  }

  // Returns the current URL index.
  uint32_t GetUrlIndex() {
    return url_index_;
  }

 private:
  // Sets the stored response_ value from the currently persisted value for
  // the response. Returns the same value.
  std::string LoadResponse();

  // Sets the url_index_ value from the currently persisted value for
  // URL index. Returns the same value.
  uint32_t LoadUrlIndex();

  // Sets the current URL index.
  void SetUrlIndex(uint32_t url_index);

  // Interface object with which we read/write persisted state. This must
  // be set by calling the Initialize method before calling any other method.
  PrefsInterface* prefs_;

  // Cached value of the latest subset of the Omaha response with which we're
  // working off currently. This value is persisted so we load it off the next
  // time when update_engine restarts. The rest of the state in this class will
  // be cleared when we set a new  response.
  std::string response_;

  // The number of urls in the current response. Not persisted.
  uint32_t num_urls_;

  // Cached value of the index of the current URL, to be used in case we are
  // unable to read from the persisted store for any reason. This type is
  // different from the one in the accessor methods because PrefsInterface
  // supports only int64_t but we want to provide a stronger abstraction of
  // uint32_t.
  int64_t url_index_;

  DISALLOW_COPY_AND_ASSIGN(PayloadState);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__
