// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_SYSTEM_STATE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_SYSTEM_STATE_H_

#include <policy/device_policy.h>
#include <policy/libpolicy.h>

#include <update_engine/connection_manager.h>

namespace chromeos_update_engine {

// An interface to global system context, including platform resources,
// the current state of the system, high-level objects, system interfaces, etc.
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
  virtual void SetDevicePolicy(const policy::DevicePolicy* device_policy) = 0;
  virtual const policy::DevicePolicy* GetDevicePolicy() const = 0;

  // Gets the connection manager object.
  virtual ConnectionManager* GetConnectionManager() = 0;
};

// A real implementation of the SystemStateInterface which is
// used by the actual product code.
class RealSystemState : public SystemState {
public:
  // Constructors and destructors.
  RealSystemState();
  virtual ~RealSystemState() {}

  virtual bool IsOOBEComplete();

  virtual void SetDevicePolicy(const policy::DevicePolicy* device_policy);
  virtual const policy::DevicePolicy* GetDevicePolicy() const;

  virtual ConnectionManager* GetConnectionManager();

private:
  // The latest device policy object from the policy provider.
  const policy::DevicePolicy* device_policy_;

  // The connection manager object that makes download
  // decisions depending on the current type of connection.
  ConnectionManager connection_manager_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H_
