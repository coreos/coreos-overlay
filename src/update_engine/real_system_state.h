// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_REAL_SYSTEM_STATE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_REAL_SYSTEM_STATE_H_

#include <update_engine/system_state.h>

#include <update_engine/payload_state.h>
#include <update_engine/prefs.h>
#include <update_engine/update_attempter.h>

namespace chromeos_update_engine {

// A real implementation of the SystemStateInterface which is
// used by the actual product code.
class RealSystemState : public SystemState {
public:
  // Constructors and destructors.
  RealSystemState();
  virtual ~RealSystemState() {}

  virtual inline PrefsInterface* prefs() {
    return &prefs_;
  }

  virtual inline PayloadStateInterface* payload_state() {
    return &payload_state_;
  }

  virtual inline UpdateAttempter* update_attempter() {
    return update_attempter_.get();
  }

  // Returns a pointer to the object that stores the parameters that are
  // common to all Omaha requests.
  virtual inline OmahaRequestParams* request_params() {
    return &request_params_;
  }

  // Initializes this concrete object. Other methods should be invoked only
  // if the object has been initialized successfully.
  bool Initialize();

private:
  // Interface for persisted store.
  Prefs prefs_;

  // All state pertaining to payload state such as
  // response, URL, backoff states.
  PayloadState payload_state_;

  // The dbus object used to initialize the update attempter.
  ConcreteDbusGlib dbus_;

  // Pointer to the update attempter object.
  scoped_ptr<UpdateAttempter> update_attempter_;

  // Common parameters for all Omaha requests.
  OmahaRequestParams request_params_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_REAL_SYSTEM_STATE_H_
