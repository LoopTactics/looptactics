#include "islutils/highlighter.h"
#include "islutils/util.h"
#include "islutils/error.h"
#include "islutils/matchers.h"
#include "islutils/builders.h"
#include "islutils/loop_opt.h"
#include "islutils/access_patterns.h"
#include "islutils/access.h"
#include "islutils/gsl/gsl_assert"

#include <iostream>
#include <regex>        // std::regex
#include <algorithm>    // std::remove_if
#include <fstream>      // std::fstream

using namespace userFeedback;
using namespace LoopTactics;

int Highlighter::stmt_id_ = 0;

static bool bad_workaround = true;

static int calls_to_interchange = 0;

Highlighter::Highlighter(isl::ctx context, 
  QTextDocument *parent) : context_(context), QSyntaxHighlighter(parent),
  opt_(LoopOptimizer()), tuner_(TunerThread(nullptr, context)),
  haystackRunner_(nullptr, context) {

  qRegisterMetaType<userFeedback::TimingInfo>("TimingInfo");
  // FIXME: rename compareSchedules
  QObject::connect(&tuner_, &TunerThread::compareSchedules,
                   this, &Highlighter::updateTime);
  qRegisterMetaType<userFeedback::CacheStats>("CacheStats");
  QObject::connect(&haystackRunner_, &HaystackRunner::computedStats,
    this, &Highlighter::updateCacheStats);

  HighlightingRule rule;
  patternFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("pattern"));
  rule.format_ = patternFormat_;
  rule.id_rule_ = 0;
  highlightingRules.append(rule);
   
  tileFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("tile"));
  rule.format_ = tileFormat_;
  rule.id_rule_ = 1;
  highlightingRules.append(rule);

  interchangeFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("interchange"));
  rule.format_ = interchangeFormat_;
  rule.id_rule_ = 2;
  highlightingRules.append(rule);

  unrollFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("unroll"));
  rule.format_ = unrollFormat_;
  rule.id_rule_ = 3;
  highlightingRules.append(rule);

  timeFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("compareWithBaseline"));
  rule.format_ = timeFormat_;
  rule.id_rule_ = 4;
  highlightingRules.append(rule);

  loopReversalFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("loopReverse")); 
  rule.format_ = timeFormat_;
  rule.id_rule_ = 5;
  highlightingRules.append(rule);

  fuseFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("fuse"));
  rule.format_ = fuseFormat_;
  rule.id_rule_ = 6;
  highlightingRules.append(rule);

  cacheEmulatorFormat_.setForeground(Qt::blue);
  rule.pattern_ = QRegularExpression(QStringLiteral("runCacheEmulator"));
  rule.format_ = cacheEmulatorFormat_;
  rule.id_rule_ = 7;
  highlightingRules.append(rule);
}

bool Highlighter::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type()==QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if ( (key->key()==Qt::Key_Enter) || (key->key()==Qt::Key_Return) ) {
            //Enter or return was pressed
        } else {
            return QObject::eventFilter(obj, event);
        }
        return true;
    } else {
        return QObject::eventFilter(obj, event);
    }
    return false;
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

