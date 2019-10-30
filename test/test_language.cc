#include "gtest/gtest.h"
#include "islutils/matchers.h"
#include "islutils/ctx.h"
#include "islutils/pet_wrapper.h"
#include "islutils/builders.h"
#include "islutils/gsl/gsl_assert"
#include "islutils/parser.h"
#include <islutils/access_patterns.h>

#include <boost/filesystem.hpp>
#include <stack>
#include <iostream>

using util::ScopedCtx;

/** \defgroup Matchers Language
 * \brief User-provided matchers language.
 *  
 *  The language describes a given loop nest. We distinguish between
 *  loop structural properties and loop access-pattern properties. 
 *  The former are optional while the latter are mandatory.
 *  At the highest level the matcher will look like:
 *  ```
 *  loop(_and(structural properties, access pattern properties))
 *  ```
 *  The following callbacks are provided to check structural properties:
 *    1. hasDimensionality(int)
 *  
 *  Access pattern properties must be nested in hasStmt(...)
 *  hasStmt will apply it's payload to every statement. If a statement
 *  matches will get promoted to "candidate". A candidate will get 
 *  promoted to "match" if and only if the entire matcher will match.
 *  
 *  hasStmt(...) accpets:
 *    1. anything() 
 *    2. hasAccess(...)
 *
 *  hasAccess(...) accepts:
 *    1. anything()
 *    2. high-level callback that work on the entire isl::union_map (i.e., isGemmLike())
 *    3. onWrite/onRead(oneOf/anyOf/allOf(...)) which allow to query properties of the single access
 *
 *    onWrite and onRead restrict the access to write or read respectively and are 
 *    optional.
 *  
 *    oneOf/anyOf/allOf have the same meaning as:
 *    https://www.boost.org/doc/libs/1_52_0/libs/algorithm/doc/html/algorithm/CXX11.html
 *  
 */

/** \ingroup Matchers */


