#include "highlighter.h"

#include "islutils/error.h"
#include "islutils/matchers.h"
#include "islutils/builders.h"
#include <islutils/loop_opt.h>
#include <islutils/access_patterns.h>
#include <islutils/access.h>

#include <iostream>
#include <regex>        // std::regex
#include <algorithm>    // std::remove_if
#include <fstream>      // std::fstream

using namespace timeInfo;

isl::schedule_node
replace_DFSPreorder_repeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement);

Highlighter::Highlighter(isl::ctx context, 
  QTextDocument *parent) : context_(context), QSyntaxHighlighter(parent),
  opt_(LoopOptimizer()), tuner_(TunerThread(nullptr, context)) {

  qRegisterMetaType<timeInfo::TimingInfo>("TimingInfo");
  QObject::connect(&tuner_, &TunerThread::compareSchedules,
                   this, &Highlighter::updateTime);

  HighlightingRule rule;
  patternFormat_.setForeground(Qt::blue);
  rule.pattern = QRegularExpression(QStringLiteral("pattern"));
  rule.format = patternFormat_;
  highlightingRules.append(rule);
  
  
  tileFormat_.setForeground(Qt::blue);
  rule.pattern = QRegularExpression(QStringLiteral("tile"));
  rule.format = tileFormat_;
  highlightingRules.append(rule);

  interchangeFormat_.setForeground(Qt::blue);
  rule.pattern = QRegularExpression(QStringLiteral("interchange"));
  rule.format = interchangeFormat_;
  highlightingRules.append(rule);

  unrollFormat_.setForeground(Qt::blue);
  rule.pattern = QRegularExpression(QStringLiteral("unroll"));
  rule.format = unrollFormat_;
  highlightingRules.append(rule);

  timeFormat_.setForeground(Qt::blue);
  rule.pattern = QRegularExpression(QStringLiteral("compareWithBaseline"));
  rule.format = timeFormat_;
  highlightingRules.append(rule);

  loopReversalFormat_.setForeground(Qt::blue);
  rule.pattern = QRegularExpression(QStringLiteral("loopReverse")); 
  rule.format = timeFormat_;
  highlightingRules.append(rule);
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

/// Do the accesses satisfy the pattern obtained from the parser?
///
/// @param ctx: Context
/// @param descr_accesses: Accesses obtained from parser.
/// @reads: Reads accesses.
/// @Writes: Write accesses.
static bool check_access_pattern(isl::ctx ctx,
std::vector<Parser::AccessDescriptor> descr_accesses, isl::union_map accesses) {

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

  auto ps = allOf(accesses_list);
  auto matches = match(accesses, ps);
  #ifdef DEBUG
    std::cout << "#matches: " << matches.size() << "\n";
  #endif
  return (matches.size() == 1) ? true : false;
}

/// Are the writes a possible match for the accesses
/// obtained from the parser?
///
/// @param ctx: Context
/// @param descr_accesses: Accesses obtained from parser.
/// @reads: Reads accesses.
/// @Writes: Write accesses.
static bool check_accesses_write(isl::ctx ctx,
std::vector<Parser::AccessDescriptor> write_descr_accesses, isl::union_map writes) {

  bool res = check_access_pattern(ctx, write_descr_accesses, writes);
  #ifdef DEBUG
    std::cout << __func__ << ": " << res << "\n";
    std::cout << "writes: " << writes.to_str() << "\n";
    // FIXME: be consistent with naming.
    std::cout << "#write_descr_accesses: " << write_descr_accesses.size() << "\n";
  #endif
  return res;
}

/// Are the reads a possible match for the accesses
/// obtained from the parser?
///
/// @param ctx: Context
/// @param descr_accesses: Accesses obtained from parser.
/// @reads: Reads accesses.
/// @Writes: Write accesses.
static bool check_accesses_read(isl::ctx ctx,
std::vector<Parser::AccessDescriptor> read_descr_accesses, isl::union_map reads) {
  
  bool res = check_access_pattern(ctx, read_descr_accesses, reads);
  #ifdef DEBUG
    std::cout << __func__ << ": " << res << "\n";
    std::cout << "reads: " << reads.to_str() << "\n";
    std::cout << "#read_descr_accesses: " << read_descr_accesses.size() << "\n";
  #endif
  return res;
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

  std::vector<Parser::AccessDescriptor> read_descr_accesses;
  std::vector<Parser::AccessDescriptor> write_descr_accesses;

  for (size_t i = 0; i < descr_accesses.size(); i++) {
    if (descr_accesses[i].type_ == Parser::Type::READ ||
        descr_accesses[i].type_ == Parser::Type::READ_AND_WRITE)
      read_descr_accesses.push_back(descr_accesses[i]);
    if (descr_accesses[i].type_ == Parser::Type::WRITE ||
        descr_accesses[i].type_ == Parser::Type::READ_AND_WRITE)
      write_descr_accesses.push_back(descr_accesses[i]);
  }
  
  auto res = check_accesses_read(ctx, read_descr_accesses, reads) 
         && check_accesses_write(ctx, write_descr_accesses, writes);
  #ifdef DEBUG
    std::cout << __func__ << ": " << res << "\n";
  #endif
  return res;
}

void Highlighter::update_schedule(isl::schedule new_schedule) {

  if (!current_schedule_.plain_is_equal(new_schedule)) {
    current_schedule_ = new_schedule;
    std::string file_path_as_std_string =
      file_path_.toStdString();
    assert(!file_path_as_std_string.empty()
      && "empty file path!");
    pet::Scop scop = 
      pet::Scop(pet::Scop::parseFile(context_,
        file_path_as_std_string));
    scop.schedule() = new_schedule;
    std::cout << new_schedule.to_str() << "\n";
    std::string code = scop.codegen();
    QString code_as_q_string = QString(code.c_str());
    Q_EMIT codeChanged(code_as_q_string);
  }
}

static bool look_up_schedule_tree(const isl::schedule schedule, const std::string &mark_id) {

  assert(schedule && "empty schedule");
  isl::schedule_node root = schedule.get_root();

  struct payload {
    bool is_marked = false;
    std::string name = "empty";
  } p;
  
  p.name = mark_id;

    isl_schedule_node_foreach_descendant_top_down(
      root.get(),
      [](__isl_keep isl_schedule_node *node_ptr, void *user) -> isl_bool {
        payload *p = static_cast<payload *>(user);

        isl::schedule_node node = isl::manage_copy(node_ptr);
        if (isl_schedule_node_get_type(node.get()) == isl_schedule_node_mark) {
          isl::id id = node.mark_get_id();
          std::string id_as_string = id.to_str();
          if ((id_as_string.compare(p->name) == 0)) {
            p->is_marked = true;
          }
        }
        return isl_bool_true;
      },
      &p);

  return p.is_marked;
}

// FIXME: Here we need to check if the write array is the same as the read one.
bool Highlighter::match_pattern_helper(
const std::vector<Parser::AccessDescriptor> accesses_descriptors, const pet::Scop &scop) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
    std::cout << "Input : " << "\n";
    std::cout << "  #access_descriptors: " << accesses_descriptors.size() << "\n";
  #endif

  isl::schedule schedule = scop.schedule();   
  isl::schedule_node root = schedule.get_root();
  root = squeeze_tree(root);

  // structural properties:
  // # induction variables -> #loops
  size_t dims = extract_inductions(accesses_descriptors).size();
  
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

  auto reads = scop.reads();  
  auto writes = scop.writes();
  auto ctx = scop.getCtx();
  
  auto has_pattern = [&](isl::schedule_node node) {

    // A band node always have a child (may be a leaf), and the prefix schedule
    // of that child includes the partial schedule of the node. 
    auto schedule = node.child(0).get_prefix_schedule_union_map();
    auto filtered_reads = reads.apply_domain(schedule);
    auto filtered_writes = writes.apply_domain(schedule);
    #ifdef DEBUG  
      std::cout << "has_pattern callback " << "\n";
      std::cout << "reads: " << filtered_reads.to_str() << "\n";
      std::cout << "writes: " << filtered_writes.to_str() << "\n";
      std::cout << "node : " << node.to_str() << "\n";
    #endif 
    if (!check_accesses(ctx, accesses_descriptors,
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

  #ifdef DEBUG
    std::cout << "Schedule after pattern matching: " << "\n";
    std::cout << root.root().to_str() << "\n";
  #endif
  // mark the detected pattern.
  root = wrap_DFSPreorder(root, loop_matcher, "_tactic_"); 
  root = unsqueeze_tree(root.child(0));
  // mark the loop in the detected region. The region
  // is labeled with a mark node _tactic_
  root = mark_loop(root, "_tactic_");
  
  isl::schedule new_schedule = root.get_schedule();
  // avoid to update the schedule if not tactic has 
  // been detected!.
  if (!look_up_schedule_tree(new_schedule, "_tactic_"))
    return false;
  update_schedule(new_schedule);  
  return true;
}

void Highlighter::take_snapshot() {

  assert(current_schedule_ 
    && "cannot take a snapshot of an empty schedule!");
  BlockSchedule *bs = new BlockSchedule();
  bs->schedule_block_ = current_schedule_;
  setCurrentBlockUserData(bs);
  return;
}

void Highlighter::match_pattern(const std::string &pattern) {
 
  #ifdef DEBUG
    std::cout << __func__ << "\n";
    std::cout << "Input: " << pattern << "\n";
  #endif 
 
  if (file_path_.isEmpty()) {
    #ifdef DEBUG
      std::cout << __func__ << ": " 
        << "exit as file_path is empty!" << "\n";
    #endif
    return;
  }
  std::string file_path_as_std_string =
    file_path_.toStdString();

  // check if the file path is good.  
  std::ifstream f(file_path_as_std_string);
  if (!f.good()) {
    #ifdef DEBUG
      std::cout << __func__ << " : " 
        << "exit as file_path is not valid!" << "\n";
    #endif
    return;
  }

  bool has_matched = false;
  try {
   pet::Scop scop = pet::Scop(pet::Scop::parseFile(context_, 
     file_path_as_std_string));
    current_schedule_ = scop.schedule();
    std::vector<Parser::AccessDescriptor> 
      accesses_descriptors = Parser::parse(pattern);
    if (accesses_descriptors.size() == 0) {
      return;
    }
    std::cout << "Success in parsing: " << pattern << "\n";
    has_matched = match_pattern_helper(accesses_descriptors, scop);
  } catch (Error::Error e) {
    std::cout << "Error:" << e.message_ << "\n";
    return;
  }
    catch (...) {
    std::cout << "Error: pet cannot open the file!\n";
    return;
  }
  if (has_matched)
    take_snapshot(); 
}

void Highlighter::tile(std::string tile_transformation) { 

  if (tile_transformation.empty())
    return;

  tile_transformation.erase(
    std::remove_if(
      tile_transformation.begin(),
      tile_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), tile_transformation.end());

  std::regex pattern_regex(R"(([a-z_]+),([0-9]+))");
  std::smatch matched_text;
  if (!std::regex_match(tile_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule = 
    opt_.tile(current_schedule_, matched_text[1].str(), std::stoi(matched_text[2].str()));
  update_schedule(new_schedule);
  take_snapshot();
}

void Highlighter::unroll(std::string unroll_transformation) {
  
  if (unroll_transformation.empty())
    return;

  unroll_transformation.erase(
    std::remove_if(
      unroll_transformation.begin(),
      unroll_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), unroll_transformation.end());

  std::regex pattern_regex(R"(([a-z_]+),([0-9]+))");
  std::smatch matched_text;
  if (!std::regex_match(unroll_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule =
    opt_.unroll_loop(current_schedule_, matched_text[1].str(), std::stoi(matched_text[2].str()));
  update_schedule(new_schedule);
  take_snapshot();
}

void Highlighter::interchange(std::string interchange_transformation) {

  if (interchange_transformation.empty())
    return;

  interchange_transformation.erase( 
    std::remove_if(
      interchange_transformation.begin(),
      interchange_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), interchange_transformation.end());

  std::regex pattern_regex(R"(([a-z_]+),([a-z_]+))");
  std::smatch matched_text;
  if (!std::regex_match(interchange_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule =
    opt_.swap_loop(current_schedule_, matched_text[1].str(), matched_text[2].str());
  update_schedule(new_schedule);

  // take a snapshot of the current schedule.
  take_snapshot();
}

void Highlighter::loop_reverse(std::string loop_reverse_transformation) {

  if (loop_reverse_transformation.empty())
    return;

  loop_reverse_transformation.erase(
    std::remove_if(
      loop_reverse_transformation.begin(),
      loop_reverse_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), loop_reverse_transformation.end());

  // FIXME: all this regex are nightmare for maintainability.
  std::regex pattern_regex(R"(([a-z_]+))");
  std::smatch matched_text;
  if (!std::regex_match(loop_reverse_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule =
    opt_.loop_reverse(current_schedule_, matched_text[1].str());
  update_schedule(new_schedule);
  take_snapshot();
}

// The boolean means the following:
// 1 -> compare with the baseline
// 0 -> compare with the previous schedule
void Highlighter::compare(bool with_baseline) {

  assert(with_baseline
    && "not implemented for non baseline");

  std::string file_path_as_std_string =
    file_path_.toStdString();
  
  if (file_path_as_std_string.empty()) {
    #ifdef DEBUG
      std::cout << __func__ << " : " 
        << "file path is empty " << "\n";
    #endif
    return;
  }

  pet::Scop scop =
      pet::Scop(pet::Scop::parseFile(context_,
        file_path_as_std_string));

  isl::schedule baseline_schedule =
    scop.schedule();
    
  // compare current schedule with
  // original one.
  tuner_.compare(baseline_schedule,
    current_schedule_, file_path_);

}  
  
void Highlighter::highlightBlock(const QString &text) {
 
  // FIXME: maybe just reuse the QRegex? 
  std::regex pattern_regex(R"(pattern\[(.*)\])");
  std::regex tile_regex(R"(tile\[(.*)\])");
  std::regex interchange_regex(R"(interchange\[(.*)\])");
  std::regex unroll_regex(R"(unroll\[(.*)\])");
  std::regex loop_reverse_regex(R"(loopReverse\[(.*)])");
  std::regex time_regex(R"(compareWithBaseline)");
  std::smatch matched_text;

  auto *current_block_data = currentBlockUserData();
  if (current_block_data) {
    BlockSchedule *payload = dynamic_cast<BlockSchedule*>(current_block_data);
    if (payload) {
      #ifdef DEBUG
      std::cout << __func__ << " : "
        << "Current block schedule -> " 
        << payload->schedule_block_.to_str() << "\n";
      std::cout << "Current text: " << text.toStdString() << "\n";
      #endif 
      update_schedule(payload->schedule_block_);
    }
  }
    
  for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
      while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        std::string tmp = text.toStdString();    
        if (std::regex_match(tmp, matched_text, pattern_regex)) {
          match_pattern(matched_text[1].str());
        }
        if (std::regex_match(tmp, matched_text, tile_regex))
          tile(matched_text[1].str());
        if (std::regex_match(tmp, matched_text, unroll_regex))
          unroll(matched_text[1].str());
        if (std::regex_match(tmp, matched_text, interchange_regex))
          interchange(matched_text[1].str());
        if (std::regex_match(tmp, matched_text, loop_reverse_regex))
          loop_reverse(matched_text[1].str());
        if (std::regex_match(tmp, matched_text, time_regex))
          compare(true);
      }
  }
}

void Highlighter::updatePath(const QString &path) {
  #ifdef DEBUG  
    std::cout << __func__ << " : "
      << path.toStdString() << "\n";
  #endif
  file_path_ = path;
}

void Highlighter::updateTime(
const TimingInfo &baseline_time, const TimingInfo &opt_time) {

    Q_EMIT userFeedbackChanged(baseline_time, opt_time);
}
