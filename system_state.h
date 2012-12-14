// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_SYSTEM_STATE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_SYSTEM_STATE_H_

#include "metrics/metrics_library.h"
#include <policy/device_policy.h>
#include <policy/libpolicy.h>

#include <update_engine/connection_manager.h>
#include <update_engine/payload_state.h>
#include <update_engine/prefs.h>

namespace chromeos_update_engine {

// An interface to global system context, including platform resources,
// the current state of the system, high-level objects whose lifetime is same
// as main, system interfaces, etc.
// Carved out separately so it can be mocked for unit tests.
// Currently it has only one method, but we should start migrating other
// methods to use this as and when needed to unit test them.
// TODO (jaysri): Consider renaming this to something like GlobalContext.
class SystemState {
 public:
  // Destructs this object.
  virtual ~SystemState() {}

  // Returns true if the OOBE process has been completed and EULA accepted.
  // False otherwise.
  virtual bool IsOOBEComplete() = 0;

  // Sets or gets the latest device policy.
  virtual void set_device_policy(const policy::DevicePolicy* device_policy) = 0;
  virtual const policy::DevicePolicy* device_policy() const = 0;

  // Gets the connection manager object.
  virtual ConnectionManager* connection_manager() = 0;

  // Gets the Metrics Library interface for reporting UMA stats.
  virtual MetricsLibraryInterface* metrics_lib() = 0;

  // Gets the interface object for persisted store.
  virtual PrefsInterface* prefs() = 0;

  // Gets the URL State object.
  virtual PayloadState* payload_state() = 0;
};

// A real implementation of the SystemStateInterface which is
// used by the actual product code.
class RealSystemState : public SystemState {
public:
  // Constructors and destructors.
  RealSystemState();
  virtual ~RealSystemState() {}

  virtual bool IsOOBEComplete();

  virtual void set_device_policy(const policy::DevicePolicy* device_policy);
  virtual const policy::DevicePolicy* device_policy() const;

  virtual ConnectionManager* connection_manager();

  virtual MetricsLibraryInterface* metrics_lib();

  virtual PrefsInterface* prefs();

  virtual PayloadState* payload_state();

  // Initializs this concrete object. Other methods should be invoked only
  // if the object has been initialized successfully.
  bool Initialize();

private:
  // The latest device policy object from the policy provider.
  const policy::DevicePolicy* device_policy_;

  // The connection manager object that makes download
  // decisions depending on the current type of connection.
  ConnectionManager connection_manager_;

  // The Metrics Library interface for reporting UMA stats.
  MetricsLibrary metrics_lib_;

  // Interface for persisted store.
  Prefs prefs_;

  // All state pertaining to payload state such as
  // response, URL, back-off states.
  PayloadState payload_state_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H_