namespace lang {

enum class AccessRestriction {ALL, READS, WRITES};
enum class GroupStrategy {ONE_MATCH_ONE_INSTANCE, MULTIPLE_MATCH_ONE_INSTANCE};

/// Global visibility. hasStmt will
/// push a new candidate if a statement matches.
class CandidateStmts {
  public:
    std::vector<std::string> matches_;
}candidateStmts;

/// 
class MatchedStmts {
  public:
    std::vector<std::vector<std::string>> matches_;
    std::vector<std::string> noMatches_;
};

/// global pointer. It is clutter and will be removed.
/// It is used to pass reads and writes information
/// to matcher callbacks.
std::unique_ptr<std::string> pointer;


#if defined(DEBUG) && defined(LEVEL_ONE)
void dump(const MatchedStmts &m) {

  std::cout << "Matched { ";
  for (const auto &sv : m.matches_) {
    std::cout << " { ";
    for (const auto &s : sv)
      std::cout << s << " ";
    std::cout << " } ";
  }
  std::cout << " } ";

  std::cout << "Not Matched { ";
  for (const auto &s : m.noMatches_)
    std::cout << s << " ";
  std::cout << " } \n";
}
#endif


/// Helper function: is the access *not* a constant?
static bool isArray(isl::map access) {

  isl::space space = access.get_space();
  if (space.dim(isl::dim::out) == 0)
    return false;
  return true;
}

/// Helper function to remove constant accesses from "accesses".
static isl::union_map getAccessesToArrayOnly(isl::union_map accesses) {

  if (accesses.is_empty())
    return accesses;

  std::vector<isl::map> accessesAsMap{};
  accesses.foreach_map([&accessesAsMap](isl::map m) {
    if (isArray(m))
      accessesAsMap.push_back(m);
    return isl_stat_ok;
  });

  isl::union_map res = isl::union_map(accessesAsMap[0]);
  for (auto const & access : accessesAsMap)
    res = res.unite(isl::union_map(access));
  return res;
}

/// wildcard for hasStmt. Accept every possible statement. 
std::function<bool(isl::schedule_node)> anything() {

  return [](isl::schedule_node node) {
    // hasStmt pass leaf to callbacks. 
    Expects(node.get_type() == isl_schedule_node_leaf);
    return true;
  };
}


/// wildcard for hasAccess. Accept every possible access.
std::function<bool(isl::schedule_node, AccessRestriction r)> __anything() {

  return [](isl::schedule_node node, AccessRestriction r) {
    Expects(node.get_type() == isl_schedule_node_leaf);
    return true;
  };
}

/// given a prefix schedule of a leaf node returns the statement 
/// name if any.
static std::string getStatementName(isl::union_map prefixSchedule) {

  Expects(prefixSchedule.n_map() == 1);

  std::string res{};
  isl::set domainPrefixSchedule =
    isl::map::from_union_map(prefixSchedule).domain();

  if (domainPrefixSchedule.has_tuple_name())
    res = domainPrefixSchedule.get_tuple_name();

  return res;
}

/// Implementation for hasStmt.
/// This function restricts the matcher on band node that have:
/// 1) only a leaf node as child. We assume each leaf node is a statement. 
///    We run the callback f on the leaf statement.
/// 2) a sequence node followed by filter(s). We only look at filter nodes with
///    a leaf as a child. We run the callback on all the statements.
/// In case of multiple matches we hasStmt returns true if at least one match happened.
static bool hasStmtImpl(isl::schedule_node node, std::function<bool(isl::schedule_node)> f) {

  if (node.get_type() != isl_schedule_node_band)
    return false;

  // we expect all the band nodes to be 
  // single dimension.
  Expects(node.band_n_member() == 1);

  if (!node.n_children())
    return false;

  isl::schedule_node maybeLeafOrSequence = node.first_child();
  if (maybeLeafOrSequence.get_type() != isl_schedule_node_leaf &&
      maybeLeafOrSequence.get_type() != isl_schedule_node_sequence)
    return false;

  // handle single statement.
  if (maybeLeafOrSequence.get_type() == isl_schedule_node_leaf) {
    isl::schedule_node leaf = maybeLeafOrSequence;
    bool hasMatched = f(leaf);
    std::string stmtName = getStatementName(leaf.get_prefix_schedule_union_map());
    Expects(!stmtName.empty());
    if (hasMatched)
      candidateStmts.matches_.push_back(stmtName);
    return hasMatched;
  }

  // handle multiple statements.
  isl::schedule_node sequence = maybeLeafOrSequence;
  size_t children = sequence.n_children();
  bool hasAtLeastOneMatch = false;
  for (size_t i = 0; i < children; i++) {
    isl::schedule_node maybeLeaf = sequence.child(i).child(0);
    if (maybeLeaf.get_type() != isl_schedule_node_leaf)
      continue;
    isl::schedule_node leaf = maybeLeaf;
    bool hasMatched = f(leaf);
    hasAtLeastOneMatch = hasAtLeastOneMatch or hasMatched;
    std::string stmtName = getStatementName(leaf.get_prefix_schedule_union_map());
    Expects(!stmtName.empty());
    if (hasMatched)
      candidateStmts.matches_.push_back(stmtName);
  }
  return hasAtLeastOneMatch;
}

/// hasStmt make sure that the band node is followed by a leaf.
/// The assumption is that only leaf node contains statements.
std::function<bool(isl::schedule_node)> hasStmt(std::function<bool(isl::schedule_node)> f) {

  return [f](isl::schedule_node node) {
    return hasStmtImpl(node, f);
  };
}

/// Implementation for hasDimensionality.
static bool hasDimensionalityImpl(isl::schedule_node node, size_t dim) {

  if (node.get_type() != isl_schedule_node_band)
    return false;

  Expects(node.band_n_member() == 1);

  if (!node.n_children())
    return false;

  isl::union_map prefixSchedule =
    node.child(0).get_prefix_schedule_union_map();

  std::vector<isl::map> prefixScheduleAsMap{};
  prefixSchedule.foreach_map([&prefixScheduleAsMap](isl::map m) {
    prefixScheduleAsMap.push_back(m);
    return isl_stat_ok;
  });

  size_t dimensionality = 0;
  for (const auto m : prefixScheduleAsMap) {
    if (m.dim(isl::dim::out) > dimensionality)
      dimensionality = m.dim(isl::dim::out);
  }
  return dimensionality == dim;
}

/// hasDimensionality checks the loop dimensionality.
std::function<bool(isl::schedule_node)> hasDimensionality(int dim) {

  return [dim](isl::schedule_node node) {
    return hasDimensionalityImpl(node, dim);
  };
}

static bool hasAccessImpl(isl::schedule_node node,
  std::function<bool(isl::schedule_node, AccessRestriction)> f, AccessRestriction r) {

  // as we are looking at the access pattern properties
  // we accept only leaf at this point.
  Expects(node.get_type() == isl_schedule_node_leaf);
  return f(node, r);
}

/// hasAccess allows composition of access patterns properties.
/// It accepts:
///  1. __anything() wildcard for access pattern.
///  2. High-level callbacks such as isGemmLike. High-level callbacks work on the entire isl::union_map.
///  3. Low-level callbacks (i.e., isTranspose()) which allow to check properties of the single access (isl::map).
/// hasAccess must be called in hasStmt.
std::function<bool(isl::schedule_node)> hasAccess(std::function<bool(isl::schedule_node, AccessRestriction r)> f) {

  return [f](isl::schedule_node node) {
    return hasAccessImpl(node, f, AccessRestriction::ALL);
  };
}

/// helper function to get reads and writes.
/// scop writes and reads. Note the use of "pointer" which is 
/// a global-visibilty pointer to store the file name.
std::pair<isl::union_map, isl::union_map> getReadsAndWrites(isl::ctx ctx) {

  // get reads and write for the leaf's schedule.
  // "pointer" is clutter (global pointer) how to remove?
  auto reads = pet::Scop::parseFile(ctx, *pointer).getScop().reads.curry();
  auto writes = pet::Scop::parseFile(ctx, *pointer).getScop().mustWrites.curry();

  return std::make_pair(reads, writes);
}

/// helper function to get reads and writes, and restrict them to node.
std::pair<isl::union_map, isl::union_map> getReadsAndWrites(isl::schedule_node node) {

  Expects(node.get_type() == isl_schedule_node_leaf);

  auto reads = getReadsAndWrites(node.get_ctx()).first;
  auto writes = getReadsAndWrites(node.get_ctx()).second;
  reads = reads.apply_domain(node.get_prefix_schedule_union_map());
  writes = writes.apply_domain(node.get_prefix_schedule_union_map());

  return std::make_pair(reads, writes);
}

/// Callback to be used to check property of a single access.
/// Is "map" an array access?
std::function<bool(isl::map)> isArray() {

  return [](isl::map map) {
    isl::space space = map.get_space();
    if (space.dim(isl::dim::out) == 0)
      return false;
    return true;
  };
}

/// Callback to be used to check property of a single access.
/// Is "map" bijective?
std::function<bool(isl::map)> isBijective() {

  return [](isl::map map) {
    return map.is_bijective();
  };
}

/// Callback to be used to check property of a single access.
/// Is "map" a transpose access?
// FIXME: make me generic. ATM only works for 2-d arrays.
std::function<bool(isl::map)> isTranspose() {

  return [](isl::map map) {
    if (!isArray(map))
      return false;
    isl::ctx ctx = map.get_space().get_ctx();
    using namespace matchers;
    auto _i = placeholder(ctx);
    auto _j = placeholder(ctx);
    auto _A = arrayPlaceholder();
    auto ps = allOf(access(_A, _i, _j));
    auto m = match(map, ps);
    if (m.size() != 1)
      return false;
    return m[0][_i].payload().inputDimPos_ == 1;
  };
}

/// Callback to be used to check property of a single access.
/// Is "map" a write access?
static bool isWriteImpl(isl::map map) {

  auto writes = getReadsAndWrites(map.get_space().get_ctx()).second;
  std::vector<isl::map> vectorWrites{};
  writes.foreach_map([&vectorWrites](isl::map m) {
    vectorWrites.push_back(m);
    return isl_stat_ok;
  });

  for (const auto &m : vectorWrites) {
    if(map.range().is_equal(m.range()))
      return true;
  }
  return false;
}

std::function<bool(isl::map)> isWrite() {

  return [](isl::map map) {
    return isWriteImpl(map);
  };
}

/// Callback to be used to check property of a single access.
/// Is "map" a read access?
static bool isReadImpl(isl::map map) {

  auto reads = getReadsAndWrites(map.get_space().get_ctx()).first;
  std::vector<isl::map> vectorReads{};
  reads.foreach_map([&vectorReads](isl::map m) {
    vectorReads.push_back(m);
    return isl_stat_ok;
  });

  for (const auto &m : vectorReads) {
    if(map.range().is_equal(m.range()))
      return true;
  }
  return false;
}

std::function<bool(isl::map)> isRead() {

  return [](isl::map map) {
    return isReadImpl(map);
  };
}

/// Helper function.
/// Returns reads and write that belong to node based on the restriction r.
static std::pair<std::vector<isl::map>, std::vector<isl::map>>
  getReadsAndWritesAsVectorOfMap(isl::schedule_node node, AccessRestriction r) {

  isl::union_map reads, writes;
  if (r == AccessRestriction::ALL || r == AccessRestriction::READS)
    reads = getReadsAndWrites(node).first;
  if (r == AccessRestriction::ALL || r == AccessRestriction::WRITES)
    writes = getReadsAndWrites(node).second;

  std::vector<isl::map> vectorReads{};
  std::vector<isl::map> vectorWrites{};

  reads.foreach_map([&vectorReads] (isl::map m) {
    vectorReads.push_back(m);
    return isl_stat_ok;
  });

  writes.foreach_map([&vectorWrites] (isl::map m) {
    vectorWrites.push_back(m);
    return isl_stat_ok;
  });

  return std::make_pair(vectorReads, vectorWrites);
}

/// __allOf impl.
static bool __allOfImpl(isl::schedule_node node, std::function<bool(isl::map)> f, AccessRestriction r) {

  auto vectorReads = getReadsAndWritesAsVectorOfMap(node, r).first;
  auto vectorWrites = getReadsAndWritesAsVectorOfMap(node, r).second;

  for (const auto &m : vectorWrites) {
    if (!f(m))
      return false;
  }

  for (const auto &m : vectorReads) {
    if (!f(m))
      return false;
  }
  return true;
}

/// __allOf. 
/// We follow: https://www.boost.org/doc/libs/1_52_0/libs/algorithm/doc/html/algorithm/CXX11.html
std::function<bool(isl::schedule_node, AccessRestriction r)> __allOf(std::function<bool(isl::map)> f) {

  return [f](isl::schedule_node node, AccessRestriction r) {
    return __allOfImpl(node, f, r);
  };
}

/// __oneOf impl.
static bool __oneOfImpl(isl::schedule_node node, std::function<bool(isl::map)> f, AccessRestriction r) {

  auto vectorReads = getReadsAndWritesAsVectorOfMap(node, r).first;
  auto vectorWrites = getReadsAndWritesAsVectorOfMap(node, r).second;

  int matches = 0;
  for (const auto &m: vectorWrites) {
    if (f(m))
      matches++;
  }

  for (const auto &m: vectorReads) {
    if (f(m))
      matches++;
  }

  return matches == 1;
}

/// __oneOf (one and only one)
std::function<bool(isl::schedule_node, AccessRestriction r)> __oneOf(std::function<bool(isl::map)> f) {

  return [f](isl::schedule_node node, AccessRestriction r) {
    return __oneOfImpl(node, f, r);
  };
}

static bool __multipleOfImpl(isl::schedule_node node, std::function<bool(isl::map)> f, AccessRestriction r) {

  auto vectorReads = getReadsAndWritesAsVectorOfMap(node, r).first;
  auto vectorWrites = getReadsAndWritesAsVectorOfMap(node, r).second;

  int matches = 0;
  for (const auto &m: vectorWrites) {
    if (f(m))
      matches++;
  }

  for (const auto &m: vectorReads) {
    if (f(m))
      matches++;
  }

  return matches > 1;
}

/// __multipleOf ( > 1)
std::function<bool(isl::schedule_node, AccessRestriction r)> __multipleOf(std::function<bool(isl::map)> f) {

  return [f](isl::schedule_node node, AccessRestriction r) {
    return __multipleOfImpl(node, f, r);
  };
}

static bool __anyOfImpl(isl::schedule_node node, std::function<bool(isl::map)> f, AccessRestriction r) {

  auto vectorReads = getReadsAndWritesAsVectorOfMap(node, r).first;
  auto vectorWrites = getReadsAndWritesAsVectorOfMap(node, r).second;

  for (const auto &m: vectorWrites) {
    if (f(m))
      return true;
  }

  for (const auto &m: vectorReads) {
    if (f(m))
      return true;
  }

  return false;
}

/// __anyOf 
std::function<bool(isl::schedule_node, AccessRestriction r)> __anyOf(std::function<bool(isl::map)> f) {

  return [f](isl::schedule_node node, AccessRestriction r) {
    return __anyOfImpl(node, f, r);
  };
}

/// And between callbacks (isl::map)
template <typename... Args>
std::function<bool(isl::map)> __and(Args... args) {

  std::vector<std::function<bool(isl::map)>> vec = {args...};

  return [vec](isl::map map) {
    std::function<bool(isl::map)> tmp = vec[0];
    bool result = tmp(map);
    for (size_t i = 1; i < vec.size(); i++) {
      tmp = vec[i];
      result = result and tmp(map);
    }
    return result;
  };
}

/// Or between callbacks (isl::map)
template <typename... Args>
std::function<bool(isl::map)> __or(Args... args) {

  std::vector<std::function<bool(isl::map)>> vec = {args...};

  return [vec](isl::map map) {
    std::function<bool(isl::map)> tmp = vec[0];
    bool result = tmp(map);
    for (size_t i = 1; i < vec.size(); i++) {
      tmp = vec[i];
      result = result or tmp(map);
    }
    return result;
  };
}

/// Not
std::function<bool(isl::map)> __not(std::function<bool(isl::map)> f) {

  return [f](isl::map map) {
    return not f(map);
  };
}

/// Restricts access look-up to writes.
std::function<bool(isl::schedule_node, AccessRestriction r)>
  onWrite(std::function<bool(isl::schedule_node, AccessRestriction r)> f) {

  return [f](isl::schedule_node node, AccessRestriction r) {
    // it is nested within hasAccess(...) so we don't
    // expect any restriction in the access patterns.
    // call now allOf/anyOf or oneOf with restriction.
    Expects(r == AccessRestriction::ALL);
    return f(node, AccessRestriction::WRITES);
  };
}

/// Restricts access look-up to reads.
std::function<bool(isl::schedule_node, AccessRestriction r)>
  onRead(std::function<bool(isl::schedule_node, AccessRestriction r)> f) {

  return [f](isl::schedule_node node, AccessRestriction r) {
    // it is nested within hasAccess(...) so we don't
    // expect any restriction in the access patterns.
    // call now allOf/anyOf or oneOf with restriction.
    Expects(r == AccessRestriction::ALL);
    return f(node, AccessRestriction::READS);
  };
}

static bool isGemmLikeImpl(isl::schedule_node node, AccessRestriction r) {

  Expects(node.get_type() == isl_schedule_node_leaf);
  Expects(r == AccessRestriction::ALL);

  // remove constant from reads/writes
  auto reads = getReadsAndWrites(node).first;
  auto writes = getReadsAndWrites(node).second;
  reads = getAccessesToArrayOnly(reads);
  writes = getAccessesToArrayOnly(writes);

  if (writes.n_map() != 1)
    return false;
  if (reads.n_map() != 3)
    return false;

  isl::ctx ctx = node.get_ctx();

  using namespace matchers;
  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto _ii = placeholder(ctx);
  auto _jj = placeholder(ctx);
  auto _A = arrayPlaceholder();
  auto _B = arrayPlaceholder();
  auto _C = arrayPlaceholder();
  
  auto psRead = allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
  auto psWrite = allOf(access(_A, _ii, _jj));
  
  auto readMatches = match(reads, psRead);
  auto writeMatches = match(writes, psWrite);
  
  if ((readMatches.size() != 1) || (writeMatches.size() != 1))
    return false;

  if (writeMatches[0][_ii].payload().inputDimPos_ !=
      readMatches[0][_i].payload().inputDimPos_)
    return false;

  if (writeMatches[0][_jj].payload().inputDimPos_ !=
      readMatches[0][_j].payload().inputDimPos_)
    return false; 

  return true;
}

/// isGemmLike access pattern.
std::function<bool(isl::schedule_node, AccessRestriction r)> isGemmLike() {

  return [](isl::schedule_node node, AccessRestriction r) {
    return isGemmLikeImpl(node, r);
  };
}

// FIXME
static bool isPointWiseImpl(isl::schedule_node node, AccessRestriction r) {

  Expects(node.get_type() == isl_schedule_node_leaf);
  Expects(r == AccessRestriction::ALL);

  auto reads = getReadsAndWrites(node).first;
  if (reads.n_map() != 0)
    return false;
  return true;
}

/// isPointWise access pattern.
std::function<bool(isl::schedule_node, AccessRestriction r)> isPointWise() {

  return [](isl::schedule_node node, AccessRestriction r) {
    return isPointWiseImpl(node, r);
  };
}

/// Helper function.
/// Check if "p" is a valid file path.
static bool checkIfValid(const std::string &p) {

  namespace filesys = boost::filesystem;
  try {
    filesys::path path(p);
    if (filesys::exists(p) && filesys::is_regular_file(p))
      return true;
  } catch(filesys::filesystem_error &e) {
    std::cerr << e.what() << std::endl;
  }
  return false;
}

///
static void removeFromMatched(const std::string &s, lang::MatchedStmts &matchedStmts) {

  for (size_t i = 0; i < matchedStmts.noMatches_.size(); i++)
    if (matchedStmts.noMatches_[i] == s)
      matchedStmts.noMatches_.erase(matchedStmts.noMatches_.begin() + i);
}

/// if a matcher match with the current node. We promote the
/// candidate statements to be matched statements.
static isl::schedule_node insertOnMatch(
  const matchers::ScheduleNodeMatcher &m, isl::schedule_node node,
  lang::MatchedStmts &matchedStmts,
  lang::GroupStrategy g) {

  // if the matcher matches, promote the candidates to matches
  // and remove the candidates to the no match array.
  std::vector<std::string> tmp{};
  if (matchers::ScheduleNodeMatcher::isMatching(m, node)) {

    switch (g) {
      case GroupStrategy::ONE_MATCH_ONE_INSTANCE:
      {
        for (size_t i = 0; i < candidateStmts.matches_.size(); i++) {
          tmp.push_back(candidateStmts.matches_[i]);
          removeFromMatched(candidateStmts.matches_[i], matchedStmts);
        }
        matchedStmts.matches_.push_back(std::move(tmp));
        break;
      }
      case GroupStrategy::MULTIPLE_MATCH_ONE_INSTANCE:
      {
        for (size_t i = 0; i < candidateStmts.matches_.size(); i++) {
          tmp.push_back(candidateStmts.matches_[i]);
          if (matchedStmts.matches_.empty())
            matchedStmts.matches_.push_back(std::move(tmp));
          else
            matchedStmts.matches_[0].push_back(candidateStmts.matches_[i]);
          removeFromMatched(candidateStmts.matches_[i], matchedStmts);
        }
        break;
      }
      default:
        Expects(false);
    }
    candidateStmts.matches_.clear();
  }
  return node;
}

/// simply work the entire schedule tree.
static isl::schedule_node match(
  const matchers::ScheduleNodeMatcher &m, isl::schedule_node node,
  MatchedStmts &matchedStmts,
  GroupStrategy g) {

  node = insertOnMatch(m, node, matchedStmts, g);

  for (int i = 0; i < node.n_children(); ++i) {
    node = match(m, node.child(i), matchedStmts, g).parent();
  }
  return node;
}

/// Collect all the statements within the scop.
/// All the collected stmts are stored as "not matched"
/// in "matchedStmts".
static void collectAllStmts(isl::schedule_node root, lang::MatchedStmts &matchedStmts) {

  isl_schedule_node_foreach_descendant_top_down(
    root.get(),
    [](__isl_keep isl_schedule_node *nodePtr, void *user) -> isl_bool {

      lang::MatchedStmts *p = static_cast<lang::MatchedStmts *>(user);
      isl::schedule_node node = isl::manage_copy(nodePtr);
      if (node.get_type() == isl_schedule_node_leaf) {
        auto name = getStatementName(node.get_prefix_schedule_union_map());
        p->noMatches_.push_back(name);
      }
      return isl_bool_true;
    }, &matchedStmts);
}


static MatchedStmts groupStatements(
  const matchers::ScheduleNodeMatcher &m, const std::string &p,
  GroupStrategy g = GroupStrategy::ONE_MATCH_ONE_INSTANCE) {

  using namespace lang;

  MatchedStmts matchedStmts{};

  if (!checkIfValid(p))
    return matchedStmts;

  // get the scop.
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, p);
  auto scop = petScop.getScop();

