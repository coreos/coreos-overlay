// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_H__

#include <deque>

#include "base/basictypes.h"

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
  kActionCodeOmahaRequestNoUpdateCheckNode = 32,
  kActionCodeOmahaRequestNoUpdateCheckStatus = 33,
  kActionCodeOmahaRequestBadUpdateCheckStatus = 34,
  kActionCodeOmahaUpdateIgnoredPerPolicy = 35,
  kActionCodeOmahaUpdateDeferredPerPolicy = 36,
  kActionCodeOmahaErrorInHTTPResponse = 37,
  kActionCodeDownloadOperationHashMissingError = 38,
  kActionCodeDownloadInvalidMetadataSize = 39,
  kActionCodeDownloadInvalidMetadataSignature = 39,
  kActionCodeDownloadMetadataSignatureMissingError = 40,

  // Any code above this is sent to both Omaha and UMA as-is.
  // Codes/flags below this line is sent only to Omaha and not to UMA.

  // kNumBucketsForUMAMetrics is not an error code per se, it's just the count
  // of the number of enums above.  Add any new errors above this line if you
  // want them to show up on UMA. Stuff below this line will not be sent to UMA
  // but is used for other errors that are sent to Omaha. We don't assign any
  // particular value for this enum so that it's just one more than the last
  // one above and thus always represents the correct count of UMA metrics
  // buckets, even when new enums are added above this line in future. See
  // utils::SendErrorCodeToUMA on how this enum is used.
  kNumBucketsForUMAMetrics,

  // use the 2xxx range to encode HTTP errors. These errors are available in
  // Dremel with the individual granularity. But for UMA purposes, all these
  // errors are aggregated into one: kActionCodeOmahaErrorInHTTPResponse.
  kActionCodeOmahaRequestHTTPResponseBase = 2000,  // + HTTP response code

  // Bit flags. Remember to update the mask below for new bits.
  kActionCodeResumedFlag = 1 << 30,  // Set if resuming an interruped update.
  kActionCodeBootModeFlag = 1 << 31,  // Set if boot mode not normal.

  // Mask to be used to extract the actual code without the higher-order
  // bit flags (for reporting to UMA which requires small contiguous
  // enum values)
  kActualCodeMask = ~(kActionCodeResumedFlag | kActionCodeBootModeFlag)
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
