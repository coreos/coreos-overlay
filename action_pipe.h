// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PIPE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PIPE_H__

#include <stdio.h>
#include <iostream>
#include <map>
#include <string>
#include <tr1/memory>
#include "base/basictypes.h"
#include "chromeos/obsolete_logging.h"

// The structure of these classes (Action, ActionPipe, ActionProcessor, etc.)
// is based on the KSAction* classes from the Google Update Engine code at
// http://code.google.com/p/update-engine/ . The author of this file sends
// a big thanks to that team for their high quality design, implementation,
// and documentation.

// This class serves as a temporary holding area for an object passed out
// from one Action and into another Action. It's templated so that it may
// contain any type of object that an Action outputs/inputs. Actions
// cannot be bonded (i.e., connected with a pipe) if their output/input
// object types differ (a compiler error will result).
//
// An ActionPipe is generally created with the Bond() method and owned by
// the two Action objects. a shared_ptr is used so that when the last Action
// pointing to an ActionPipe dies, the ActionPipe dies, too.

using std::map;
using std::string;
using std::tr1::shared_ptr;

namespace chromeos_update_engine {

// Used by Actions an InputObjectType or OutputObjectType to specify that
// for that type, no object is taken/given.
class NoneType {};

template<typename T>
class Action;

template<typename ObjectType>
class ActionPipe {
 public:
  virtual ~ActionPipe() {
    LOG(INFO) << "ActionPipe died";
  }

  // This should be called by an Action on its input pipe.
  // Returns a reference to the stored object.
  const ObjectType& contents() const { return contents_; }

  // This should be called by an Action on its output pipe.
  // Stores a copy of the passed object in this pipe.
  void set_contents(const ObjectType& contents) { contents_ = contents; }

  // Bonds two Actions together with a new ActionPipe. The ActionPipe is
  // jointly owned by the two Actions and will be automatically destroyed
  // when the last Action is destroyed.
  template<typename FromAction, typename ToAction>
  static void Bond(FromAction* from, ToAction* to) {
    shared_ptr<ActionPipe<ObjectType> > pipe(new ActionPipe<ObjectType>);
    from->set_out_pipe(pipe);

    to->set_in_pipe(pipe);  // If you get an error on this line, then
    // it most likely means that the From object's OutputObjectType is
    // different from the To object's InputObjectType.
  }

 private:
  ObjectType contents_;

  // The ctor is private. This is because this class should construct itself
  // via the static Bond() method.
  ActionPipe() {}
  DISALLOW_COPY_AND_ASSIGN(ActionPipe);
};

// Utility function
template<typename FromAction, typename ToAction>
void BondActions(FromAction* from, ToAction* to) {
  // TODO(adlr): find something like this that the compiler accepts:
  // COMPILE_ASSERT(typeof(typename FromAction::OutputObjectType) ==
  //                typeof(typename ToAction::InputObjectType),
  //     FromAction_OutputObjectType_doesnt_match_ToAction_InputObjectType);
  ActionPipe<typename FromAction::OutputObjectType>::Bond(from, to);
}

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PIPE_H__
