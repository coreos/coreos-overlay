// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_H__

#include <deque>

#include "macros.h"

// The structure of these classes (Action, ActionPipe, ActionProcessor, etc.)
// is based on the KSAction* classes from the Google Update Engine code at
// http://code.google.com/p/update-engine/ . The author of this file sends
// a big thanks to that team for their high quality design, implementation,
// and documentation.

// See action.h for an overview of this class and other other Action* classes.

// An ActionProcessor keeps a queue of Actions and processes them in order.

namespace chromeos_update_engine {

// Action exit codes.
enum ActionExitCode {
  kActionCodeSuccess = 0,
  kActionCodeError = 1,
  kActionCodeOmahaRequestError = 2,
  kActionCodeOmahaResponseHandlerError = 3,
  kActionCodeFilesystemCopierError = 4,
  kActionCodePostinstallRunnerError = 5,
  kActionCodeSetBootableFlagError = 6,  // TODO(petkov): Unused. Recycle?
  kActionCodeInstallDeviceOpenError = 7,
  kActionCodeKernelDeviceOpenError = 8,
  kActionCodeDownloadTransferError = 9,
  kActionCodePayloadHashMismatchError = 10,
  kActionCodePayloadSizeMismatchError = 11,
  kActionCodeDownloadPayloadVerificationError = 12,
  kActionCodeDownloadNewPartitionInfoError = 13,
  kActionCodeDownloadWriteError = 14,
  kActionCodeNewRootfsVerificationError = 15,
  kActionCodeNewKernelVerificationError = 16,
  kActionCodeSignedDeltaPayloadExpectedError = 17,
  kActionCodeDownloadPayloadPubKeyVerificationError = 18,
  kActionCodePostinstallBootedFromFirmwareB = 19,
  kActionCodeDownloadStateInitializationError = 20,
  kActionCodeDownloadInvalidMetadataMagicString = 21,
  kActionCodeDownloadSignatureMissingInManifest = 22,
  kActionCodeDownloadManifestParseError = 23,
  kActionCodeDownloadMetadataSignatureError = 24,
  kActionCodeDownloadMetadataSignatureVerificationError = 25,
  kActionCodeDownloadMetadataSignatureMismatch = 26,
  kActionCodeDownloadOperationHashVerificationError = 27,
  kActionCodeDownloadOperationExecutionError = 28,
  kActionCodeDownloadOperationHashMismatch = 29,
  kActionCodeOmahaRequestEmptyResponseError = 30,
  kActionCodeOmahaRequestXMLParseError = 31,
  kActionCodeDownloadInvalidMetadataSize = 32,
  kActionCodeDownloadInvalidMetadataSignature = 33,
  kActionCodeOmahaResponseInvalid = 34,
  kActionCodeOmahaUpdateIgnoredPerPolicy = 35,
  kActionCodeOmahaUpdateDeferredPerPolicy = 36,
  kActionCodeOmahaErrorInHTTPResponse = 37,
  kActionCodeDownloadOperationHashMissingError = 38,
  kActionCodeDownloadMetadataSignatureMissingError = 39,
  kActionCodeOmahaUpdateDeferredForBackoff = 40,
  kActionCodePostinstallPowerwashError = 41,

  // Note: When adding new error codes, please remember to add the
  // error into one of the buckets in PayloadState::UpdateFailed method so
  // that the retries across URLs and the payload backoff mechanism work
  // correctly for those new error codes.

  // use the 2xxx range to encode HTTP errors. For PayloadState::UpdateFailed
  // all these errors are aggregated into kActionCodeOmahaErrorInHTTPResponse.
  kActionCodeOmahaRequestHTTPResponseBase = 2000,  // + HTTP response code

  // TODO(jaysri): Move out all the bit masks into separate constants
  // outside the enum as part of fixing bug 34369.
  // Bit flags. Remember to update the mask below for new bits.

  // Set if boot mode not normal.
  kActionCodeDevModeFlag        = 1 << 31,

  // Set if resuming an interruped update.
  kActionCodeResumedFlag         = 1 << 30,

  // Set if using a dev/test image as opposed to an MP-signed image.
  kActionCodeTestImageFlag       = 1 << 29,

  // Set if using devserver or Omaha sandbox (using crosh autest).
  kActionCodeTestOmahaUrlFlag    = 1 << 28,

  // Mask that indicates bit positions that are used to indicate special flags
  // that are embedded in the error code to provide additional context about
  // the system in which the error was encountered.
  kSpecialFlags = (kActionCodeDevModeFlag |
                   kActionCodeResumedFlag |
                   kActionCodeTestImageFlag |
                   kActionCodeTestOmahaUrlFlag)
};

class AbstractAction;
class ActionProcessorDelegate;

class ActionProcessor {
 public:
  ActionProcessor();

  virtual ~ActionProcessor();

  // Starts processing the first Action in the queue. If there's a delegate,
  // when all processing is complete, ProcessingDone() will be called on the
  // delegate.
  virtual void StartProcessing();

  // Aborts processing. If an Action is running, it will have
  // TerminateProcessing() called on it. The Action that was running
  // will be lost and must be re-enqueued if this Processor is to use it.
  void StopProcessing();

  // Returns true iff an Action is currently processing.
  bool IsRunning() const { return NULL != current_action_; }

  // Adds another Action to the end of the queue.
  virtual void EnqueueAction(AbstractAction* action);

  // Sets/gets the current delegate. Set to NULL to remove a delegate.
  ActionProcessorDelegate* delegate() const { return delegate_; }
  void set_delegate(ActionProcessorDelegate *delegate) {
    delegate_ = delegate;
  }

  // Returns a pointer to the current Action that's processing.
  AbstractAction* current_action() const {
    return current_action_;
  }

  // Called by an action to notify processor that it's done. Caller passes self.
  void ActionComplete(AbstractAction* actionptr, ActionExitCode code);

 private:
  // Actions that have not yet begun processing, in the order in which
  // they'll be processed.
  std::deque<AbstractAction*> actions_;

  // A pointer to the currrently processing Action, if any.
  AbstractAction* current_action_;

  // A pointer to the delegate, or NULL if none.
  ActionProcessorDelegate *delegate_;
  DISALLOW_COPY_AND_ASSIGN(ActionProcessor);
};

// A delegate object can be used to be notified of events that happen
// in an ActionProcessor. An instance of this class can be passed to an
// ActionProcessor to register itself.
class ActionProcessorDelegate {
 public:
  // Called when all processing in an ActionProcessor has completed. A pointer
  // to the ActionProcessor is passed. |code| is set to the exit code of the
  // last completed action.
  virtual void ProcessingDone(const ActionProcessor* processor,
                              ActionExitCode code) {}

  // Called when processing has stopped. Does not mean that all Actions have
  // completed. If/when all Actions complete, ProcessingDone() will be called.
  virtual void ProcessingStopped(const ActionProcessor* processor) {}

  // Called whenever an action has finished processing, either successfully
  // or otherwise.
  virtual void ActionCompleted(ActionProcessor* processor,
                               AbstractAction* action,
                               ActionExitCode code) {}
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_H__
