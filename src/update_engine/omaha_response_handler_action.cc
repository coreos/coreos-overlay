// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_response_handler_action.h"

#include <string>

#include <glog/logging.h>

#include "update_engine/payload_processor.h"
#include "update_engine/payload_state_interface.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

const char OmahaResponseHandlerAction::kDeadlineFile[] =
    "/tmp/update-check-response-deadline";

OmahaResponseHandlerAction::OmahaResponseHandlerAction(
    SystemState* system_state)
    : system_state_(system_state),
      got_no_update_response_(false),
      key_path_(PayloadProcessor::kUpdatePayloadPublicKeyPath) {}

void OmahaResponseHandlerAction::PerformAction() {
  CHECK(HasInputObject());
  ScopedActionCompleter completer(processor_, this);
  const OmahaResponse& response = GetInputObject();
  if (!response.update_exists) {
    got_no_update_response_ = true;
    LOG(INFO) << "There are no updates. Aborting.";
    return;
  }

  // All decisions as to which URL should be used have already been done. So,
  // make the download URL as the payload URL at the current url index.
  uint32_t url_index = system_state_->payload_state()->GetUrlIndex();
  LOG(INFO) << "Using Url" << url_index << " as the download url this time";
  CHECK(url_index < response.payload_urls.size());
  install_plan_.download_url = response.payload_urls[url_index];

  // Fill up the other properties based on the response.
  install_plan_.payload_size = response.size;
  install_plan_.payload_hash = response.hash;
  install_plan_.hash_checks_mandatory = AreHashChecksMandatory(response);
  install_plan_.is_resume =
      PayloadProcessor::CanResumeUpdate(system_state_->prefs(), response.hash);
  if (!install_plan_.is_resume) {
    LOG_IF(WARNING, !PayloadProcessor::ResetUpdateProgress(
        system_state_->prefs(), false))
        << "Unable to reset the update progress.";
    LOG_IF(WARNING, !system_state_->prefs()->SetString(
        kPrefsUpdateCheckResponseHash, response.hash))
        << "Unable to save the update check response hash.";
  }

  TEST_AND_RETURN(GetInstallDev(
      (!boot_device_.empty() ? boot_device_ : utils::BootDevice()),
      &install_plan_.install_path));

  TEST_AND_RETURN(HasOutputPipe());
  if (HasOutputPipe())
    SetOutputObject(install_plan_);
  LOG(INFO) << "Using this install plan:";
  install_plan_.Dump();

  // Send the deadline data (if any) to Chrome through a file. This is a pretty
  // hacky solution but should be OK for now.
  //
  // TODO(petkov): Rearchitect this to avoid communication through a
  // file. Ideallly, we would include this information in D-Bus's GetStatus
  // method and UpdateStatus signal. A potential issue is that update_engine may
  // be unresponsive during an update download.
  utils::WriteFile(kDeadlineFile,
                   response.deadline.data(),
                   response.deadline.size());
  chmod(kDeadlineFile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  completer.set_code(kActionCodeSuccess);
}

bool OmahaResponseHandlerAction::GetInstallDev(const std::string& boot_dev,
                                               std::string* install_dev) {
  TEST_AND_RETURN_FALSE(utils::StringHasPrefix(boot_dev, "/dev/"));
  string ret(boot_dev);
  string::reverse_iterator it = ret.rbegin();  // last character in string
  // Right now, we just switch '3' and '4' partition numbers.
  TEST_AND_RETURN_FALSE((*it == '3') || (*it == '4'));
  *it = (*it == '3') ? '4' : '3';
  *install_dev = ret;
  return true;
}

bool OmahaResponseHandlerAction::AreHashChecksMandatory(
    const OmahaResponse& response) {
  // All our internal testing uses dev server which doesn't generate metadata
  // signatures yet. So, in order not to break image_to_live or other AU tools,
  // we should waive the hash checks for those cases. Since all internal
  // testing is done using a dev_image or test_image, we can use that as a
  // criteria for waiving. This criteria reduces the attack surface as
  // opposed to waiving the checks when we're in dev mode, because we do want
  // to enforce the hash checks when our end customers run in dev mode if they
  // are using an official build, so that they are protected more.
  if (!utils::IsOfficialBuild()) {
    LOG(INFO) << "Waiving payload hash checks for unofficial builds";
    return false;
  }

  // TODO(jaysri): VALIDATION: For official builds, we currently waive hash
  // checks for HTTPS until we have rolled out at least once and are confident
  // nothing breaks. chromium-os:37082 tracks turning this on for HTTPS
  // eventually.

  // Even if there's a single non-HTTPS URL, make the hash checks as
  // mandatory because we could be downloading the payload from any URL later
  // on. It's really hard to do book-keeping based on each byte being
  // downloaded to see whether we only used HTTPS throughout.
  for (size_t i = 0; i < response.payload_urls.size(); i++) {
    if (!utils::IsHTTPS(response.payload_urls[i])) {
      LOG(INFO) << "Mandating payload hash checks since Omaha response "
                << "contains non-HTTPS URL(s)";
      return true;
    }
  }

  LOG(INFO) << "Waiving payload hash checks since Omaha response "
            << "only has HTTPS URL(s)";
  return false;
}

}  // namespace chromeos_update_engine
