#include <islutils/loop_opt.h>

#include <islutils/matchers.h>
#include <islutils/builders.h>

using namespace LoopTactics;

/// utility function.
///
/// @param node: Current node where to start cutting.
/// @param replacement: Subtree to be attached after @p node.
/// @return: Root node of the rebuild subtree.
///
/// NOTE: This is not always possible. Cutting children
/// in set or sequence is not allowed by ISL and as a consequence
/// by Loop Tactics.
static isl::schedule_node rebuild(isl::schedule_node node,
                           const builders::ScheduleNodeBuilder &replacement) {

  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}


/// utility function.
///
/// @param node: Root of the subtree to inspect.
/// @param pattern: Pattern to look-up in the subtree.
/// @param replacement: Replacement to be applied in case
/// of a match with @p pattern.
static isl::schedule_node
replace_repeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {

  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
    // XXX: if we insert a single mark node, we end up in
    // an infinate loop, since they subtree under the mark will always
    // match the matcher. Escape this skipping the mark node and the
    // root node of the matcher.
    if (isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
      node = node.child(0).child(0);
  }
  return node;
}

/// walk the schedule tree starting from "node" and in
/// case of a match with the matcher "pattern" modify
/// the schedule tree using the builder "replacement".
///
/// @param node: Root of the subtree to inspect.
/// @param pattern: Pattern to look-up in the subtree.
/// @param replacement: Replacement to be applied in case of
/// a match with @p pattern.
isl::schedule_node
replace_DFSPreorder_repeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement) {

  node = replace_repeatedly(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); i++) {
    node = replace_DFSPreorder_repeatedly(node.child(i), pattern, replacement)
               .parent();
  }
  return node;
}


/// utility function.
///
/// @param node: Root of the subtree to inspect.
/// @param pattern: Pattern to look-up in the subtree.
/// @param replacement: Replacement to be applied in case
/// of a match with @p pattern.
isl::schedule_node
replace_once(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement) {

  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
    //std::cout << node.to_str() << "\n";
    //std::cout << node.child(0).child(0).to_str() << "\n";
    //assert(0);
  }
  return node;
}

/// walk the schedule tree starting from "node" and in
/// case of a match with the matcher "pattern" modify
/// the schedule tree using the builder "replacement".
///
/// @param node: Root of the subtree to inspect.
/// @param pattern: Pattern to look-up in the subtree.
/// @param replacement: Replacement to be applied in case of
/// a match with @p pattern.
isl::schedule_node
replace_DFSPreorder_once(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement) {

  node = replace_once(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); i++) {
    node =
        replace_DFSPreorder_once(node.child(i), pattern, replacement)
            .parent();
  }
  return node;
}

/// Sink the band node to the leaf of the subtree root at "node"
/// The newly created mark node has id "mark_id"
static isl::schedule_node sink_mark(isl::schedule_node node, isl::id mark_id) {

  int n_children = node.n_children();
  if (n_children == 0) {
    node = node.insert_mark(mark_id);
    return node;
  }

  for (int i = 0; i < n_children; i++) {
    node = sink_mark(node.child(i), mark_id).parent();
  }
  return node;
}

/// Sink the point tile node. We look for the introduced mark node
/// with id "node_id". We then create a new mark node with the same
/// id at the leaf of the current subtree rooted at "node". We remove
/// the old mark node and we sink the band to the leaf.
static isl::schedule_node sink_point_tile(isl::schedule_node node, 
  const std::string &node_id) {

  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
      && (node.mark_get_id().to_str().compare(node_id) == 0)) {
    node = sink_mark(node, node.mark_get_id());
    node = isl::manage(isl_schedule_node_delete(node.release()));
    node = isl::manage(isl_schedule_node_band_sink(node.release()));
    return node;
  }

  for (int i = 0; i < node.n_children(); i++) {
    node = sink_point_tile(node.child(i), node_id).parent();
  }
  return node;

}

/// Apply the tiling transformation.
std::pair<isl::multi_union_pw_aff, isl::multi_union_pw_aff>
tile_node(isl::schedule_node node, const int &tileSize) {

  auto space = isl::manage(isl_schedule_node_band_get_space(node.get()));
  auto dims = space.dim(isl::dim::set);
  auto sizes = isl::multi_val::zero(space);

  for (unsigned i = 0; i < dims; i++) {
    sizes = sizes.set_val(i, isl::val(node.get_ctx(), tileSize));
  }

  node =
      isl::manage(isl_schedule_node_band_tile(node.release(), sizes.release()));
  auto res = std::make_pair(node.band_get_partial_schedule(),
                            node.child(0).band_get_partial_schedule());
  return res;
}

