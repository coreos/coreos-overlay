// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__

#include <tr1/memory>
#include <string>
#include <vector>
#include <glib.h>
#include "update_engine/action_processor.h"
#include "update_engine/omaha_response_handler_action.h"

namespace chromeos_update_engine {

class UpdateAttempter : public ActionProcessorDelegate {
 public:
  explicit UpdateAttempter(GMainLoop *loop)
      : full_update_(false),
        loop_(loop) {}
  void Update(bool force_full_update);
  
  // Delegate method:
  void ProcessingDone(const ActionProcessor* processor, bool success);
  
  // Stop updating. An attempt will be made to record status to the disk
  // so that updates can be resumed later.
  void Terminate();
  
  // Try to resume from a previously Terminate()d update.
  void ResumeUpdating();
  
  // Returns the current status in the out params. Returns true on success.
  bool GetStatus(int64_t* last_checked_time,
                 double* progress,
                 std::string* current_operation,
                 std::string* new_version,
                 int64_t* new_size);

 private:
  bool full_update_;
  std::vector<std::tr1::shared_ptr<AbstractAction> > actions_;
  ActionProcessor processor_;
  GMainLoop *loop_;

  // pointer to the OmahaResponseHandlerAction in the actions_ vector;
  std::tr1::shared_ptr<OmahaResponseHandlerAction> response_handler_action_;
  DISALLOW_COPY_AND_ASSIGN(UpdateAttempter);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
