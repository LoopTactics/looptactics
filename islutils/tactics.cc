#include "islutils/tactics.h"

using namespace LoopTactics;
//using namespace std;

/// Constructor
Tactics::Tactics(const std::string id, const std::string pattern, const std::string path_to_file) : 
  program_(path_to_file), tactics_id_(id) {

  accesses_descriptors_ = Parser::parse(pattern);
  if (accesses_descriptors_.size() == 0)
    throw Error::Error("empty accesses array!");

  current_schedule_ = program_.schedule();
}

/// Extract induction variables from the arrays obtained from the parser.
/// i.e., C(i, j) += A(i, k) * B(k, j) will return i, j and k
///
/// @param accesses: Accesses returned by the parser.
/// @return: Set of *unique* induction variables.
static std::set<std::string> extract_inductions(std::vector<Parser::AccessDescriptor> accesses) {

  std::set<std::string> result{};
  for (size_t i = 0; i < accesses.size(); i++) {
    std::set<std::string> tmp = accesses[i].induction_vars_;
    for (auto it = tmp.begin(); it != tmp.end(); it++) {
      result.insert(*it);
    }
  }
  return result;
}

/// Extract array name from the arrays obtained from parser.
/// i.e., C(i, j) += A(i, k) * B(k, j) will return A, B and C
///
/// @param accesses: Accesses returned by the parser.
/// @return: Set of *unique* array names.
static std::set<std::string> extract_array_names(std::vector<Parser::AccessDescriptor> accesses) {
  
  std::set<std::string> result{};  
  for (size_t i = 0; i < accesses.size(); i++) {
    result.insert(accesses[i].name_);
  }
  return result;
}

/// Are the reads and writes a possible match for the accesses
/// obtained from the parser?
///
/// @param ctx: Context
/// @param descr_accesses: Accesses obtained from parser.
/// @reads: Reads accesses.
/// @Writes: Write accesses.
static bool check_accesses(isl::ctx ctx, 
std::vector<Parser::AccessDescriptor> descr_accesses, isl::union_map reads, 
isl::union_map writes) {

  assert(descr_accesses.size() != 0 && "empty user provided accesses!");
  assert(reads.n_map() != 0 && "empty reads");
  assert(writes.n_map() != 0 && "empty writes");

  using namespace matchers;
  using Placeholder = Placeholder<SingleInputDim,UnfixedOutDimPattern<SimpleAff>>;
  using Access = ArrayPlaceholderList<SingleInputDim, FixedOutDimPattern<SimpleAff>>;

  struct Placeholder_set {
    Placeholder p;
    std::string id;
  };
  struct Array_placeholder_set {
    ArrayPlaceholder p;
    std::string id;
  };

  std::vector<Placeholder_set> vector_placeholder_set{};
  std::vector<Array_placeholder_set> vector_array_placeholder_set{};
  std::vector<Access> accesses_list{};
  std::set<std::string> inductions_set = extract_inductions(descr_accesses);
  std::set<std::string> array_names_set = extract_array_names(descr_accesses);

  for (auto it = inductions_set.begin(); it != inductions_set.end(); it++) {
    Placeholder_set tmp = {placeholder(ctx), *it};
    vector_placeholder_set.push_back(tmp);
  }
  
  for (auto it = array_names_set.begin(); it != array_names_set.end(); it++) {
    Array_placeholder_set tmp = {arrayPlaceholder(), *it};
    vector_array_placeholder_set.push_back(tmp);
  }

  auto find_index_in_arrays = [&vector_array_placeholder_set](const std::string id) {
    for (size_t i = 0; i < vector_array_placeholder_set.size(); i++) {
      const Array_placeholder_set tmp = vector_array_placeholder_set[i];
      if (tmp.id.compare(id) == 0)
        return i;
    }
    assert(0 && "cannot find array id in array placeholder");
  };

  auto find_index_in_placeholders = [&vector_placeholder_set](const std::string id) {
    for (size_t i = 0; i < vector_placeholder_set.size(); i++) {
      const Placeholder_set tmp = vector_placeholder_set[i];
      if (tmp.id.compare(id) == 0)
        return i; 
    }
    assert(0 && "cannot find placeholder id in placeholders");
  };

  // build the accesses.
  for (size_t i = 0; i < descr_accesses.size(); i++) {

    size_t dims = descr_accesses[i].induction_vars_.size();
  
    switch (dims) {
      case 1 : {
        size_t index_in_array_placeholder = 
          find_index_in_arrays(descr_accesses[i].name_);
        auto it = descr_accesses[i].induction_vars_.begin();
        size_t index_in_placeholder_dim_zero = 
          find_index_in_placeholders(*it);

        accesses_list.push_back(access(
          vector_array_placeholder_set[index_in_array_placeholder].p,
          vector_placeholder_set[index_in_placeholder_dim_zero].p));
 
        break;
      }
      case 2: {
        size_t index_in_array_placeholder = 
          find_index_in_arrays(descr_accesses[i].name_);
        auto it = descr_accesses[i].induction_vars_.begin();
        size_t index_in_placeholder_dim_zero =
          find_index_in_placeholders(*it);
        it++;
        size_t index_in_placeholder_dim_one =
          find_index_in_placeholders(*it);

        accesses_list.push_back(access(
          vector_array_placeholder_set[index_in_array_placeholder].p,
          vector_placeholder_set[index_in_placeholder_dim_zero].p,
          vector_placeholder_set[index_in_placeholder_dim_one].p));

        break;
      }
      default :
        assert(0 && "Can only handle 1d and 2d array");
    }

  } 

  auto ps_read = allOf(accesses_list);
  auto read_matches = match(reads, ps_read);
  return (read_matches.size() == 1) ? true : false;
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

/// utility function.
isl::schedule_node
wrap(isl::schedule_node node, 
     const matchers::ScheduleNodeMatcher &pattern,
     const std::string &tactics_id) {

  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = node.insert_mark(isl::id::alloc(node.get_ctx(), tactics_id, nullptr));
  }
  return node;
}

