#include <islutils/loop_opt.h>
#include <stack>
#include <islutils/builders.h>
#include <islutils/matchers.h>

#include <iostream>

using namespace LoopTactics;

static isl::schedule_node mark_only_loop_subtree(isl::schedule_node node) {

  std::stack<isl::schedule_node> node_stack;
  node_stack.push(node);

  while (node_stack.empty() == false) {
  
    node = node_stack.top();
    node_stack.pop();

    if (isl_schedule_node_get_type(node.get()) == isl_schedule_node_band) {
      assert(isl_schedule_node_band_n_member(node.get()) == 1);
      isl::union_map partial_schedule =
        isl::union_map::from(node.band_get_partial_schedule());
      isl::map partial_schedule_as_map = 
        isl::map::from_union_map(partial_schedule);
      if (!partial_schedule_as_map.has_tuple_id(isl::dim::out))
        assert(0 && "not tuple id");
      std::cout << partial_schedule_as_map.get_tuple_id(isl::dim::out).to_str() << "\n";
      assert(0);
    }

    size_t n_children =
      static_cast<size_t>(isl_schedule_node_n_children(node.get()));
    for (size_t i = 0; i < n_children; i++) {
      node_stack.push(node.child(i));
    }
  }
  return node;
}
  
static isl::schedule_node mark_only_loop(isl::schedule_node node,
  const std::string &mark_id) {

  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
      && (node.mark_get_id().to_str().compare(mark_id) == 0)) {
    node = mark_only_loop_subtree(node.child(0));
  }

  for (int i = 0; i < node.n_children(); i++) {
    node = mark_only_loop(node.child(i), mark_id);
  }
  return node;
}

/// utility function.
///
/// @param node: Current node where to start cutting.
/// @param replacement: Subtree to be attached after @p node.
/// @return: Root node of the rebuild subtree.
///
/// NOTE: This is not always possible. Cutting children
/// in set or sequence is not allowed by ISL and as a consequence
/// by Loop Tactics.
static isl::schedule_node
rebuild(isl::schedule_node node,
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
static isl::schedule_node replace_DFSPreorder_repeatedly(
    isl::schedule_node node, const matchers::ScheduleNodeMatcher &pattern,
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
static isl::schedule_node
replace_once(isl::schedule_node node,
             const matchers::ScheduleNodeMatcher &pattern,
             const builders::ScheduleNodeBuilder &replacement) {

  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
    // std::cout << node.to_str() << "\n";
    // std::cout << node.child(0).child(0).to_str() << "\n";
    // assert(0);
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
static isl::schedule_node
replace_DFSPreorder_once(isl::schedule_node node,
                         const matchers::ScheduleNodeMatcher &pattern,
                         const builders::ScheduleNodeBuilder &replacement) {

  node = replace_once(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); i++) {
    node =
        replace_DFSPreorder_once(node.child(i), pattern, replacement).parent();
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

  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark) &&
      (node.mark_get_id().to_str().compare(node_id) == 0)) {
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
static std::pair<isl::multi_union_pw_aff, isl::multi_union_pw_aff>
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

/// utility function.
static __isl_give isl_schedule_node *
unsqueeze_band(__isl_take isl_schedule_node *node, void *user) {

  if (isl_schedule_node_get_type(node) != isl_schedule_node_band)
    return node;

  if (isl_schedule_node_band_n_member(node) == 1)
    return node;
 
  size_t members = isl_schedule_node_band_n_member(node);
  for (size_t i = 1; i < members; i++) {
    node = isl_schedule_node_band_split(node, 1);
    node = isl_schedule_node_child(node, 0);
  }
  return node;
}

/// Un-squeeze the schedule tree.
/// given a schedule tree that looks like
///
/// schedule (i, j)
///
/// this function will give
///
/// schedule(i)
///   schedule(j)
///
/// @param root: Current root node for the subtree to simplify.
static isl::schedule_node unsqueeze_tree(isl::schedule_node root) {

  root = isl::manage(isl_schedule_node_map_descendant_bottom_up(
    root.release(), unsqueeze_band, nullptr));  
  return root;
}

/// Squeeze the schedule tree.
/// given a tree that looks like
///
/// schedule (i)
///    schedule (j)
///      anyTree
///
/// this will get simplify as
///
/// schedule(i,j)
///   anyTree
///
/// @param schedule_node: Current schedule node to be simplified.
static isl::schedule_node squeeze_tree(isl::schedule_node root) {

  isl::schedule_node parent, child, grandchild;
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
      band(child,
        anyTree(grandchild)));
    //clang-format on
  }();
    
  auto merger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    // clang-format off
    auto computeSched = [&]() {
      isl::multi_union_pw_aff sched =
        parent.band_get_partial_schedule().flat_range_product(
          child.band_get_partial_schedule());
      return sched;
    };
    // clang-format on
    auto st = [&]() { return subtreeBuilder(grandchild); };
    merger = band(computeSched, subtree(st));
  }

  root = replace_DFSPreorder_repeatedly(root, matcher, merger);
  return root.root();
}

