#include "islutils/common.h"

// READ/WRITE from/to file

// derive the output file from the "in" file name or use 
// the "out" name. In both case we append "mathcers.cpp" at
// the end on the file.
std::ofstream get_output_file(std::string in, std::string out) {

  std::string extension = ".matchers.cpp";
  std::ofstream of;

  in.replace(in.find('.'), in.length(), extension);

  if(out.empty()) {
    out = in;
  } else {
    out.replace(out.find('.'), in.length(), extension);
  }

  LOG(INFO) << "output file name :" << out; 
  of.open(out);
  return of;
}

// write to file
void write_on_file(std::string s, std::ofstream &o) {
  if(o.is_open()) {
    o << s;
  }
}

// read from file.
std::string read_from_file(std::string in) {
  std::stringstream buffer;
  std::ifstream t(in);
  assert(t && "not able to open the file");
  buffer << t.rdbuf();
  return buffer.str();
}

// REBUILD

isl::schedule_node rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement) {
  // this may not be always legal...
  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}

// FIND AND REPLACE

isl::schedule_node
replaceOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
replaceDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement) {
  node = replaceRepeatedly(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderRepeatedly(node.child(i), pattern, replacement)
               .parent();
  }
  return node;
}

isl::schedule_node
replaceDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement) {
  std::cout << node.to_str() << std::endl;
  node = replaceOnce(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderOnce(node.child(i), pattern, replacement).parent();
  }
  return node;
}


// GENERAL OPT.

inline isl::union_map
filterOutCarriedDependences(isl::union_map dependences,
                            isl::schedule_node node) {
  auto partialSchedule = node.get_prefix_schedule_multi_union_pw_aff();
  return dependences.eq_at(partialSchedule);
}

// can we merge band nodes?
bool canMerge(isl::schedule_node parentBand,
                     isl::union_map dependences) {
  // Permutability condition: there are no negative distances along the
  // dimensions that are not carried until now by any of dimensions.
  auto t1 = parentBand.band_get_partial_schedule();
  auto t2 = parentBand.child(0).band_get_partial_schedule();
  auto schedule = isl::union_map::from(t1.flat_range_product(t2));
  auto scheduleSpace = isl::set(schedule.range()).get_space();
  auto positiveOrthant =
      isl::set(isl::basic_set::positive_orthant(scheduleSpace));
  dependences = filterOutCarriedDependences(dependences, parentBand);
  return dependences.apply_domain(schedule)
      .apply_range(schedule)
      .deltas()
      .is_subset(positiveOrthant);
}

// compute deps.

isl::union_map computeAllDependences(const Scop &scop) {
  // For the simplest possible dependence analysis, get rid of reference tags.
  auto reads = scop.reads.domain_factor_domain();
  auto mayWrites = scop.mayWrites.domain_factor_domain();
  auto mustWrites = scop.mustWrites.domain_factor_domain();

  // False dependences (output and anti).
  // Sinks are writes, sources are reads and writes.
  auto falseDepsFlow = isl::union_access_info(mayWrites.unite(mustWrites))
                           .set_may_source(mayWrites.unite(reads))
                           .set_must_source(mustWrites)
                           .set_schedule(scop.schedule)
                           .compute_flow();

  isl::union_map falseDeps = falseDepsFlow.get_may_dependence();

  // Flow dependences.
  // Sinks are reads and sources are writes.
  auto flowDepsFlow = isl::union_access_info(reads)
                          .set_may_source(mayWrites)
                          .set_must_source(mustWrites)
                          .set_schedule(scop.schedule)
                          .compute_flow();

  isl::union_map flowDeps = flowDepsFlow.get_may_dependence();

  return flowDeps.unite(falseDeps);
}

// merge bands together if possible.

isl::schedule_node mergeIfTilable(isl::schedule_node node,
                                  isl::union_map dependences) {
  isl::schedule_node parent, child, grandchild;

  auto canMergeCaptureChild = [&child, dependences](isl::schedule_node node) {
    if (canMerge(node.parent(), dependences)) {
      child = node;
      return true;
    }
    return false;
  };

  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(canMergeCaptureChild,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Use lambdas to lazily initialize the builder with the nodes and values yet
  // to be captured by the matcher.
  auto declarativeMerger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule = [&]() {
      auto descr =
          BandDescriptor(parent.band_get_partial_schedule().flat_range_product(
              child.band_get_partial_schedule()));
      descr.permutable = 1;
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(grandchild); };
    declarativeMerger = band(schedule, subtree(st));
  }

  return replaceDFSPreorderRepeatedly(node, matcher, declarativeMerger);
}

// return the top most band node startint from "node"

isl::schedule_node topmostBand(isl::schedule_node node) {

  assert(node.get() && "expect valid node");

  isl::schedule_node parent, child;

  auto matcher = [&]() {
    using namespace matchers;
    return band(parent, anyTree(child));
  }();

  std::stack<isl::schedule_node> nodeStack;
  nodeStack.push(node);

  while(nodeStack.empty() == false) {
    isl::schedule_node node = nodeStack.top();
    nodeStack.pop();

    if(matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
      return node;
    }
   
    size_t n_children = 
      static_cast<size_t>(isl_schedule_node_n_children(node.get()));
    for(size_t i=0; i<n_children; ++i) {
      nodeStack.push(node.child(i));
    }
  }
  
  LOG(INFO) << "topmostBand returns nullptr, no (or no others) band node found";
  return nullptr;
}


// return string representation for "target"
std::string getStringFromTarget(int target) {

  switch(target) {
    case 1: return "CPU";
    case 2: return "Access Processor";
    case 3: return "GPU";
    default: return "target not defined";
  }
}

// tile node "node" with the given tile size.
// call this fuction after "getScheduleTile"
// Example:
// auto tileSchedule = getScheduleTile(node, tileSizes);
// auto pointSchedule = getSchedulePointTile(node, tileSchedule);

isl::multi_union_pw_aff getSchedulePointTile(isl::schedule_node node,
                                                    isl::multi_union_pw_aff t) {
  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  return sched.sub(t);
}

// tile node "node" with the given tile size "tileSizes"

isl::multi_union_pw_aff getScheduleTile(isl::schedule_node node,
                                               std::vector<int> tileSizes) {
  assert(tileSizes.size() != 0 && "empty tileSizes array");
  isl::space space = isl::manage(isl_schedule_node_band_get_space(node.get()));
  unsigned dims = space.dim(isl::dim::set);
  assert(dims == tileSizes.size() &&
         "number of dimensions should match tileSizes size");

  isl::multi_val sizes = isl::multi_val::zero(space);
  for (unsigned i = 0; i < dims; ++i) {
    int tileSize = tileSizes[i];
    sizes = sizes.set_val(i, isl::val(node.get_ctx(), tileSize));
  }

  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  for (unsigned i = 0; i < dims; ++i) {
    isl::union_pw_aff upa = sched.get_union_pw_aff(i);
    isl::val v = sizes.get_val(i);
    upa = upa.scale_down_val(v);
    upa = upa.floor();
    sched = sched.set_union_pw_aff(i, upa);
  }
  return sched;
}