  isl::schedule_node root = scop.schedule.get_root();
  Expects(root);

  collectAllStmts(root, matchedStmts);
  root = match(m, root, matchedStmts, g);

  return matchedStmts;
}
} // end namespace lang

TEST(language, testTwentyTwo) {

  using namespace lang;

  auto m = []() {
    using namespace matchers;
    return loop(hasStmt(anything()));
  }();

  auto res = groupStatements(m, "inputs/gemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 2);
  EXPECT_TRUE(res.noMatches_.size() == 0);

  auto m0 = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(__anything())));
  }();
  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/gemm.c"));

  res = groupStatements(m0, "inputs/gemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 2);
  EXPECT_TRUE(res.noMatches_.size() == 0);

  res = groupStatements(m, "inputs/3mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 6);
  EXPECT_TRUE(res.noMatches_.size() == 0);

  auto m1 = []() {
    using namespace matchers;
    return loop(_and(hasDimensionality(3), hasStmt(anything())));
  }();

  res = groupStatements(m1, "inputs/gemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);
  EXPECT_TRUE(res.noMatches_.size() == 1);

  auto m2 = []() {
    using namespace matchers;
    return loop(_and(hasDimensionality(2), hasStmt(anything())));
  }();

  res = groupStatements(m2, "inputs/gemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);
  EXPECT_TRUE(res.noMatches_.size() == 1);

  res = groupStatements(m2, "inputs/3mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 3);
  EXPECT_TRUE(res.noMatches_.size() == 3);
}

TEST(language, testTwentyFour) {

  using namespace lang;

  auto m = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(__allOf(isArray()))));
  }();

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/doubleStmtGemm.c"));

  auto res = groupStatements(m, "inputs/doubleStmtGemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)  
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);
  EXPECT_TRUE(res.noMatches_.size() == 5);
}

TEST(language, testTwentyFive) {

  using namespace lang;

  auto m = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(__allOf(isWrite()))));
  }();

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/doubleStmtGemm.c"));

  auto res = groupStatements(m, "inputs/doubleStmtGemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)  
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);
  EXPECT_TRUE(res.noMatches_.size() == 5);
}

