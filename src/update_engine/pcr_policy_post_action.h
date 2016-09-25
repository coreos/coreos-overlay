// Copyright (c) 2016 The CoreOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PCR_POLICY_POST_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PCR_POLICY_POST_ACTION_H__

#include <memory>
#include <string>

#include "update_engine/action.h"
#include "update_engine/http_fetcher.h"
#include "update_engine/install_plan.h"
#include "update_engine/system_state.h"

namespace chromeos_update_engine {

class PCRPolicyPostAction;

template<>
class ActionTraits<PCRPolicyPostAction> {
 public:
  // Takes the install plan as input
  typedef InstallPlan InputObjectType;
  // Passes the install plan as output
  typedef InstallPlan OutputObjectType;
};

class PCRPolicyPostAction : public Action<PCRPolicyPostAction>,
                            public HttpFetcherDelegate {
 public:
  // URL to POST PCR data to is found in system_state.
  // Takes ownership of the passed in HttpFetcher. Useful for testing.
  PCRPolicyPostAction(SystemState* system_state, HttpFetcher* http_fetcher)
    : system_state_(system_state),
      http_fetcher_(http_fetcher) {}
  virtual ~PCRPolicyPostAction() {}

  typedef ActionTraits<PCRPolicyPostAction>::InputObjectType
  InputObjectType;
  typedef ActionTraits<PCRPolicyPostAction>::OutputObjectType
  OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  // Delegate methods (see http_fetcher.h)
  virtual void ReceivedBytes(HttpFetcher *fetcher,
                             const char* bytes, int length);

  virtual void TransferComplete(HttpFetcher *fetcher, bool successful);

  // Debugging/logging
  static std::string StaticType() { return "PCRPolicyPostAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // Global system context.
  SystemState* system_state_;

  // Pointer to our HttpFetcher.
  std::unique_ptr<HttpFetcher> http_fetcher_;

  // Stores the HTTP response.
  std::string response_buffer_;

  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;

  DISALLOW_COPY_AND_ASSIGN(PCRPolicyPostAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PCR_POLICY_POST_ACTION_H__