/// utility function.
///
/// @param node: Root of the subtree to inspect.
/// @param pattern: Pattern to look-up in the subtree.
/// @param replacement: Replacement to be applied in case
/// of a match with @p pattern
static isl::schedule_node
replace_once(isl::schedule_node node,
             const matchers::ScheduleNodeMatcher &pattern,
             const builders::ScheduleNodeBuilder &replacement) {

  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
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
/// a match with @p pattern
static isl::schedule_node replace_DFSPreorder_once(
  isl::schedule_node node, const matchers::ScheduleNodeMatcher &pattern,
  const builders::ScheduleNodeBuilder &replacement) {

  node = replace_once(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replace_DFSPreorder_once(node.child(i), pattern, replacement).parent();
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
static isl::schedule_node
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
static isl::schedule_node
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

/// Extract induction variables from the arrays obtained from the parser.
/// i.e., C(i, j) += A(i, k) * B(k, j) will return i, j and k
///
/// @param accesses: Accesses returned by the parser.
/// @return: Set of *unique* induction variables.
static std::set<std::string> extract_inductions(std::vector<Parser::AccessDescriptor> accesses) {

  std::set<std::string> result{};
  for (size_t i = 0; i < accesses.size(); i++) {
    std::vector<Parser::AffineAccess> tmp = accesses[i].affine_access_;
    for (auto it = tmp.begin(); it != tmp.end(); it++) {
      result.insert((*it).induction_var_name_);
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
    result.insert(accesses[i].array_name_);
  }
  return result;
}

/// Given a partial schedule as **string**
/// return the loop id.
static std::string get_loop_id(std::string partial_schedule) {

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
    Expects(match.size() == 2);
  }
  return match[1].str();
}

/// Given a partial schedule as **string** retunr the loop id.
/// For example, for the following partial schedule
/// { S_0[i, j] -> [(i)]; S_1[i, j, k] -> [(i)] }
/// the function returns "i". We also make sure that
/// the output dimension is "i" for all the partial schedules 
/// in the isl::union_map.
static std::string 
get_loop_id_from_partial_schedule(isl::union_map schedule) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif

  if (schedule.n_map() == 1) {
    isl::map schedule_as_map = isl::map::from_union_map(schedule);
    return get_loop_id(schedule_as_map.to_str());
  }

  std::vector<isl::map> schedule_as_map{};  
  schedule.foreach_map([&schedule_as_map](isl::map m) { 
    schedule_as_map.push_back(m); 
    return isl_stat_ok;
  });

  std::string loop_id = get_loop_id(schedule_as_map[0].to_str());  
  for (const auto & s : schedule_as_map) {
    std::string tmp = get_loop_id(s.to_str());
    Expects(tmp == loop_id);
    loop_id = tmp;
  }
  return loop_id;
}

/// Simple stack-based walker. Look for a mark node with id "mark_id"
/*
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
*/

/// look for a subtree with a mark node as a root.
/// the mark node should have id "mark_id"
static isl::schedule_node mark_loop_and_stmt(
  isl::schedule_node node, const std::string &mark_id) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif

  auto has_not_annotation = [&](isl::schedule_node band) {
    if (!band.has_parent())
      return true;
    auto maybe_mark = band.parent();
    if (isl_schedule_node_get_type(maybe_mark.get()) != isl_schedule_node_mark)
      return true;
    // add exception for the band node annotated with _tactic_
    auto mark_id_node = maybe_mark.mark_get_id().to_str();
    if (mark_id_node.compare(mark_id) == 0)
      return true;
    else return false;
  };

  isl::schedule_node band_schedule, continuation;
  auto matcher = [&]() {
    using namespace matchers;
    return
      band(has_not_annotation, band_schedule, anyTree(continuation));
  }();

  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    
    auto marker = [&]() {
      return isl::id::alloc(
        node.get_ctx(), 
        get_loop_id_from_partial_schedule(
          isl::union_map::from(band_schedule.band_get_partial_schedule())),
        nullptr);
    };
    auto original_schedule = [&]() {
      auto descr = BandDescriptor(band_schedule);
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(continuation); };

    builder = mark(marker, band(original_schedule, subtree(st)));
  }

  node = replace_DFSPreorder_once(node, matcher, builder).root();
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

  #ifdef DEBUG
    std::cout << __func__ << std::endl;
    for (auto it = inductions_set.begin(); it != inductions_set.end(); it++) 
      std::cout << "induction : " << *it << "\n";;
    for (auto it = array_names_set.begin(); it != array_names_set.end(); it++)
      std::cout << "array name : " << *it << "\n";
  #endif

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

    size_t dims = descr_accesses[i].affine_access_.size();
  
    switch (dims) {
      case 1 : {
        size_t index_in_array_placeholder = 
          find_index_in_arrays(descr_accesses[i].array_name_);
        auto it = descr_accesses[i].affine_access_.begin();
        size_t index_in_placeholder_dim_zero = 
          find_index_in_placeholders((*it).induction_var_name_);

        accesses_list.push_back(access(
          vector_array_placeholder_set[index_in_array_placeholder].p,
          vector_placeholder_set[index_in_placeholder_dim_zero].p));
 
        break;
      }
      case 2: {
        size_t index_in_array_placeholder = 
          find_index_in_arrays(descr_accesses[i].array_name_);
        auto it = descr_accesses[i].affine_access_.begin();
        size_t index_in_placeholder_dim_zero =
          find_index_in_placeholders((*it).induction_var_name_);
        it++;
        size_t index_in_placeholder_dim_one =
          find_index_in_placeholders((*it).induction_var_name_);

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

  Expects(descr_accesses.size() != 0);
  Expects(reads.n_map() != 0);
  Expects(writes.n_map() != 0);

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

static std::vector<std::string> get_band_node_as_string(isl::schedule schedule) {

  isl::schedule_node root = schedule.get_root();

  struct payload {
    std::vector<std::string> band_as_string;
  } p;

  isl_schedule_node_foreach_descendant_top_down(
    root.get(),
    [](__isl_keep isl_schedule_node *node_ptr, void *user) -> isl_bool {
      payload *p = static_cast<payload *>(user);

      isl::schedule_node node = isl::manage_copy(node_ptr);
      if (isl_schedule_node_get_type(node.get()) == isl_schedule_node_band) {
        p->band_as_string.push_back(node.band_get_partial_schedule().to_str()); 
      }
      return isl_bool_true;
    }, &p);

  return p.band_as_string;
}

static bool are_same_tree(isl::schedule current_schedule,
  isl::schedule new_schedule) {

  Expects(current_schedule);
  Expects(new_schedule);

  if (!current_schedule.plain_is_equal(new_schedule))
    return false;

  auto current_s = get_band_node_as_string(current_schedule);
  auto new_s = get_band_node_as_string(new_schedule);

  if (current_s.size() != new_s.size())
    return false;

  for (size_t i = 0; i < current_s.size(); i++) {
    if (current_s[i] != new_s[i])
      return false;
  }
  return true;
}

void Highlighter::update_schedule(isl::schedule new_schedule, bool update_prev) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
    std::cout << "Input: " << "\n";
    std::cout << "New schedule : " 
      << new_schedule.get_root().to_str() << "\n";
  #endif

  // plain is equal just check that the trees are
  // obviously the same. But it does not capture
  // if the bands have been tiled with different tile sizes.
  if (are_same_tree(current_schedule_, new_schedule))
    return;

  #ifdef DEBUG
    std::cout << "new_schedule != current_schedule_; updating..\n";
  #endif

  if (update_prev)
    previous_schedule_ = current_schedule_;
  current_schedule_ = new_schedule;
  std::string file_path_as_std_string =
    file_path_.toStdString();
  Expects(!file_path_as_std_string.empty());
  pet::Scop scop = 
    pet::Scop(pet::Scop::parseFile(context_,
      file_path_as_std_string));
  scop.schedule() = new_schedule;
  std::string code = scop.codegen();
  QString code_as_q_string = QString(code.c_str());
  Q_EMIT codeChanged(code_as_q_string);
}

static bool look_up_schedule_tree(const isl::schedule schedule, const std::string &mark_id) {

  Expects(schedule);
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
// The structural matchers are derived as follow:
// 1. In case the LHS array is INIT_REDUCTION. We look for 
// an initialization stmt, with the following structure:
//  band(property_one
//    sequence(
//      filter(
//      filter(
//        band(property_two
//          anyTree(
// Property_one is derived from the LHS array only. The # of members in this band
// should be equal to the # of induction variables used in the LHS.
// Property_two is derived from the total number of induction variables. The # of
// members in this band should be equal to the # of induction variables used.
// (note that here we check the input dimension and *not* the output ones)
//
// 2. In case the LHS is ASSIGN or ASSIGNMENT_BY_ADDITION. We look for a trivial nested loop,
// with the following structure:
// band(property_one
//  anyTree(
// Property_one is derived from the total number of induction variables. The
// # if members in this band should be equal to the # of induction variables used.
// (note that here we check the output dimensions and *not* the input ones).
bool Highlighter::match_pattern_helper(
const std::vector<Parser::AccessDescriptor> accesses_descriptors, 
const pet::Scop &scop, bool recompute) {

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
  bool has_init =
    accesses_descriptors[0].type_ == Parser::Type::INIT_REDUCTION ? 1 : 0;
  // FIXME: remove me
  has_init = 1;
  
  auto has_conditions = [&](isl::schedule_node band) {
    isl::union_map schedule =
      isl::union_map::from(band.band_get_partial_schedule());
    if (schedule.n_map() != 1) {
      #ifdef DEBUG
        std::cout << "has_coditions returns false: n_map() != 1\n";
        std::cout << "on band node: \n";
        std::cout << band.to_str() << "\n";
      #endif
      return false;
    }
    isl::map schedule_as_map = 
      isl::map::from_union_map(schedule);
    if (schedule_as_map.dim(isl::dim::in) != dims) {
      #ifdef DEBUG
        std::cout << "ha_conditions returns false: isl::dim::out\n";
        std::cout << "on band node: \n";
        std::cout << band.to_str() << "\n";
      #endif
      return false;
    }
    return true;
  };

  auto has_conditions_init = [&](isl::schedule_node band) {
    isl::union_map schedule =
      isl::union_map::from(band.band_get_partial_schedule());
    if (schedule.n_map() != 2) {
      #ifdef DEBUG
        std::cout << "has_conditions_init returns false: n_map() != 2\n";
        std::cout << "on band node: \n";
        std::cout << band.to_str() << "\n";
      #endif
    }
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

  auto loop_matcher_init = [&]() {
    using namespace matchers;
    return  
      band(
        sequence(
          filter(leaf()),
          filter(
            band(has_pattern, anyTree(subTree)))));
  }();

  #ifdef DEBUG
    std::cout << "Schedule before pattern matching: " << "\n";
    std::cout << root.root().to_str() << "\n";
  #endif
  // mark the detected pattern.
  if (!has_init) {
    #ifdef DEBUG
      std::cout << "not init stmt..\n";
    #endif
    root = wrap_DFSPreorder(root, loop_matcher, "_tactic_");
  }
  else {
    #ifdef DEBUG
      std::cout << "init stmt..\n"; 
    #endif
    root = wrap_DFSPreorder(root, loop_matcher_init, "_tactic_");
  } 
  root = unsqueeze_tree(root.child(0));
  // mark the loop in the detected region. The region
  // is labeled with a mark node _tactic_
  root = mark_loop_and_stmt(root, "_tactic_");
  
  isl::schedule new_schedule = root.get_schedule();
  // avoid to update the schedule if not tactic has 
  // been detected!.
  if (!look_up_schedule_tree(new_schedule, "_tactic_"))
    return false;
  update_schedule(new_schedule, !recompute);  
  return true;
}

void Highlighter::take_snapshot(const QString &text) {

  Expects(current_schedule_);
  BlockSchedule *bs = new BlockSchedule();
  bs->schedule_block_ = current_schedule_;
  bs->transformation_string_ = text;
  setCurrentBlockUserData(bs);
  return;
}

void Highlighter::match_pattern(const QString &text, bool recompute) {

  // FIXME
  // the problem is that pressing carriege return
  // triggers again this function as consequence
  // the static variable for stmt annotation changes!
  if (!bad_workaround) {
    return; 
  }

  std::string pattern = text.toStdString();

  std::regex pattern_regex(R"(pattern\[(.*)\])");
  std::smatch matched_text;
  if (!std::regex_match(pattern, matched_text, pattern_regex))
    return;  
  pattern = matched_text[1].str();
  // FIXME
  bad_workaround = false;

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
    #ifdef DEBUG
      std::cout << "Success in parsing: " << pattern << "\n";
    #endif
    has_matched = match_pattern_helper(accesses_descriptors, scop, recompute);
  } catch (Error::Error e) {
    std::cout << "Error:" << e.message_ << "\n";
    return;
    }
    catch (...) {
    std::cout << "Error: pet cannot open the file or isl exception!\n";
    return;
    }
  if (has_matched)
    take_snapshot(text); 
}

void Highlighter::tile(const QString &text, bool recompute) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif 

  std::string tile_transformation = text.toStdString();
  
  std::regex tile_regex(R"(tile\[(.*)\])");
  std::smatch matched_text;
  if (!std::regex_match(tile_transformation, matched_text, tile_regex))
    return;
  tile_transformation = matched_text[1].str();

  //if (tile_transformation.empty())
  //  return;

  tile_transformation.erase(
    std::remove_if(
      tile_transformation.begin(),
      tile_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), tile_transformation.end());

  std::regex pattern_regex(R"(([a-z_]+),([0-9]+))");
  if (!std::regex_match(tile_transformation, matched_text, pattern_regex))
    return;

  #ifdef DEBUG
    std::cout << "================\n";
    std::cout << "Current schedule: " << "\n";
    std::cout << current_schedule_.to_str() << "\n";
    std::cout << "Prev schedule: " << "\n";
    std::cout << previous_schedule_.to_str() << "\n";
    std::cout << "================\n";
  #endif
  isl::schedule new_schedule = (!recompute) ?
    opt_.tile(current_schedule_, matched_text[1].str(), std::stoi(matched_text[2].str())):
    opt_.tile(previous_schedule_, matched_text[1].str(), std::stoi(matched_text[2].str()));

  
  // update to new schedule. If we are *not*
  // recomputing we set also the previous schedule.
  update_schedule(new_schedule, !recompute);
  take_snapshot(text);
}

void Highlighter::unroll(const QString &text, bool recompute) {

  std::string unroll_transformation = text.toStdString();

  std::regex unroll_regex(R"(unroll\[(.*)\])");
  std::smatch matched_text;
  if (!std::regex_match(unroll_transformation, matched_text, unroll_regex))
    return;
  unroll_transformation = matched_text[1].str();
  
  if (unroll_transformation.empty())
    return;

  unroll_transformation.erase(
    std::remove_if(
      unroll_transformation.begin(),
      unroll_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), unroll_transformation.end());

  std::regex pattern_regex(R"(([a-z_]+),([0-9]+))");
  if (!std::regex_match(unroll_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule =
    opt_.unroll_loop(current_schedule_, matched_text[1].str(), std::stoi(matched_text[2].str()));

  update_schedule(new_schedule, !recompute);
  take_snapshot(text);
}

void Highlighter::interchange(const QString &text, bool recompute) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
    std::cout << "Input: " << "\n";
    std::cout << " remcompute: " << recompute << "\n";
    std::cout << " text: " << text.toStdString() << "\n";
  #endif

  std::string interchange_transformation = text.toStdString();
  
  std::regex interchange_regex(R"(interchange\[(.*)\])");
  std::smatch matched_text;
  if (!std::regex_match(interchange_transformation, matched_text, interchange_regex))
    return;
  interchange_transformation = matched_text[1].str();

  if (interchange_transformation.empty())
    return;

  interchange_transformation.erase( 
    std::remove_if(
      interchange_transformation.begin(),
      interchange_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), interchange_transformation.end());

  std::regex pattern_regex(R"(([a-z_]+),([a-z_]+))");
  if (!std::regex_match(interchange_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule =
    opt_.swap_loop(current_schedule_, matched_text[1].str(), matched_text[2].str());
  update_schedule(new_schedule, !recompute);

  // take a snapshot of the current schedule.
  take_snapshot(text);
  calls_to_interchange++;
}

void Highlighter::fuse(const QString &text, bool recompute) {

  std::string fuse_transformation = text.toStdString();

  std::regex fuse_regex(R"(fuse\[(.*)\])");
  std::smatch matched_text;
  if (!std::regex_match(fuse_transformation, matched_text, fuse_regex))
    return;
  fuse_transformation = matched_text[1].str();

  if (fuse_transformation.empty())
    return;

  fuse_transformation.erase(
    std::remove_if(
      fuse_transformation.begin(),
      fuse_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), fuse_transformation.end());

  std::regex pattern_regex(R"((stmt[1-9]+),(stmt[1-9]+))");
  if (!std::regex_match(fuse_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule =
    opt_.fuse(current_schedule_, matched_text[1].str(), matched_text[2].str());
  update_schedule(new_schedule, !recompute);

  take_snapshot(text);
}

void Highlighter::loop_reverse(const QString &text, bool recompute) {

  std::string loop_reverse_transformation = text.toStdString();

  std::regex loop_reverse_regex(R"(loopReverse\[(.*)])");
  std::smatch matched_text;
  if (!std::regex_match(
    loop_reverse_transformation, matched_text, loop_reverse_regex))
    return;
  loop_reverse_transformation = matched_text[1].str();

  if (loop_reverse_transformation.empty())
    return;

  loop_reverse_transformation.erase(
    std::remove_if(
      loop_reverse_transformation.begin(),
      loop_reverse_transformation.end(),
      [](unsigned char x) { return std::isspace(x); }), loop_reverse_transformation.end());

  // FIXME: all this regex are nightmare for maintainability.
  std::regex pattern_regex(R"(([a-z_]+))");
  if (!std::regex_match(loop_reverse_transformation, matched_text, pattern_regex))
    return;

  isl::schedule new_schedule =
    opt_.loop_reverse(current_schedule_, matched_text[1].str());

  // do not update prev schedule if 
  // we are recomputing.
  update_schedule(new_schedule, !recompute);
  take_snapshot(text);
}

// The boolean means the following:
// 1 -> compare with the baseline
// 0 -> compare with the previous schedule
void Highlighter::compare(bool with_baseline) {

  Expects(with_baseline);

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

/// run cache model (haystack)
void Highlighter::runCacheModel() {

  if (current_schedule_.is_null())
    return;
  haystackRunner_.runModel(current_schedule_, file_path_);
} 

void Highlighter::do_transformation(const QString &text, bool recompute) {
  
  for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
    QRegularExpressionMatchIterator matchIterator = rule.pattern_.globalMatch(text);
      while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), rule.format_);

        switch(rule.id_rule_) {
          case 0: match_pattern(text, recompute); break;
          case 1: tile(text, recompute); break;
          case 2: interchange(text, recompute); break;
          case 3: unroll(text, recompute); break;
          case 4: compare(true); break;
          case 5: loop_reverse(text, recompute); break;
          case 6: fuse(text, recompute); break; 
          case 7: runCacheModel(); break;
          default: assert(0 && "rule not implemented!");
        }
      }
  }
}

void Highlighter::highlightBlock(const QString &text) {

  #ifdef DEBUG
    std::cout << "=====================================================\n";
    std::cout << "TEXT: " << text.toStdString() << "\n";
    std::cout << "calls_to_interchange: " << calls_to_interchange << "\n";
  #endif

  if (text.isEmpty() || prev_text_ == text)
    return;
 
  auto *current_block_data = currentBlockUserData();
  if (current_block_data) {
    BlockSchedule *payload = dynamic_cast<BlockSchedule*>(current_block_data);
    if (payload && (text == payload->transformation_string_)) {
      #ifdef DEBUG
      std::cout << __func__ << " : "
        << "Current block schedule -> " 
        << payload->schedule_block_.get_root().to_str() << "\n";
      #endif 
      update_schedule(payload->schedule_block_, false);
    }
    else {
      #ifdef DEBUG
        std::cout << "re-compute transformation with text: " 
          << text.toStdString() << "\n";
      #endif
      do_transformation(text, true);
    }
  }
    
  do_transformation(text, false);
  
  prev_text_ = text;  

  #ifdef DEBUG
    std::cout << "=====================================================\n";
  #endif
}

void Highlighter::updatePath(const QString &path) {
  #ifdef DEBUG  
    std::cout << __func__ << " : "
      << path.toStdString() << "\n";
  #endif
  file_path_ = path;
}

void Highlighter::updateTime(const TimingInfo &baseline_time, 
  const TimingInfo &opt_time) {

    Q_EMIT timeUserFeedbackChanged(baseline_time, opt_time);
}

void Highlighter::updateCacheStats(const CacheStats &stats) {

  Q_EMIT cacheUserFeedbackChanged(stats);
}

int Highlighter::get_next_stmt_id() {
  Highlighter::stmt_id_++;
  return Highlighter::stmt_id_;
}
