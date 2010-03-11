// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/cycle_breaker.h"
#include <set>
#include <utility>
#include "update_engine/graph_utils.h"
#include "update_engine/tarjan.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::set;
using std::vector;

namespace chromeos_update_engine {

// This is the outer function from the original paper.
void CycleBreaker::BreakCycles(const Graph& graph, set<Edge>* out_cut_edges) {
  cut_edges_.clear();
  
  // Make a copy, which we will modify by removing edges. Thus, in each
  // iteration subgraph_ is the current subgraph or the original with
  // vertices we desire. This variable was "A_K" in the original paper.
  subgraph_ = graph;
    
  // The paper calls for the "adjacency structure (i.e., graph) of
  // strong (-ly connected) component K with least vertex in subgraph
  // induced by {s, s + 1, ..., n}".
  // We arbitrarily order each vertex by its index in the graph. Thus,
  // each iteration, we are looking at the subgraph {s, s + 1, ..., n}
  // and looking for the strongly connected component with vertex s.

  TarjanAlgorithm tarjan;
    
  for (Graph::size_type i = 0; i < subgraph_.size(); i++) {
    if (i > 0) {
      // Erase node (i - 1) from subgraph_. First, erase what it points to
      subgraph_[i - 1].out_edges.clear();
      // Now, erase any pointers to node (i - 1)
      for (Graph::size_type j = i; j < subgraph_.size(); j++) {
        subgraph_[j].out_edges.erase(i - 1);
      }
    }

    // Calculate SCC (strongly connected component) with vertex i.
    vector<Vertex::Index> component_indexes;
    tarjan.Execute(i, &subgraph_, &component_indexes);

    // Set subgraph edges for the components in the SCC.
    for (vector<Vertex::Index>::iterator it = component_indexes.begin();
         it != component_indexes.end(); ++it) {
      subgraph_[*it].subgraph_edges.clear();
      for (vector<Vertex::Index>::iterator jt = component_indexes.begin();
           jt != component_indexes.end(); ++jt) {
        // If there's a link from *it -> *jt in the graph,
        // add a subgraph_ edge
        if (utils::MapContainsKey(subgraph_[*it].out_edges, *jt))
          subgraph_[*it].subgraph_edges.insert(*jt);
      }        
    }

    current_vertex_ = i;
    blocked_.clear();
    blocked_.resize(subgraph_.size());
    blocked_graph_.clear();
    blocked_graph_.resize(subgraph_.size());
    Circuit(current_vertex_);
  }
  
  out_cut_edges->swap(cut_edges_);
  DCHECK(stack_.empty());
}

void CycleBreaker::HandleCircuit() {
  stack_.push_back(current_vertex_);
  CHECK_GE(stack_.size(), 2);
  Edge min_edge = make_pair(stack_[0], stack_[1]);
  uint64 min_edge_weight = kuint64max;
  for (vector<Vertex::Index>::const_iterator it = stack_.begin();
       it != (stack_.end() - 1); ++it) {
    Edge edge = make_pair(*it, *(it + 1));
    if (cut_edges_.find(edge) != cut_edges_.end()) {
      stack_.pop_back();
      return;
    }
    uint64 edge_weight = graph_utils::EdgeWeight(subgraph_, edge);
    if (edge_weight < min_edge_weight) {
      min_edge_weight = edge_weight;
      min_edge = edge;
    }
  }
  cut_edges_.insert(min_edge);
  stack_.pop_back();
}

void CycleBreaker::Unblock(Vertex::Index u) {
  blocked_[u] = false;

  for (Vertex::EdgeMap::iterator it = blocked_graph_[u].out_edges.begin();
       it != blocked_graph_[u].out_edges.end(); ) {
    Vertex::Index w = it->first;
    blocked_graph_[u].out_edges.erase(it++);
    if (blocked_[w])
      Unblock(w);
  }
}

bool CycleBreaker::Circuit(Vertex::Index vertex) {
  // "vertex" was "v" in the original paper.
  bool found = false;  // Was "f" in the original paper.
  stack_.push_back(vertex);
  blocked_[vertex] = true;

  for (Vertex::SubgraphEdgeMap::iterator w =
           subgraph_[vertex].subgraph_edges.begin();
       w != subgraph_[vertex].subgraph_edges.end(); ++w) {
    if (*w == current_vertex_) {
      // The original paper called for printing stack_ followed by
      // current_vertex_ here, which is a cycle. Instead, we call
      // HandleCircuit() to break it.
      HandleCircuit();
      found = true;
    } else if (!blocked_[*w]) {
      if (Circuit(*w))
        found = true;
    }
  }

  if (found) {
    Unblock(vertex);
  } else {
    for (Vertex::SubgraphEdgeMap::iterator w =
             subgraph_[vertex].subgraph_edges.begin();
         w != subgraph_[vertex].subgraph_edges.end(); ++w) {
      subgraph_[*w].subgraph_edges.insert(vertex);
    }
  }
  CHECK_EQ(vertex, stack_.back());
  stack_.pop_back();
  return found;
}

}  // namespace chromeos_update_engine
