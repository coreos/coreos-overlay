// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UDEV_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UDEV_INTERFACE_H__

#include <libudev.h>

#include "update_engine/utils.h"

// An interface for libudev calls, allowing to easily mock it.

namespace chromeos_update_engine {

// An interface for libudev methods that are being used in update engine.
//
// TODO(garnold) As is, this is a pretty lame indirection layer that otherwise
// does not provide any better abstraction than the existing libudev API. Done
// properly, we should replace it with encapsulated udev, enumerate and device
// objects, and hide initialization, reference handling and iterators in ways
// more appropriate to an object-oriented setting...
class UdevInterface {
 public:
  virtual ~UdevInterface() {}

  // Interfaces for various udev closers. All of these are merely containers for
  // a single pointer to some udev handle, which invoke the provided interface's
  // unref method and nullify the handle upon destruction. It should suffice for
  // derivative (concrete) interfaces to implement the various unref methods to
  // fit their needs, making these closers behave as expected.
  class UdevCloser {
   public:
    explicit UdevCloser(UdevInterface* udev_iface, struct udev** udev_p)
        : udev_iface_(udev_iface), udev_p_(udev_p) {
      CHECK(udev_iface && udev_p);
    }
    virtual ~UdevCloser() {
      if (*udev_p_) {
        udev_iface_->Unref(*udev_p_);
        *udev_p_ = NULL;
      }
    }
   protected:
    UdevInterface* udev_iface_;
    struct udev** udev_p_;
   private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(UdevCloser);
  };

  class UdevEnumerateCloser {
   public:
    explicit UdevEnumerateCloser(UdevInterface* udev_iface,
                                 struct udev_enumerate** udev_enum_p)
        : udev_iface_(udev_iface), udev_enum_p_(udev_enum_p) {
      CHECK(udev_iface && udev_enum_p);
    }
    virtual ~UdevEnumerateCloser() {
      if (*udev_enum_p_) {
        udev_iface_->EnumerateUnref(*udev_enum_p_);
        *udev_enum_p_ = NULL;
      }
    }
   protected:
    UdevInterface* udev_iface_;
    struct udev_enumerate** udev_enum_p_;
   private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(UdevEnumerateCloser);
  };

  class UdevDeviceCloser {
   public:
    explicit UdevDeviceCloser(UdevInterface* udev_iface,
                              struct udev_device** udev_dev_p)
        : udev_iface_(udev_iface), udev_dev_p_(udev_dev_p) {
      CHECK(udev_iface && udev_dev_p);
    }
    virtual ~UdevDeviceCloser() {
      if (*udev_dev_p_) {
        udev_iface_->DeviceUnref(*udev_dev_p_);
        *udev_dev_p_ = NULL;
      }
    }
   protected:
    UdevInterface* udev_iface_;
    struct udev_device** udev_dev_p_;
   private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(UdevDeviceCloser);
  };

  virtual UdevCloser* NewUdevCloser(struct udev** udev_p) {
    return new UdevCloser(this, udev_p);
  }
  virtual UdevEnumerateCloser* NewUdevEnumerateCloser(
      struct udev_enumerate** udev_enum_p) {
    return new UdevEnumerateCloser(this, udev_enum_p);
  }
  virtual UdevDeviceCloser* NewUdevDeviceCloser(
      struct udev_device** udev_dev_p) {
    return new UdevDeviceCloser(this, udev_dev_p);
  }

  // Lists.
  virtual const char* ListEntryGetName(struct udev_list_entry* list_entry) = 0;
  virtual udev_list_entry* ListEntryGetNext(
      struct udev_list_entry* list_entry) = 0;

  // Device methods.
  virtual struct udev_device* DeviceNewFromSyspath(
      struct udev* udev,
      const char* syspath) = 0;
  virtual const char* DeviceGetPropertyValue(struct udev_device* udev_device,
                                             const char* key) = 0;
  virtual const char* DeviceGetSyspath(
      struct udev_device* udev_device) = 0;
  virtual void DeviceUnref(struct udev_device* udev_device) = 0;

  // Enumerate methods.
  virtual struct udev_enumerate* EnumerateNew(struct udev* udev) = 0;
  virtual int EnumerateAddMatchSubsystem(struct udev_enumerate* udev_enum,
                                         const char* subsystem) = 0;
  virtual int EnumerateAddMatchSysname(struct udev_enumerate* udev_enum,
                                       const char* sysname) = 0;
  virtual int EnumerateScanDevices(struct udev_enumerate* udev_enum) = 0;
  virtual struct udev_list_entry* EnumerateGetListEntry(
      struct udev_enumerate* udev_enum) = 0;
  virtual void EnumerateUnref(struct udev_enumerate* udev_enum) = 0;

  // Udev instance methods.
  virtual struct udev* New() = 0;
  virtual void Unref(struct udev* udev) = 0;
};


// Implementation of libudev interface using concrete udev calls.
class StandardUdevInterface : public UdevInterface {
 public:
  virtual ~StandardUdevInterface() {}

  // Concrete udev API wrappers utilizing the standard libudev calls.
  virtual const char* ListEntryGetName(struct udev_list_entry* list_entry) {
    return udev_list_entry_get_name(list_entry);
  }
  virtual struct udev_list_entry* ListEntryGetNext(
      struct udev_list_entry* list_entry) {
    return udev_list_entry_get_next(list_entry);
  }

  virtual struct udev_device* DeviceNewFromSyspath(struct udev* udev,
                                                   const char* syspath) {
    return udev_device_new_from_syspath(udev, syspath);
  }
  virtual const char* DeviceGetPropertyValue(struct udev_device* udev_device,
                                             const char* key) {
    return udev_device_get_property_value(udev_device, key);
  }
  virtual const char* DeviceGetSyspath(struct udev_device* udev_device) {
    return udev_device_get_syspath(udev_device);
  }
  virtual void DeviceUnref(struct udev_device* udev_device) {
    udev_device_unref(udev_device);
  }

  virtual struct udev_enumerate* EnumerateNew(struct udev* udev) {
    return udev_enumerate_new(udev);
  }
  virtual int EnumerateAddMatchSubsystem(struct udev_enumerate* udev_enum,
                                         const char* subsystem) {
    return udev_enumerate_add_match_subsystem(udev_enum, subsystem);
  }
  virtual int EnumerateAddMatchSysname(struct udev_enumerate* udev_enum,
                                       const char* sysname) {
    return udev_enumerate_add_match_sysname(udev_enum, sysname);
  }
  virtual int EnumerateScanDevices(struct udev_enumerate* udev_enum) {
    return udev_enumerate_scan_devices(udev_enum);
  }
  virtual struct udev_list_entry* EnumerateGetListEntry(
      struct udev_enumerate* udev_enum) {
    return udev_enumerate_get_list_entry(udev_enum);
  }
  virtual void EnumerateUnref(struct udev_enumerate* udev_enum) {
    udev_enumerate_unref(udev_enum);
  }

  virtual struct udev* New() {
    return udev_new();
  }
  virtual void Unref(struct udev* udev) {
    udev_unref(udev);
  }
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UDEV_INTERFACE_H__