/// walk the schedule tree starting from "node" and 
/// in case of a match with the matcher "pattern"
/// wrap the matched subtree with a mark node with id 
/// "tactics_id".
///
/// @param node: Root of the subtree to inspect.
/// @param pattern: Pattern to look-up in the subtree
/// @param tactics_id: id for the mark node
isl::schedule_node
wrap_DFSPreorder(isl::schedule_node node,
                 const matchers::ScheduleNodeMatcher &pattern,
                 const std::string &tactics_id) {

  node = wrap(node, pattern, tactics_id);
  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark))
    return node;
  for (int i = 0; i < node.n_children(); i++) {
    node = wrap_DFSPreorder(node.child(i), pattern, tactics_id).parent();
  }
  return node;
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
isl::schedule_node squeeze_tree(isl::schedule_node root) {

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

/// utility function.
__isl_give isl_schedule_node *
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
isl::schedule_node unsqueeze_tree(isl::schedule_node root) {

  root = isl::manage(isl_schedule_node_map_descendant_bottom_up(
    root.release(), unsqueeze_band, nullptr));  
  return root;
}

/// Given a partial schedule as **string**
/// return the loop id.
std::string get_loop_id(std::string partial_schedule) {

  auto f = [](unsigned char const c) { return std::isspace(c); };
  partial_schedule.erase(std::remove_if(
                                  partial_schedule.begin(),
                                  partial_schedule.end(),
                                  f), partial_schedule.end());
 
  std::string delimiter = "->";
  partial_schedule = 
    partial_schedule. 
      substr(partial_schedule.find(delimiter), partial_schedule.length()); 
  std::smatch match;
  std::regex regex_loop_id(R"(\[([a-z]+)\])");
  std::regex_search(partial_schedule, match, regex_loop_id);
  if (match.size() != 2) {
    std::cout << "#matches : " << match.size() << "\n";
    std::cout << "schedule : " << partial_schedule << "\n";
    assert(0 && "expect only two matches");
  }
  return match[1].str();
}

/// Mark band node in the subtree rooted at "node".
/// Each band node is marked with an id that is the name of
/// the outermost dimension of the partial schedule contained in
/// the band.
isl::schedule_node mark_loop_subtree(isl::schedule_node node, bool insert) {

  if (insert)
    node = node.insert_mark(isl::id::alloc(node.get_ctx(), "start", nullptr)); 

  if (isl_schedule_node_get_type(node.get()) == isl_schedule_node_band) {
    if (isl_schedule_node_band_n_member(node.get()) != 1)
      assert(0 && "expect unsqueeze tree!"); 
    isl::union_map partial_schedule = 
      isl::union_map::from(node.band_get_partial_schedule());
    isl::map partial_schedule_as_map = isl::map::from_union_map(partial_schedule);
    //XXX: we use regex to get the loop name:
    // S_1[i, k, j] -> [i] return i
    std::string loop_id = get_loop_id(partial_schedule_as_map.to_str());
    node = 
      node.insert_mark(isl::id::alloc(node.get_ctx(), loop_id, nullptr)).child(0);  
  }
 
  for (int i = 0; i < node.n_children(); i++) {
    node = mark_loop_subtree(node.child(i), false).parent();
  }
  
  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
      && (node.mark_get_id().to_str().compare("start") == 0)) {
    node = isl::manage(isl_schedule_node_delete(node.release()));
    return node.parent();
  }

  if (isl_schedule_node_get_type(node.get()) == isl_schedule_node_band)
    node = node.parent();

  return node;
}

/// look for a subtree with a mark node as a root.
/// the mark node should have id "mark_id"
isl::schedule_node mark_loop(isl::schedule_node node, const std::string &mark_id) {

  if ((isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark)
      && (node.mark_get_id().to_str().compare(mark_id) == 0)) {
    return mark_loop_subtree(node.child(0), true);
  }

  for (int i = 0; i < node.n_children(); i++) {
    node = mark_loop(node.child(i), mark_id).parent();
  }
  return node;
} 

