// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_H__

#include <stdio.h>
#include <tr1/memory>
#include <iostream>
#include "base/basictypes.h"
#include "chromeos/obsolete_logging.h"
#include "update_engine/action_processor.h"
#include "update_engine/action_pipe.h"

// The structure of these classes (Action, ActionPipe, ActionProcessor, etc.)
// is based on the KSAction* classes from the Google Update Engine code at
// http://code.google.com/p/update-engine/ . The author of this file sends
// a big thanks to that team for their high quality design, implementation,
// and documentation.
//
// Readers may want to consult this wiki page from the Update Engine site:
// http://code.google.com/p/update-engine/wiki/ActionProcessor
// Although it's referring to the Objective-C KSAction* classes, much
// applies here as well.
//
// How it works:
//
// First off, there is only one thread and all I/O should be asynchronous.
// A glib main loop blocks whenever there is no work to be done. This happens
// where there is no CPU work to be done and no I/O ready to transfer in or
// out. Two kinds of events can wake up the main loop: timer alarm or file
// descriptors. If either of these happens, glib finds out the owner of what
// fired and calls the appropriate code to handle it. As such, all the code
// in the Action* classes and the code that is calls is non-blocking.
//
// An ActionProcessor contains a queue of Actions to perform. When
// ActionProcessor::StartProcessing() is called, it executes the first action.
// Each action tells the processor when it has completed, which causes the
// Processor to execute the next action. ActionProcessor may have a delegate
// (an object of type ActionProcessorDelegate). If it does, the delegate
// is called to be notified of events as they happen.
//
// ActionPipe classes
//
// See action_pipe.h
//
// ActionTraits
//
// We need to use an extra class ActionTraits. ActionTraits is a simple
// templated class that contains only two typedefs: OutputObjectType and
// InputObjectType. Each action class also has two typedefs of the same name
// that are of the same type. So, to get the input/output types of, e.g., the
// DownloadAction class, we look at the type of
// DownloadAction::InputObjectType.
//
// Each concrete Action class derives from Action<T>. This means that during
// template instatiation of Action<T>, T is declared but not defined, which
// means that T::InputObjectType (and OutputObjectType) is not defined.
// However, the traits class is constructed in such a way that it will be
// template instatiated first, so Action<T> *can* find the types it needs by
// consulting ActionTraits<T>::InputObjectType (and OutputObjectType).
// This is why the ActionTraits classes are needed.

namespace chromeos_update_engine {

// It is handy to have a non-templated base class of all Actions.
class AbstractAction {
 public:
  AbstractAction() : processor_(NULL) {}

  // Begin performing the action. Since this code is asynchronous, when this
  // method returns, it means only that the action has started, not necessarily
  // completed. However, it's acceptable for this method to perform the
  // action synchronously; Action authors should understand the implications
  // of synchronously performing, though, because this is a single-threaded
  // app, the entire process will be blocked while the action performs.
  //
  // When the action is complete, it must call
  // ActionProcessor::ActionComplete(this); to notify the processor that it's
  // done.
  virtual void PerformAction() = 0;

  // Called by the ActionProcessor to tell this Action which processor
  // it belongs to.
  void SetProcessor(ActionProcessor* processor) {
    if (processor)
      CHECK(!processor_);
    else
      CHECK(processor_);
    processor_ = processor;
  }

  // Returns true iff the action is the current action of its ActionProcessor.
  bool IsRunning() const {
    if (!processor_)
      return false;
    return processor_->current_action() == this;
  }

  // Called on asynchronous actions if canceled. Actions may implement if
  // there's any cleanup to do. There is no need to call
  // ActionProcessor::ActionComplete() because the processor knows this
  // action is terminating.
  // Only the ActionProcessor should call this.
  virtual void TerminateProcessing() {};

  // These methods are useful for debugging. TODO(adlr): consider using
  // std::type_info for this?
  // Type() returns a string of the Action type. I.e., for DownloadAction,
  // Type() would return "DownloadAction".
  virtual std::string Type() const = 0;

 protected:
  // A weak pointer to the processor that owns this Action.
  ActionProcessor* processor_;
};

// Forward declare a couple classes we use.
template<typename T>
class ActionPipe;
template<typename T>
class ActionTraits;

template<typename SubClass>
class Action : public AbstractAction {
 public:
  virtual ~Action() {
    LOG(INFO) << "Action died";
  }

  // Attaches an input pipe to this Action. This is optional; an Action
  // doesn't need to have an input pipe. The input pipe must be of the type
  // of object that this class expects.
  // This is generally called by ActionPipe::Bond()
  void set_in_pipe(
      // this type is a fancy way of saying: a shared_ptr to an
      // ActionPipe<InputObjectType>.
      const std::tr1::shared_ptr<ActionPipe<
          typename ActionTraits<SubClass>::InputObjectType> >&
          in_pipe) {
    in_pipe_ = in_pipe;
  }

  // Attaches an output pipe to this Action. This is optional; an Action
  // doesn't need to have an output pipe. The output pipe must be of the type
  // of object that this class expects.
  // This is generally called by ActionPipe::Bond()
  void set_out_pipe(
      // this type is a fancy way of saying: a shared_ptr to an
      // ActionPipe<OutputObjectType>.
      const std::tr1::shared_ptr<ActionPipe<
          typename ActionTraits<SubClass>::OutputObjectType> >&
          out_pipe) {
    out_pipe_ = out_pipe;
  }

  // Returns true iff there is an associated input pipe. If there's an input
  // pipe, there's an input object, but it may have been constructed with the
  // default ctor if the previous action didn't call SetOutputObject().
  bool HasInputObject() const { return in_pipe_.get(); }

  // returns a const reference to the object in the input pipe.
  const typename ActionTraits<SubClass>::InputObjectType& GetInputObject()
      const {
    CHECK(HasInputObject());
    return in_pipe_->contents();
  }

  // Returns true iff there's an output pipe.
  bool HasOutputPipe() const {
    return out_pipe_.get();
  }

  // Copies the object passed into the output pipe. It will be accessible to
  // the next Action via that action's input pipe (which is the same as this
  // Action's output pipe).
  void SetOutputObject(
      const typename ActionTraits<SubClass>::OutputObjectType& out_obj) {
    CHECK(HasOutputPipe());
    out_pipe_->set_contents(out_obj);
  }

  // Returns a reference to the object sitting in the output pipe.
  const typename ActionTraits<SubClass>::OutputObjectType& GetOutputObject() {
    CHECK(HasOutputPipe());
    return out_pipe_->contents();
  }

protected:
  // We use a shared_ptr to the pipe. shared_ptr objects destroy what they
  // point to when the last such shared_ptr object dies. We consider the
  // Actions on either end of a pipe to "own" the pipe. When the last Action
  // of the two dies, the ActionPipe will die, too.
  std::tr1::shared_ptr<
      ActionPipe<typename ActionTraits<SubClass>::InputObjectType> >
      in_pipe_;
  std::tr1::shared_ptr<
      ActionPipe<typename ActionTraits<SubClass>::OutputObjectType> >
      out_pipe_;
};

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_H__
