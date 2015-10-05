// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/cycle_breaker.h"

#include <inttypes.h>

#include <limits>
#include <set>
#include <utility>

#include "strings/string_printf.h"
#include "update_engine/graph_utils.h"
#include "update_engine/tarjan.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::set;
using std::vector;
using strings::StringPrintf;

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
  skipped_ops_ = 0;
    
  for (Graph::size_type i = 0; i < subgraph_.size(); i++) {
    InstallOperation_Type op_type = graph[i].op.type();
    if (op_type == InstallOperation_Type_REPLACE ||
        op_type == InstallOperation_Type_REPLACE_BZ) {
      skipped_ops_++;
      continue;
    }

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
        if (subgraph_[*it].out_edges.count(*jt))
          subgraph_[*it].subgraph_edges.insert(*jt);
      }
    }

    current_vertex_ = i;
    blocked_.clear();
    blocked_.resize(subgraph_.size());
    blocked_graph_.clear();
    blocked_graph_.resize(subgraph_.size());
    Circuit(current_vertex_, 0);
  }
  
  out_cut_edges->swap(cut_edges_);
  LOG(INFO) << "Cycle breaker skipped " << skipped_ops_ << " ops.";
  DCHECK(stack_.empty());
}

static const size_t kMaxEdgesToConsider = 2;

void CycleBreaker::HandleCircuit() {
  stack_.push_back(current_vertex_);
  CHECK_GE(stack_.size(),
           static_cast<std::vector<Vertex::Index>::size_type>(2));
  Edge min_edge = make_pair(stack_[0], stack_[1]);
  uint64_t min_edge_weight = std::numeric_limits<uint64_t>::max();
  size_t edges_considered = 0;
  for (vector<Vertex::Index>::const_iterator it = stack_.begin();
       it != (stack_.end() - 1); ++it) {
    Edge edge = make_pair(*it, *(it + 1));
    if (cut_edges_.find(edge) != cut_edges_.end()) {
      stack_.pop_back();
      return;
    }
    uint64_t edge_weight = graph_utils::EdgeWeight(subgraph_, edge);
    if (edge_weight < min_edge_weight) {
      min_edge_weight = edge_weight;
      min_edge = edge;
    }
    edges_considered++;
    if (edges_considered == kMaxEdgesToConsider)
      break;
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

bool CycleBreaker::StackContainsCutEdge() const {
  for (std::vector<Vertex::Index>::const_iterator it = ++stack_.begin(),
           e = stack_.end(); it != e; ++it) {
    Edge edge = make_pair(*(it - 1), *it);
    if (cut_edges_.count(edge)) {
      return true;
    }
  }
  return false;
}

bool CycleBreaker::Circuit(Vertex::Index vertex, Vertex::Index depth) {
  // "vertex" was "v" in the original paper.
  bool found = false;  // Was "f" in the original paper.
  stack_.push_back(vertex);
  blocked_[vertex] = true;
  {
    static int counter = 0;
    counter++;
    if (counter == 10000) {
      counter = 0;
      std::string stack_str;
      for (vector<Vertex::Index>::const_iterator it = stack_.begin();
           it != stack_.end(); ++it) {
        stack_str += StringPrintf("%lu -> ",
                                  static_cast<long unsigned int>(*it));
      }
      LOG(INFO) << "stack: " << stack_str;
    }
  }

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
      if (Circuit(*w, depth + 1)) {
        found = true;
        if ((depth > kMaxEdgesToConsider) || StackContainsCutEdge())
          break;
      }
    }
  }

  if (found) {
    Unblock(vertex);
  } else {
    for (Vertex::SubgraphEdgeMap::iterator w =
             subgraph_[vertex].subgraph_edges.begin();
         w != subgraph_[vertex].subgraph_edges.end(); ++w) {
      if (blocked_graph_[*w].out_edges.find(vertex) ==
          blocked_graph_[*w].out_edges.end()) {
        blocked_graph_[*w].out_edges.insert(make_pair(vertex,
                                                      EdgeProperties()));
      }
    }
  }
  CHECK_EQ(vertex, stack_.back());
  stack_.pop_back();
  return found;
}

}  // namespace chromeos_update_engine