/// Tiling.
isl::schedule LoopOptimizer::tile(isl::schedule schedule, 
const std::string &loop_id, const int &tile_size) {

  isl::schedule_node root = schedule.get_root();

  auto has_loop = [&](isl::schedule_node band) {
    auto mark = band.parent();
    if (mark.mark_get_id().to_str().compare(loop_id) == 0) {
      return true;
    }
    return false;
  };

  isl::schedule_node band_node;
  isl::schedule_node mark_node;
  isl::schedule_node sub_tree;
  auto matcher = [&]() {
    using namespace matchers;
    return
      mark(mark_node,
        band(has_loop, band_node, anyTree(sub_tree)));
  }();

  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto marker_tile = [&]() {
      return isl::id::alloc(band_node.get_ctx(), loop_id + "_t", nullptr);
    };
    auto marker_point = [&]() {
      return isl::id::alloc(band_node.get_ctx(), loop_id + "_p", nullptr);
    };
    auto scheduler_tile = [&]() {
      auto descr = BandDescriptor(band_node);
      auto new_schedule = tile_node(band_node, tile_size);
      descr.partialSchedule = new_schedule.first;
      return descr;
    };

    auto scheduler_point = [&]() {
      auto descr = BandDescriptor(band_node);
      auto new_schedule = tile_node(band_node, tile_size);
      descr.partialSchedule = new_schedule.second;
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(sub_tree); };

    builder =
      mark(marker_tile,
        band(scheduler_tile,
          mark(marker_point,
            band(scheduler_point, subtree(st)))));
  }

  root = replace_DFSPreorder_once(root, matcher, builder);
  root = sink_point_tile(root, loop_id + "_p");
  return root.root().get_schedule();
}

/// Utility function to perform loop interchange
static isl::schedule_node helper_swapper(
isl::schedule_node node, const matchers::ScheduleNodeMatcher &pattern,
std::function<isl::schedule_node(isl::schedule_node)> builder_callback) {

  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = builder_callback(node);  
  }
  return node;
}

/// Utility function to perform loop interchange.
/// The actual interchange is done via the builder_callback.
static isl::schedule_node swapper(
isl::schedule_node node, const matchers::ScheduleNodeMatcher &pattern,
std::function<isl::schedule_node(isl::schedule_node)> builder_callback) {

  node = helper_swapper(node, pattern, builder_callback);
  for (int i = 0; i < node.n_children(); i++) {
    node = swapper(node.child(i), pattern, builder_callback).parent();
  }
  return node;
}

/// Simple stack-based walker -> forward.
static isl::schedule_node walker_forward(
isl::schedule_node node, const std::string &mark_id) {

  std::stack<isl::schedule_node> node_stack;
  node_stack.push(node);

  while (node_stack.empty() == false) {
    node = node_stack.top();
    node_stack.pop();

    if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
        && (node.mark_get_id().to_str().compare(mark_id) == 0)) {
      return node;
    }

    size_t n_children =
      static_cast<size_t>(isl_schedule_node_n_children(node.get()));
    for (size_t i = 0; i < n_children; i++) {
      node_stack.push(node.child(i));
    }
  }
  assert(0 && "node is expected");
  return nullptr;
}

/// Simple stack-based walker -> backward.
static isl::schedule_node walker_backward(
isl::schedule_node node, const std::string &mark_id) {

  std::stack<isl::schedule_node> node_stack;
  node_stack.push(node);
  
  while(node_stack.empty() == false) {
    node = node_stack.top();
    node_stack.pop();
  
    if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
        && (node.mark_get_id().to_str().compare(mark_id) == 0)) {
      return node;
    }

    node_stack.push(node.parent());
  }

  assert(0 && "node is expected");
  return nullptr;
}

isl::schedule_node helper_builder_callback(
  isl::schedule_node node, isl::schedule_node band_node, isl::schedule_node sub_tree) {

  // TODO: keep not properties.
  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto scheduler = [&]() {
      auto descr = BandDescriptor(band_node.band_get_partial_schedule());
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(sub_tree); };
    builder = band(scheduler, subtree(st));
  }

  node = rebuild(node, builder);
  return node;
}