void Tactics::match() {

  isl::schedule_node root = current_schedule_.get_root().child(0);
  root = squeeze_tree(root);

  // structural properties:
  // # induction variables -> # loops
  size_t dims = extract_inductions(accesses_descriptors_).size();

  auto has_conditions = [&](isl::schedule_node band) {
    isl::union_map schedule =
      isl::union_map::from(band.band_get_partial_schedule());
    if (schedule.n_map() != 1)
      return false;
    isl::map schedule_as_map = 
      isl::map::from_union_map(schedule);
    if (schedule_as_map.dim(isl::dim::in) != dims)
      return false;
    return true;
  };

  auto reads = program_.reads();
  auto writes = program_.writes();
  auto accesses_descr = accesses_descriptors_;
  auto ctx = program_.scop_.getCtx();

  auto has_pattern = [&](isl::schedule_node node) {

    // A band node always have a child (may be a leaf), and the prefix schedule
    // of that child includes the partial schedule of the node. 
    auto schedule = node.child(0).get_prefix_schedule_union_map();
    auto filtered_reads = reads.apply_domain(schedule);
    auto filtered_writes = writes.apply_domain(schedule);
    if (!check_accesses(ctx, accesses_descr,
                        filtered_reads,
                        filtered_writes)) {
      return false;
    }
    return true;
  };

  isl::schedule_node subTree;
  auto loop_matcher = [&]() {
    using namespace matchers;
    return
      band(_and(has_conditions, has_pattern),
        anyTree(subTree));
  }();

  root = wrap_DFSPreorder(root, loop_matcher, tactics_id_);
  root = unsqueeze_tree(root.child(0));
  root = mark_loop(root, tactics_id_);
  current_schedule_ = root.root().get_schedule();
}  

/// Show the generated code for the current schedule.
void Tactics::show() {

  program_.scop_.schedule() = current_schedule_;
  std::cout << program_.scop_.codegen() << std::endl;
}

/// Apply the tiling transformation.
std::pair<isl::multi_union_pw_aff, isl::multi_union_pw_aff>
tile_node(isl::schedule_node node, int tileSize) {

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
static isl::schedule_node sink_point_tile(isl::schedule_node node, std::string node_id) {

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

/// Matcher and builder for tiling.
/// "loop_id" is the mark for the loop that we need to tile.
/// "sizes" is the tile factor.
void Tactics::tile(std::string loop_id, std::vector<int> sizes) {

  isl::schedule_node root = current_schedule_.get_root();

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
      auto new_schedule = tile_node(band_node, 32);
      descr.partialSchedule = new_schedule.first;
      return descr;
    };

    auto scheduler_point = [&]() {
      auto descr = BandDescriptor(band_node);
      auto new_schedule = tile_node(band_node, 32);
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
  current_schedule_ = root.root().get_schedule(); 
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
static isl::schedule_node builder_callback_walker_forward(
isl::schedule_node node, const std::string mark_id) {

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
static isl::schedule_node builder_callback_walker_backward(
isl::schedule_node node, const std::string mark_id) {

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

static isl::schedule_node swap_loop(
isl::schedule_node node, std::string loop_source, std::string loop_destination) {

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
    // use builder to re-build the band node
    auto builder_upper = builders::ScheduleNodeBuilder();
    {
      using namespace builders;
      auto scheduler = [&]() {
        auto descr = BandDescriptor(band_node_upper);
        descr.partialSchedule = band_node_lower.band_get_partial_schedule();
        return descr;
      };
      auto st = [&]() { return subtreeBuilder(sub_tree_upper); };
      builder_upper =
        band(scheduler,
          subtree(st));
    }
    node = node.child(0);
    node = node.cut();
    node = builder_upper.insertAt(node);
    // keep walking the subtree 
    node = builder_callback_walker_forward(node, mark_node_lower.mark_get_id().to_str());
    node = isl::manage(isl_schedule_node_delete(node.release()));
    node = node.insert_mark(mark_node_upper.mark_get_id());
    // use builer to update the band node
    auto builder_lower = builders::ScheduleNodeBuilder();
    {
      using namespace builders;
      auto scheduler = [&]() {
        auto descr = BandDescriptor(band_node_lower);
        descr.partialSchedule = band_node_upper.band_get_partial_schedule();
        return descr;
      };
      auto st = [&]() { return subtreeBuilder(sub_tree_lower); };
      auto builder_lower =
        band(scheduler,
          subtree(st));
    }
    node = node.child(0);
    node = node.cut();
    node = builder_lower.insertAt(node);
    // walk back to the entry point to
    // avoid breaking recursion.
    node = builder_callback_walker_backward(node, mark_node_lower.mark_get_id().to_str());
    return node;
  };

  node = swapper(node, matcher, builder_callback);
  return node;
}

/// Interchange "loop_source" with "loop_destination"
void Tactics::interchange(std::string loop_source, std::string loop_destination) {

  isl::schedule_node root = current_schedule_.get_root().child(0);
  root = swap_loop(root, loop_source, loop_destination);
  current_schedule_ = root.root().get_schedule();
}