static isl::schedule_node remove_loop_mark(isl::schedule_node node) {

  isl::schedule_node band_node;
  isl::schedule_node continuation;

  auto matcher = [&]() {
    using namespace matchers;
    return mark(band(band_node, anyTree(continuation)));
  }();
  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto new_schedule = [&]() {
      return band_node.band_get_partial_schedule();
    };
    auto st = [&]() { return subtreeBuilder(continuation); };
    builder = band(new_schedule, subtree(st));
  }
  node = replace_DFSPreorder_once(node, matcher, builder);
  return node;
}
/*
static bool is_valid_fusion(isl::schedule_node stmt_one, isl::schedule_node stmt_two) {

  auto *id_first = isl_schedule_node_mark_get_id(stmt_one.get());
  auto *pp_first = static_cast<ReadsWrites *>(isl_id_get_user(id_first));
  auto reads_first_stmt = pp_first->reads;
  delete pp_first;
  auto *id_second = isl_schedule_node_mark_get_id(stmt_two.get());
  auto *pp_second = static_cast<ReadsWrites *>(isl_id_get_user(id_second));
  auto reads_second_stmt = pp_second->reads;
  delete pp_second;
  isl_id_free(id_first);
  isl_id_free(id_second);  

  // check if the kernels are independent
  
  return true;
}

static isl::schedule_node fuse_helper(isl::schedule_node node) {

  isl::schedule_node domain_node;
  isl::schedule_node first_band_to_be_fused, second_band_to_be_fused;
  isl::schedule_node first_filter_node, second_filter_node;
  isl::schedule_node first_mark_stmt, second_mark_stmt;

  auto matcher = [&]() {
    using namespace matchers; 
    return
      domain(domain_node,
        sequence(
          filter(first_filter_node,
            mark(band(first_band_to_be_fused, mark(first_mark_stmt, leaf())))),
          filter(second_filter_node,
            mark(band(second_band_to_be_fused, mark(second_mark_stmt, leaf()))))));
  }();

  if (!matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
    return node;
  }

  if (!is_valid_fusion(first_mark_stmt, second_mark_stmt)) {
    return node;
  }

  auto p1 = first_band_to_be_fused.child(0).get_prefix_schedule_union_map();
  auto p2 = second_band_to_be_fused.child(0).get_prefix_schedule_union_map();
  auto mupa1 = isl::multi_union_pw_aff::from_union_map(p1);
  auto mupa2 = isl::multi_union_pw_aff::from_union_map(p2);
  auto fused_schedule = mupa1.union_add(mupa2);

  auto new_root = [&]() {
    using namespace builders;

    auto marker = [&]() {
      return 
        isl::id::alloc(first_band_to_be_fused.get_ctx(), "_tactic_", nullptr);
    };

    auto builder =
      domain(domain_node.domain_get_domain(),
        mark(marker,
          band(fused_schedule,
            sequence(filter(first_filter_node.filter_get_filter()),
                     filter(second_filter_node.filter_get_filter())))));
    return builder.build();
  }();
  
  return new_root;
}
*/
/// Fuse.
isl::schedule LoopOptimizer::fuse(isl::schedule schedule,
                                  const std::string &stmt_one,
                                  const std::string &stmt_two) {
  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif
/*
  if (stmt_one == stmt_two)
    return schedule;

  isl::schedule_node root = schedule.get_root();
  root = remove_loop_mark(root);
  root = squeeze_tree(root); 
  root = fuse_helper(root);
  root = unsqueeze_tree(root);
  std::cout << "ROOT: " << root.to_str() << "\n";
  root = mark_only_loop(root, "_tactic_");
  return root.root().get_schedule();
*/
  return schedule;
}

/// Tiling.
isl::schedule LoopOptimizer::tile(isl::schedule schedule,
                                  const std::string &loop_id,
                                  const int &tile_size) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif

  if (tile_size <= 1)
    return schedule;

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
    return mark(mark_node, band(has_loop, band_node, anyTree(sub_tree)));
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
                  mark(marker_point, band(scheduler_point, subtree(st)))));
  }

  root = replace_DFSPreorder_once(root, matcher, builder);
  root = sink_point_tile(root, loop_id + "_p");
  return root.root().get_schedule();
}