TEST(language, testTwentySix) {

  using namespace lang;

  auto m = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(__allOf(__and(isWrite(), isArray())))));
  }();

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/doubleStmtGemm.c"));

  auto res = groupStatements(m, "inputs/doubleStmtGemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)  
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);
  EXPECT_TRUE(res.noMatches_.size() == 5);
}

TEST(language, testTwentySeven) {

  using namespace lang;

  auto m = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(onWrite(__allOf(__and(isWrite(), isArray()))))));
  }();

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/doubleStmtGemm.c"));

  auto res = groupStatements(m, "inputs/doubleStmtGemm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)  
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 4);
  EXPECT_TRUE(res.matches_[0].size() == 1);
  EXPECT_TRUE(res.matches_[1].size() == 2);
  EXPECT_TRUE(res.noMatches_.size() == 0);
}

TEST(language, testTwentyEigth) {

  using namespace lang;

  auto m = []() {
    using namespace matchers;
    return loop(_and(hasDimensionality(2),
                     hasStmt(hasAccess(__anyOf(isArray())))));
  }();
  auto m1 = []() {
    using namespace matchers;
    return loop(_and(hasDimensionality(2),
                     hasStmt(hasAccess(__allOf(isArray())))));
  }();
  auto m2 = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(onWrite(__oneOf(isBijective())))));
  }();
  auto m3 = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(onWrite(__oneOf(__not(isBijective()))))));
  }();
  auto m4 = []() {
    using namespace matchers;
    return loop(_and(hasDimensionality(1),
                     hasStmt(hasAccess(__anyOf(isArray())))));
  }();
  auto m5 = []() {
    using namespace matchers;
    return loop(_and(hasDimensionality(2),
                     hasStmt(hasAccess(onRead(__oneOf(isTranspose()))))));
  }();

  auto m6 = []() {
    using namespace matchers;
    return loop(_and(hasDimensionality(2),
                     hasStmt(hasAccess(__anyOf(__or(__and(isTranspose(), isRead()),
                                                    __and(isWrite(), __not(isBijective()))))))));
  }();

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/gemver.c"));

  auto res = groupStatements(m, "inputs/gemver.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 3);
  EXPECT_TRUE(res.noMatches_.size() == 1);

  res = groupStatements(m1, "inputs/gemver.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);
  EXPECT_TRUE(res.noMatches_.size() == 3);

  res = groupStatements(m2, "inputs/gemver.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 2);
  EXPECT_TRUE(res.noMatches_.size() == 2);

  res = groupStatements(m3, "inputs/gemver.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 2);
  EXPECT_TRUE(res.noMatches_.size() == 2);

  res = groupStatements(m4, "inputs/gemver.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);
  EXPECT_TRUE(res.noMatches_.size() == 3);

  res = groupStatements(m5, "inputs/gemver.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
  EXPECT_TRUE(res.matches_.size() == 1);

  res = groupStatements(m6, "inputs/gemver.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
}

TEST(language, testTwentyNine) {

  using namespace lang;

  auto m = []() {
    using namespace matchers;
    return loop(hasStmt(anything()));
  }();

  auto m1 = []() {
    using namespace matchers;
    return loop(_and(hasStmt(hasAccess(isPointWise())),
                     hasDescendant(loop(hasStmt(hasAccess(isGemmLike()))))));
  }();

  auto m2 = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(isGemmLike())));
  }();

  auto m3 = []() {
    using namespace matchers;
    return loop(hasStmt(hasAccess(isPointWise())));
  }();

  auto m4 = []() {
    using namespace matchers;
    return loop(_and(hasStmt(hasAccess(isPointWise())),
                     hasDescendant(loop(hasStmt(hasAccess(isPointWise()))))));
  }();

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/2mm.c"));

  auto res = groupStatements(m, "inputs/2mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif

  res = groupStatements(m1, "inputs/2mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif

  res = groupStatements(m2, "inputs/2mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif

  res = groupStatements(m3, "inputs/2mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif

  res = groupStatements(m4, "inputs/2mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif

  res = groupStatements(m3, "inputs/2mm.c", GroupStrategy::MULTIPLE_MATCH_ONE_INSTANCE);
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif

  res = groupStatements(m2, "inputs/2mm.c", GroupStrategy::MULTIPLE_MATCH_ONE_INSTANCE);
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(res);
  #endif
}