isl::schedule LoopOptimizer::swap_loop(isl::schedule schedule, 
const std::string &loop_source, const std::string &loop_destination) {

  isl::schedule_node node = schedule.get_root().child(0);

  auto has_loop = [&](isl::schedule_node band) {
    auto mark = band.parent();  
    auto mark_id = mark.mark_get_id().to_str(); 
    if (mark_id.compare(loop_source) == 0 ||
        mark_id.compare(loop_destination) == 0) {  
      return true;
    }
    return false;
  };

  isl::schedule_node mark_node_upper, band_node_upper;
  isl::schedule_node mark_node_lower, band_node_lower;
  isl::schedule_node sub_tree_upper, sub_tree_lower;
  auto matcher = [&]() {
    using namespace matchers;
    return 
      mark(mark_node_upper,
        band(_and(has_loop, 
                  hasDescendant(
                    mark(mark_node_lower, 
                      band(has_loop, band_node_lower, anyTree(sub_tree_lower))))), 
             band_node_upper, anyTree(sub_tree_upper)));
  }();

  // this function is called after the matching.
  // This function performs the swapping.
  auto builder_callback = [&](isl::schedule_node node) {

    // delete current mark node and insert the newer one.
    node = isl::manage(isl_schedule_node_delete(node.release()));
    node = node.insert_mark(mark_node_lower.mark_get_id());
    node = helper_builder_callback(node.child(0), band_node_lower, sub_tree_upper);
    node = walker_forward(node, mark_node_lower.mark_get_id().to_str());
    node = isl::manage(isl_schedule_node_delete(node.release()));
    node = node.insert_mark(mark_node_upper.mark_get_id());
    node = helper_builder_callback(node.child(0), band_node_upper, sub_tree_lower);
    node = walker_backward(node, mark_node_upper.mark_get_id().to_str());
    return node;
  };

  node = swapper(node, matcher, builder_callback);
  return node.root().get_schedule();
}

isl::schedule_node helper_unroll(isl::schedule_node node, const int &unroll_factor) {

  assert(isl_schedule_node_get_type(node.get()) == isl_schedule_node_band 
          && "expect band node");

  if (unroll_factor == 1)
    return node;

  auto partial_schedule = node.band_get_partial_schedule_union_map();
  assert(partial_schedule.n_map() == 1 && "expect only single map");

  isl::set set = isl::set(partial_schedule.range());
  isl::pw_aff pwa = set.dim_max(0);
  assert(pwa.n_piece() == 1 && "expect single piece for pwa");

  isl::val val;
  pwa.foreach_piece([&val](isl::set s, isl::aff aff) -> isl_stat {
    val = aff.get_constant_val();
    return isl_stat_ok;
  });

  int max_unroll_factor = std::stoi(val.add(isl::val::one(val.get_ctx())).to_str());
  
  if (unroll_factor >= max_unroll_factor)
    return node.band_set_ast_build_options(isl::union_set(node.get_ctx(), "{unroll[x]}"));

  else {
    // stripmine and unroll.
    auto space = isl::manage(isl_schedule_node_band_get_space(node.get()));
    auto dims = space.dim(isl::dim::set);
    auto sizes = isl::multi_val::zero(space);
    for (unsigned i = 0; i < dims; i++) {
      sizes = sizes.set_val(i, isl::val(node.get_ctx(), unroll_factor));
    }
    node = isl::manage(isl_schedule_node_band_tile(node.release(), sizes.release()));
    node = node.child(0);
    return node.band_set_ast_build_options(isl::union_set(node.get_ctx(), "{unroll[x]}"));
  }
}

isl::schedule LoopOptimizer::unroll_loop(isl::schedule schedule, 
  const std::string &loop_id, const int &unroll_factor) {

  isl::schedule_node node = schedule.get_root();
  node = walker_forward(node, loop_id);
  node = helper_unroll(node.child(0), unroll_factor);
  isl::union_set set = isl::manage(isl_schedule_node_band_get_ast_build_options(node.get()));
  return node.root().get_schedule();
}

isl::schedule LoopOptimizer::loop_reverse(isl::schedule schedule,
  const std::string &loop_id) {

  isl::schedule_node node = schedule.get_root();
  node = walker_forward(node, loop_id);
  assert(0 && "not implemented yet!");
  
  return schedule;
} 