/// Simple stack-based walker -> forward.
static isl::schedule_node walker_forward(isl::schedule_node node,
                                         const std::string &mark_id) {

  std::stack<isl::schedule_node> node_stack;
  node_stack.push(node);

  while (node_stack.empty() == false) {
    node = node_stack.top();
    node_stack.pop();

    if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark) &&
        (node.mark_get_id().to_str().compare(mark_id) == 0)) {
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
static isl::schedule_node walker_backward(isl::schedule_node node,
                                          const std::string &mark_id) {

  std::stack<isl::schedule_node> node_stack;
  node_stack.push(node);

  while (node_stack.empty() == false) {
    node = node_stack.top();
    node_stack.pop();

    if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark) &&
        (node.mark_get_id().to_str().compare(mark_id) == 0)) {
      return node;
    }

    node_stack.push(node.parent());
  }

  assert(0 && "node is expected");
  return nullptr;
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

static isl::schedule_node helper_builder_callback(isl::schedule_node node,
                                           isl::schedule_node band_node,
                                           isl::schedule_node sub_tree) {

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
                                       const std::string &loop_source,
                                       const std::string &loop_destination) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif

  isl::schedule_node node = schedule.get_root();

  auto has_loop = [&](isl::schedule_node band) {
    auto mark = band.parent();
    auto mark_id = mark.mark_get_id().to_str();
    if (mark_id.compare(loop_source) == 0 ||
        mark_id.compare(loop_destination) == 0) {
      return true;
    }
    return false;
  };

  // FIXME: We look also on descendant, but we may
  // need to look at the parent(s).
  isl::schedule_node mark_node_upper, band_node_upper;
  isl::schedule_node mark_node_lower, band_node_lower;
  isl::schedule_node sub_tree_upper, sub_tree_lower;
  auto matcher = [&]() {
    using namespace matchers;
    return mark(
        mark_node_upper,
        band(_and(has_loop, hasDescendant(mark(mark_node_lower,
                                               band(has_loop, band_node_lower,
                                                    anyTree(sub_tree_lower))))),
             band_node_upper, anyTree(sub_tree_upper)));
  }();

  // this function is called after the matching.
  // This function performs the swapping.
  auto builder_callback = [&](isl::schedule_node node) {
    // delete current mark node and insert the newer one.

    #ifdef DEBUG
      std::cout << "entry point builder callback: \n";
      std::cout << node.to_str() << "\n";
    #endif

    node = isl::manage(isl_schedule_node_delete(node.release()));
    node = node.insert_mark(mark_node_lower.mark_get_id());
    node =
        helper_builder_callback(node.child(0), band_node_lower, sub_tree_upper);
    node = walker_forward(node, mark_node_lower.mark_get_id().to_str());
    node = isl::manage(isl_schedule_node_delete(node.release()));
    node = node.insert_mark(mark_node_upper.mark_get_id());
    node =
        helper_builder_callback(node.child(0), band_node_upper, sub_tree_lower);
    // we walk back to the *mark_node_lower* that is now 
    // before *mark_node_upper*
    node = walker_backward(node, mark_node_lower.mark_get_id().to_str());

    #ifdef DEBUG
      std::cout << "exit point builder callback: \n";
      std::cout << node.to_str() << "\n";
    #endif

    return node;
  };

  node = swapper(node, matcher, builder_callback);
  return node.root().get_schedule();
}

