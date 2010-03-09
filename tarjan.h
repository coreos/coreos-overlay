// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_TARJAN_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_TARJAN_H__

// This is an implemenation of Tarjan's algorithm which finds all
// Strongly Connected Components in a graph.

// Note: a true Tarjan algorithm would find all strongly connected components
// in the graph. This implementation will only find the strongly connected
// component containing the vertex passed in.

#include <vector>
#include "update_engine/graph_types.h"

namespace chromeos_update_engine {

class TarjanAlgorithm {
 public:
  TarjanAlgorithm() : index_(0), required_vertex_(0) {}

  // 'out' is set to the result if there is one, otherwise it's untouched.
  void Execute(Vertex::Index vertex,
               Graph* graph,
               std::vector<Vertex::Index>* out);
 private:
  void Tarjan(Vertex::Index vertex, Graph* graph);

  Vertex::Index index_;
  Vertex::Index required_vertex_;
  std::vector<Vertex::Index> stack_;
  std::vector<std::vector<Vertex::Index> > components_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_TARJAN_H__
