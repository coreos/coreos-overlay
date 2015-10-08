// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>
#include <glog/logging.h>
#include "update_engine/tarjan.h"
#include "update_engine/utils.h"

using std::min;
using std::vector;

namespace chromeos_update_engine {

namespace {
const vector<Vertex>::size_type kInvalidIndex = -1;
}

void TarjanAlgorithm::Execute(Vertex::Index vertex,
                              Graph* graph,
                              vector<Vertex::Index>* out) {
  stack_.clear();
  components_.clear();
  index_ = 0;
  for (Graph::iterator it = graph->begin(); it != graph->end(); ++it)
    it->index = it->lowlink = kInvalidIndex;
  required_vertex_ = vertex;

  Tarjan(vertex, graph);
  if (!components_.empty())
    out->swap(components_[0]);
}

void TarjanAlgorithm::Tarjan(Vertex::Index vertex, Graph* graph) {
  CHECK_EQ((*graph)[vertex].index, kInvalidIndex);
  (*graph)[vertex].index = index_;
  (*graph)[vertex].lowlink = index_;
  index_++;
  stack_.push_back(vertex);
  for (Vertex::EdgeMap::iterator it = (*graph)[vertex].out_edges.begin();
       it != (*graph)[vertex].out_edges.end(); ++it) {
    Vertex::Index vertex_next = it->first;
    if ((*graph)[vertex_next].index == kInvalidIndex) {
      Tarjan(vertex_next, graph);
      (*graph)[vertex].lowlink = min((*graph)[vertex].lowlink,
                                     (*graph)[vertex_next].lowlink);
    } else if (utils::VectorContainsValue(stack_, vertex_next)) {
      (*graph)[vertex].lowlink = min((*graph)[vertex].lowlink,
                                     (*graph)[vertex_next].index);
    }
  }
  if ((*graph)[vertex].lowlink == (*graph)[vertex].index) {
    vector<Vertex::Index> component;
    Vertex::Index other_vertex;
    do {
      other_vertex = stack_.back();
      stack_.pop_back();
      component.push_back(other_vertex);
    } while (other_vertex != vertex && !stack_.empty());
      
    if (utils::VectorContainsValue(component, required_vertex_)) {
      components_.resize(components_.size() + 1);
      component.swap(components_.back());
    }
  }
}

}  // namespace chromeos_update_engine