/// helper fuction for unrolling.
/// node is the target band node to be unrolled.
/// unroll factor is the unroll factor for the band.
/// domain is the schedule domain.
///
/// if the unroll factor is > than the loop dimension
/// we unroll the entire loop. If this is not the case
/// we stripmine the loop and unroll the newly created loop.
static isl::schedule_node unroller(isl::schedule_node node,
                                 const int &unroll_factor,
                                 const isl::union_set domain) {

  #ifdef DEBUG
    std::cout << "****" <<  __func__ << "****" << "\n";
    std::cout << "Entry node: \n";
    std::cout << node.to_str() << "\n";
  #endif

  assert(isl_schedule_node_get_type(node.get()) == isl_schedule_node_band &&
         "expect band node");

  if (unroll_factor == 1)
    return node;

  auto partial_schedule = node.band_get_partial_schedule_union_map();
  assert(partial_schedule.n_map() == 1 && "expect only single map");
  partial_schedule = partial_schedule.intersect_domain(domain);

  isl::set set = isl::set(partial_schedule.range());
  isl::pw_aff pwa = set.dim_max(0);
  assert(pwa.n_piece() == 1 && "expect single piece for pwa");

  isl::val val;
  pwa.foreach_piece([&val](isl::set s, isl::aff aff) -> isl_stat {
    val = aff.get_constant_val();
    return isl_stat_ok;
  });

  int max_unroll_factor =
      std::stoi(val.add(isl::val::one(val.get_ctx())).to_str());

  if (unroll_factor >= max_unroll_factor) {
    node = node.band_set_ast_build_options(
        isl::union_set(node.get_ctx(), "{unroll[x]}"));
    #ifdef DEBUG 
      std::cout << "**** " << __func__ << "****\n"; 
      std::cout << node.to_str() << "\n";
      std::cout << "**** exit " << __func__ << "****\n";
    #endif
    return node;
  }

  else {
    // stripmine and unroll.
    auto space = isl::manage(isl_schedule_node_band_get_space(node.get()));
    auto dims = space.dim(isl::dim::set);
    auto sizes = isl::multi_val::zero(space);
    for (unsigned i = 0; i < dims; i++) {
      sizes = sizes.set_val(i, isl::val(node.get_ctx(), unroll_factor));
    }
    node = isl::manage(
        isl_schedule_node_band_tile(node.release(), sizes.release()));
    node = node.child(0);
    node = node.band_set_ast_build_options(
        isl::union_set(node.get_ctx(), "{unroll[x]}"));
    #ifdef DEBUG
      std::cout << "****" << __func__ << "****\n";
      std::cout << node.to_str() << "\n";
      std::cout << "**** exit " << __func__ << "****\n";
    #endif
    // note here we return .parent()  
    // as we have introduced a new loop.
    return node.parent();
  }
}

/// helper function that helps to visit all the tree.
/// In case we detected the target loop we unroll it
/// using the helper fuction unroller.
static isl::schedule_node helper_unroll(isl::schedule_node node,
  const std::string &loop_id, const int &unroll_factor,
  const isl::union_set domain) {

  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark) 
      && node.mark_get_id().to_str().compare(loop_id) == 0) {
    node = unroller(node.child(0), unroll_factor, domain);
  }

  for (int i = 0; i < node.n_children(); i++) {
    node = helper_unroll(
      node.child(i), loop_id, unroll_factor, domain).parent();
  }
  return node;
}

/// unroll the loop.
/// schedule is the target schedule.
/// loop id is the loop identifier 
/// unroll factor is the unroll factor for the target loop.
isl::schedule LoopOptimizer::unroll_loop(isl::schedule schedule,
                                         const std::string &loop_id,
                                         const int &unroll_factor) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif

  isl::schedule_node node = schedule.get_root();
  isl::union_set domain = node.domain_get_domain();
  
  node = helper_unroll(node, loop_id, unroll_factor, domain);
  return node.root().get_schedule();
}

/// Helper function for loop reverse.
/// It uses matchers/buidlers to perform the action.
static isl::schedule_node reverser(isl::schedule_node node) {

  isl::schedule_node band_node, continuation;
  auto matcher = [&]() {
    using namespace matchers;
    return band(band_node, anyTree(continuation));
  }();
  
  if (!matchers::ScheduleNodeMatcher::isMatching(matcher, node))
    assert(0);

  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto computed_schedule = [&]() {
      auto p_schedule = band_node.band_get_partial_schedule();
      auto p_schedule_neg = p_schedule.neg();
      //auto descr = BandDescriptor(band_node);
      //descr.partialSchedule = p_schedule_neg;
      return p_schedule_neg;
    };
    auto st = [&]() { return subtreeBuilder(continuation); };
    builder = band(computed_schedule, subtree(st));
  }

  node = node.cut();
  node = builder.insertAt(node);
  return node;
} 

/// Helper function for loop reverse. It is used to
/// walk all the node of the schedule tree.
static isl::schedule_node helper_reverse(isl::schedule_node node,
  const std::string &loop_id) {

  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
      && node.mark_get_id().to_str().compare(loop_id) == 0) {
    node = reverser(node.child(0));
  }

  for (int i = 0; i < node.n_children(); i++) {
    node = helper_reverse(
      node.child(i), loop_id).parent();
  }
  return node;
}    

/// Loop reverse.
isl::schedule LoopOptimizer::loop_reverse(isl::schedule schedule,
                                          const std::string &loop_id) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif

  isl::schedule_node node = schedule.get_root();
  node = helper_reverse(node, loop_id);
  return node.root().get_schedule();
}
