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

namespace lang {

// OLD API version.
/** \defgroup Matchers Language
 * \brief User-provided matchers language.
 *  
 *  The user can use the language to describe a given loop nest. For example,
 *  ```
 *  auto matcher =
 *    loop(_and(hasDepth(3),
 *              hasDescendant(loop(hasPattern(C(i,j) += A(i,k) * B(k,j)))));
 *  ```
 *  matches a loop with depth three that has a descendant loop with a GEMM-like pattern.
 *  At the moment we provide the following functions:
 *  1. hasDepth(int depth) check if the loop has depth @depth. [s]
 *  2. hasDepthLessThan(int depth) check if the loop has depth less than @depth. [s]
 *  3. hasDepthLessThanOrEqualTo(int depth) check if the loop has depth less than or equal to @depth. [s]
 *  4. hasDepthMoreThan(int depth) check if the loop has depth more than @depth. [s]
 *  5. hasDepthMoreThanOrEqualTo(int depth) check if the loop has depth more than or equal to @depth. [s]
 *  6. hasPattern(std::string p) check if the loop has the pattern @p among its statements. [a]
 *  7. hasOnlyPattern(std::string p) check if the loop has only one statement with pattern @p. [a]
 *  8. hasDescendant(loop l) check if the loop has loop @l as descendant. [m]
 *  9. hasNumberOfStmtEqualTo(int n) check if the loop has @n stmts. [s]
 *  [s] structural property.
 *  [a] access property.
 *  [m] miscellaneous property.
 *
 *  The functions can be composed via _and and _or
 */

/** \ingroup Matchers */

// NEW API version.


using Placeholder = 
  matchers::Placeholder<matchers::SingleInputDim, 
    matchers::UnfixedOutDimPattern<matchers::SimpleAff>>;
using Access = 
  matchers::ArrayPlaceholderList<matchers::SingleInputDim, 
    matchers::FixedOutDimPattern<matchers::SimpleAff>>;

/// helper containers.
struct PlaceholderSet {
  Placeholder p_;
  std::string id_;
};
struct ArrayPlaceholderSet {
  matchers::ArrayPlaceholder p_;
  std::string id_;
};
struct MatchResult {
  std::vector<std::pair<std::string, int>> boundedInduction_;
};


class CandidateStmts {
  public:
    std::vector<std::string> matches_;
}candidateStmts;

class MatchedStmts {
  public:
    std::vector<std::string> matches_;
    std::vector<std::string> noMatches_;
};

enum class ScheduleDim { allDim, anyDim, oneDim }; 

/// global pointer. It is clutter and will be removed.
/// It is used to pass reads and writes information
/// to matcher callbacks such as "hasOnlyPattern"
std::unique_ptr<std::string> pointer;

#if defined(DEBUG) && defined(LEVEL_ONE)
void dump(const std::vector<Parser::AccessDescriptor> &ads) {

  using namespace Parser;
  for (const auto &ad : ads) {
    std::cout << "Array name: " << ad.array_name_ << std::endl;
    if (ad.type_ == Type::READ)
      std::cout << "Read access" << std::endl;
    else std::cout << "Write access" << std::endl;
    for (const auto &af : ad.affine_accesses_) {
      std::cout << "Induction name: " << af.induction_var_name_ << std::endl;
      std::cout << "Increment: " << af.increment_ << std::endl;
      std::cout << "Coefficient: " << af.coefficient_ << std::endl;
    }
  }
}

void dump(const std::set<std::string> &elems) {

  for (const auto &elem : elems) {
    std::cout << "Element: " << elem << std::endl;
  }
}

void dump(const MatchResult &res) {

  for (const auto &r: res.boundedInduction_) {
    std::cout << " { ";
    std::cout << "induction : " << std::get<0>(r) << std::endl;
    std::cout << "schedule dim assigned : " << std::get<1>(r) << std::endl;
    std::cout << " } \n";
  }
}

void dump(const MatchedStmts &m) {
  
  std::cout << "Matched { ";
  for (const auto &s : m.matches_)
    std::cout << s << " ";
  std::cout << " } ";

  std::cout << "Not Matched { ";
  for (const auto &s : m.noMatches_)
    std::cout << s << " ";
  std::cout << " } \n";
}

#endif

/// Count how many times the matcher @m
/// matches a given node in the subtree 
/// rooted at @node.
static int countMatches(isl::schedule_node node,
  const matchers::ScheduleNodeMatcher &m) {

  int count = 0;
  std::stack<isl::schedule_node> nodeStack;
  nodeStack.push(node);

  while (nodeStack.empty() == false) {
    node = nodeStack.top();
    nodeStack.pop();

    if (matchers::ScheduleNodeMatcher::isMatching(m, node))
      count++;

    for (int i = 0; i < node.n_children(); ++i)
      nodeStack.push(node.child(i));
  }
  return count;
}

/// Count the depth of @node. In order to count
/// the depth we assume single-dimension band. 
/// To count how many band nodes are below @node
/// we use a simple match that matches all the 
/// single-dimension band.
static int countDepth(isl::schedule_node node) {

  // we can actually count the depth even if we
  // have multiple dims. Left for future works.
  auto hasSingleMember = [&](isl::schedule_node band) {
    if (band.band_n_member() != 1)
      assert(0 && "cannot count depth if the band has multiple dim");
    return true;
  };

  auto matcher = [&]() {
    using namespace matchers;
    return band(hasSingleMember, anyTree());
  }();

  return countMatches(node, matcher);
}

/// Implementation of hasDepth. 
static bool hasDepthImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)
    return false;
  // Expect band node to be single dimensional.
  Expects(node.band_n_member() == 1);
  return countDepth(node) == depth;
}

/// Implementation of hasDepthLessThan.
static bool hasDepthLessThanImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)
    return false;
  Expects(node.band_n_member() == 1);
  return countDepth(node) < depth;
}

/// Implementation of hasDepthLessThanOrEqual.
static bool hasDepthLessThanOrEqualToImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)  
    return false;
  Expects(node.band_n_member() == 1);
  return countDepth(node) < depth || countDepth(node) == depth;
}

/// Implementation of hasDepthMoreThan.
static bool hasDepthMoreThanImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)  
    return false;
  Expects(node.band_n_member() == 1);
  return countDepth(node) > depth;
}

/// Implementation of hasDepthMoreThanOrEqualTo.
static bool hasDepthMoreThanOrEqualToImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)  
    return false;
  Expects(node.band_n_member() == 1);
  return countDepth(node) > depth || countDepth(node) == depth;
}

/// Extract induction variables from the arrays obtained from the parser.
/// i.e., C(i, j) += A(i, k) * B(k, j) will return i, j and k
static std::set<std::string> extractInductions(
  const std::vector<Parser::AccessDescriptor> &accesses) {

  Expects(!accesses.size() == 0);
  
  std::set<std::string> results{};
  for (const auto &access : accesses) {
    for (const auto &affineExpr : access.affine_accesses_) {
      results.insert(affineExpr.induction_var_name_);
    }
  }

  #if defined(DEBUG) && defined(LEVEL_ONE)
    //dump(results);
  #endif

  return results;
}

/// Extract array name from the arrays obtained from the parser.
/// i.e., C(i, j) += A(i, k) * B(k, j) will return A, B and C
static std::set<std::string> extractArrayNames(
  const std::vector<Parser::AccessDescriptor> &accesses) {

  Expects(!accesses.size() == 0);

  std::set<std::string> results{};  
  for (const auto &access : accesses) {
    results.insert(access.array_name_);
  }

  #if defined(DEBUG) && defined(LEVEL_ONE)
    //dump(results);
  #endif

  return results;
}

/// Helper function to build the access matcher.
static std::vector<Access> buildAccessMatchers(const std::vector<PlaceholderSet> &ps,
  const std::vector<ArrayPlaceholderSet> &aps, const std::vector<Parser::AccessDescriptor> &ds) {

  Expects(!ds.size() == 0);

  using namespace matchers;
  auto findIndexInArrayPlaceholderSet = [&aps](const std::string id) {
    for (size_t i = 0; i < aps.size(); i++)
      if (aps[i].id_.compare(id) == 0)
        return i;
    Expects(0);
  };

  auto findIndexInPlaceholderSet = [&ps](const std::string id) {
    for (size_t i = 0; i < ps.size(); i++)
      if (ps[i].id_.compare(id) == 0)  
        return i;
    Expects(0);
  };

  std::vector<Access> accessList;

  for (size_t i = 0; i < ds.size(); i++) {
    size_t dims = ds[i].affine_accesses_.size();

    // FIXME: this is not ideal as for each possible array
    // dimension we need to have a case.
    switch (dims) {
      case 1: {
        size_t indexInArrayPlaceholder =
          findIndexInArrayPlaceholderSet(ds[i].array_name_);
        size_t indexInPlaceholder =
          findIndexInPlaceholderSet(ds[i].affine_accesses_[0].induction_var_name_);
          accessList.push_back(access(
            aps[indexInArrayPlaceholder].p_, 
            ds[i].affine_accesses_[0].coefficient_ 
            * ps[indexInPlaceholder].p_ + ds[i].affine_accesses_[0].increment_));
        break;
      }
      case 2: {
        size_t indexInArrayPlaceholder =
          findIndexInArrayPlaceholderSet(ds[i].array_name_);
        size_t indexInPlaceholderDimZero =
          findIndexInPlaceholderSet(ds[i].affine_accesses_[0].induction_var_name_);
        size_t indexInPlaceholderDimOne =
          findIndexInPlaceholderSet(ds[i].affine_accesses_[1].induction_var_name_);
        accessList.push_back(access(
          aps[indexInArrayPlaceholder].p_,
          ds[i].affine_accesses_[0].coefficient_
          * ps[indexInPlaceholderDimZero].p_ + ds[i].affine_accesses_[0].increment_,
          ds[i].affine_accesses_[1].coefficient_
          * ps[indexInPlaceholderDimOne].p_ + ds[i].affine_accesses_[1].increment_));
        break;
    }
    default :
      // we handle only 1d and 2d at the moment.
      Expects(0);
    }
  }
  return std::move(accessList);
}

/// Helper function to split read access descriptors  
/// from write access descriptors.
static std::vector<Parser::AccessDescriptor> getReadAccessDescriptors(
  const std::vector<Parser::AccessDescriptor> &ds) {

  using namespace Parser;
  std::vector<AccessDescriptor> res{};  
  for (const auto &d : ds) {
    Expects(d.type_ == Type::READ || d.type_ == Type::WRITE ||
            d.type_ == Type::READ_AND_WRITE);
    if (d.type_ == Type::READ || d.type_ == Type::READ_AND_WRITE)
      res.push_back(d);
  }
  return std::move(res);
}

/// Helper function to split read access descriptors  
/// from write access descriptors.
static std::vector<Parser::AccessDescriptor> getWriteAccessDescriptors(
  const std::vector<Parser::AccessDescriptor> &ds) {

  using namespace Parser;
  std::vector<AccessDescriptor> res{};  
  for (const auto &d : ds) {
    Expects(d.type_ == Type::READ || d.type_ == Type::WRITE ||
            d.type_ == Type::READ_AND_WRITE);
    if (d.type_ == Type::WRITE || d.type_ == Type::READ_AND_WRITE)
      res.push_back(d);
  }
  return std::move(res);
}

/// Helper function: is the access a constant?
static bool isArray(isl::map access) {

  isl::space space = access.get_space();
  if (space.dim(isl::dim::out) == 0)
    return false;
  return true;
}

/// Helper function to remove constant from accesses.
static isl::union_map getAccessesToArrayOnly(isl::union_map accesses) {

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

/// Helper function to make sure that one induction variable has 
/// one and only one schedule dimension assigned.
bool checkAssignmentInduction(const MatchResult &matchesReads,
  const MatchResult &matchesWrites) {

  Expects(!matchesReads.boundedInduction_.empty() 
          && !matchesWrites.boundedInduction_.empty());

  auto inductionReads = matchesReads.boundedInduction_;
  auto inductionWrites = matchesWrites.boundedInduction_;

  for (auto const &inductionRead : inductionReads) 
    for (auto const &inductionWrite : inductionWrites)
      if (std::get<0>(inductionWrite) == std::get<0>(inductionRead)) 
        if (std::get<1>(inductionWrite) != std::get<1>(inductionRead))
          return false;
  return true;
}

/// Helper function for matching the access patterns.
static MatchResult match(isl::ctx ctx, isl::union_map accesses, 
  const std::vector<Parser::AccessDescriptor> &ds) {

  Expects(!ds.size() == 0);

  // In the matcher we do not support zero-dimensional array. Hence,
  // we remove them. 
  isl::union_map accessesToArray = getAccessesToArrayOnly(accesses);

  auto res = MatchResult{};

  using namespace matchers;

  std::vector<PlaceholderSet> vectorPlaceholderSet{}; 
  std::vector<ArrayPlaceholderSet> vectorArrayPlaceholderSet{};
  std::vector<Access> accessSet{};
  std::set<std::string> inductionSet = extractInductions(ds);
  std::set<std::string> arrayNameSet = extractArrayNames(ds);

  for (auto const &arrayName : arrayNameSet) {
    ArrayPlaceholderSet tmp = {arrayPlaceholder(), arrayName};
    vectorArrayPlaceholderSet.push_back(std::move(tmp));
  }
  for (auto const &induction : inductionSet) {
    PlaceholderSet tmp = {placeholder(ctx), induction};
    vectorPlaceholderSet.push_back(std::move(tmp));
  }

  accessSet = buildAccessMatchers(vectorPlaceholderSet,
    vectorArrayPlaceholderSet, ds); 

  size_t nMaps = accessesToArray.n_map();
  #if defined(DEBUG) && defined(LEVEL_ONE)
      //std::cout << "# accessesToArray: " << nMaps << "\n";
      //std::cout << "Accesses to array: " << accessesToArray.to_str() << "\n";
      //std::cout << "# of build matchers: " << accessSet.size() << "\n";
      //dump(ds);
  #endif  
  if (accessSet.size() != nMaps)
    return res;

  auto matches = match(accessesToArray, allOf(accessSet));
  #if defined(DEBUG) && defined(LEVEL_ONE)
    //std::cout << "# of matches: " << matches.size() << "\n";
  #endif

  // as the match happens per stmt we expect 0 or 1 match
  // only.
  Expects(matches.size() <= 1);

  for (size_t i = 0; i < matches.size(); i++) {
    for (auto const &ps : vectorPlaceholderSet) {
      if (!matches[i][ps.p_].candidateSpaces().empty())
        res.boundedInduction_.push_back(
          std::make_pair(ps.id_, matches[i][ps.p_].payload().inputDimPos_));
    }
  }
  
  return res;
}

/// Helper function for implementation of hasOnlyPattern.
static bool hasOnlyPatternImplHelper(isl::ctx ctx, 
  isl::union_map reads, isl::union_map writes, const std::vector<Parser::AccessDescriptor> &ds) {

  Expects(!reads.is_empty());
  Expects(!writes.is_empty());
  Expects(!ds.size() == 0);

  auto resultMatchingReads = match(ctx, reads, getReadAccessDescriptors(ds));
  #if defined(DEBUG) && defined(LEVEL_ONE)
    //std::cout << "result matching for reads: " << std::endl;
    //dump(resultMatchingReads);
  #endif
  auto resultMatchingWrites = match(ctx, writes, getWriteAccessDescriptors(ds));
  #if defined(DEBUG) && defined(LEVEL_ONE)
    //std::cout << "result matching for writes: " << std::endl;
    //dump(resultMatchingWrites);
  #endif

  bool res = !resultMatchingReads.boundedInduction_.empty() 
             && !resultMatchingWrites.boundedInduction_.empty();
  if (!res)
    return false;

  // Placeholder are _not_ reused between different calls to allOf. 
  // We can overcome this inspecting the placeholder for the write and the read.
  // They should be equal.
  res = res && checkAssignmentInduction(
    resultMatchingReads, resultMatchingWrites);
  return res;
}

/// Helper function to get the prefix schedule as std::vector<isl::map>
static std::vector<isl::map> getPrefixSchedule(isl::schedule_node node) {

  Expects(node.get_type() == isl_schedule_node_band);

  // A band node always have a child (may be a leaf), and the prefix
  // schedule of the child includes the partial schedule of the node.
  auto prefixSchedule = node.child(0).get_prefix_schedule_union_map();
  std::vector<isl::map> schedulePrefixAsMap{};
  prefixSchedule.foreach_map([&schedulePrefixAsMap] (isl::map m) {
    schedulePrefixAsMap.push_back(m);
    return isl_stat_ok;
  });
  
  Ensures(schedulePrefixAsMap.size() != 0);
  return schedulePrefixAsMap;
}

/// Implementation of hasOnlyPattern.
static bool hasOnlyPatternImpl(isl::schedule_node node, const std::string &s, 
  bool allowMultiple = false) {

  if (node.get_type() != isl_schedule_node_band)
    return false;

  std::vector<Parser::AccessDescriptor> accessDescrs{};
  try {
    accessDescrs = Parser::parse(s);
  } catch(...) {
    std::cerr << "Error while parsing: " << s << std::endl;
    std::cout << "parser error!\n";
    return false;
  }

  #if defined(DEBUG) && defined(LEVEL_ONE)
    //dump(accessDescrs);
  #endif

  if (accessDescrs.size() == 0)
    return false;

  auto schedulePrefixAsMap = getPrefixSchedule(node);
  if (!allowMultiple && schedulePrefixAsMap.size() > 1) 
    return false;

  // this is clutter. 
  auto petScop = pet::Scop::parseFile(node.get_ctx(), *pointer);
  // end clutter
  auto reads = petScop.reads();
  auto writes = petScop.writes();

  bool res = false;
  isl::union_map scheduledReads;
  isl::union_map scheduledWrites;
  for (const auto &scheduleMap : schedulePrefixAsMap) {
    scheduledReads = reads.apply_domain(scheduleMap);
    scheduledWrites = writes.apply_domain(scheduleMap);

    if (scheduledReads.is_empty() || scheduledWrites.is_empty())
      return false;

    res = res || hasOnlyPatternImplHelper(node.get_ctx(), 
      scheduledReads, scheduledWrites, accessDescrs);
  }
  return res;
}

/// Helper function for hasNumberOfStmtEqualTo.
static bool hasNumberOfStmtEqualToImpl(isl::schedule_node node, int n) {

  if (node.get_type() != isl_schedule_node_band)
    return false;

  int stmts = node.child(0).get_prefix_schedule_union_map().n_map();
  return stmts == n;
}

/// check if the loop has depth @depth.
std::function<bool(isl::schedule_node)> hasDepth(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthImpl(node, depth);
  };
}

/// wildcard for hasStmt. Accept every possible statement. 
std::function<bool(isl::schedule_node)> anything() {

  return [](isl::schedule_node node) {
    // hasStmt pass leaf to callbacks. 
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
///    We run the callback f on the leaf.
/// 2) a sequence node followed by filter(s). We only look at filter nodes with
///    a leaf as a child. We run the callback on the leaf. 
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

  // single statement.
  if (maybeLeafOrSequence.get_type() == isl_schedule_node_leaf) {
    isl::schedule_node leaf = maybeLeafOrSequence;
    bool hasMatched = f(leaf);
    std::string stmtName = getStatementName(leaf.get_prefix_schedule_union_map());
    Expects(!stmtName.empty());
    if (hasMatched) 
      candidateStmts.matches_.push_back(stmtName); 
    return /*hasMatched;*/ true;
  }

  // multiple statements.
  isl::schedule_node sequence = maybeLeafOrSequence;
  size_t children = sequence.n_children();
  for (size_t i = 0; i < children; i++) {
    isl::schedule_node maybeLeaf = sequence.child(i).child(0);
    if (maybeLeaf.get_type() != isl_schedule_node_leaf)
      continue;
    isl::schedule_node leaf = maybeLeaf;
    bool hasMatched = f(leaf);
    std::string stmtName = getStatementName(leaf.get_prefix_schedule_union_map());
    Expects(!stmtName.empty());
    if (hasMatched)
      candidateStmts.matches_.push_back(stmtName);
  }
  return /*hasAtLeastOneMatch;*/ true;
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

static bool hasAccessImpl(isl::schedule_node node, std::function<bool(isl::schedule_node)> f) {

  // as we are looking at the access pattern properties
  // we accept only leaf node.
  Expects(node.get_type() == isl_schedule_node_leaf);
  // clutter
  auto scop = pet::Scop::parseFile(node.get_ctx(), *pointer).getScop();
  auto reads = scop.reads.curry();
  auto writes = scop.mustWrites.curry();
  std::cout << node.get_prefix_schedule_union_map().to_str() << "\n";
  std::cout << reads.apply_domain(node.get_prefix_schedule_union_map()).to_str() << "\n";
  std::cout << writes.apply_domain(node.get_prefix_schedule_union_map()).to_str() << "\n"; 
  return f(node);
}

/// hasAccess allows composition of access patterns properties.
/// The composition is obtained by combaining callbacks via _and, _or.
/// hasAccess can inspect also a single access pattern via allOf, anyOf and oneOf. 
/// hasAccess must be called in hasStmt.
std::function<bool(isl::schedule_node)> hasAccess(std::function<bool(isl::schedule_node)> f) {

  return [&f](isl::schedule_node node) {
    return hasAccessImpl(node, f);
  };
}
    
/// check if the loop has depth less than @depth.
std::function<bool(isl::schedule_node)> hasDepthLessThan(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthLessThanImpl(node, depth);
  };
}

/// check if the loop has depth less than or equal to @depth.
std::function<bool(isl::schedule_node)> hasDepthLessThanOrEqualTo(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthLessThanOrEqualToImpl(node, depth);
  };
}

/// check if the loop has depth more than @depth.
std::function<bool(isl::schedule_node)> hasDepthMoreThan(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthMoreThanImpl(node, depth);
  };
}

/// check if the loop has depth more or equal than @depth.
std::function<bool(isl::schedule_node)> hasDepthMoreThanOrEqualTo(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthMoreThanOrEqualToImpl(node, depth);
  };
}

/// check if the loop has access pattern @s.
std::function<bool(isl::schedule_node)> hasOnlyPattern(const std::string &s) {

  return [s](isl::schedule_node node) {
    return hasOnlyPatternImpl(node, s);
  };
}

/// check if the loop has multiple access patterns @s.
std::function<bool(isl::schedule_node)> hasPattern(const std::string &s) {

  return [s](isl::schedule_node node) {
    return hasOnlyPatternImpl(node, s, true);
  };
}

/// Check if the loop has @n stmts.
std::function<bool(isl::schedule_node)> hasNumberOfStmtEqualTo(int n) {

  return [n](isl::schedule_node node) {
    return hasNumberOfStmtEqualToImpl(node, n);
  };
}

/// Check if @p is a valid file path.
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

/// Utility function.
static isl::schedule_node wrapOnMatch(
  isl::schedule_node node, const matchers::ScheduleNodeMatcher &m) {

  if (matchers::ScheduleNodeMatcher::isMatching(m, node)) {
    node = node.insert_mark(isl::id::alloc(node.get_ctx(), m.getLabel(), nullptr));
  }
  return node;
}

/// Walk the schedule tree starting from @node and in
/// case of a match with the matcher @m wrap the subtree
/// with a mark node (aka label) provided by the matcher.
static isl::schedule_node match(
  const matchers::ScheduleNodeMatcher &m, isl::schedule_node node) {

  node = wrapOnMatch(node, m);

  // check if the node is already annotated.
  // FIXME: this is a workaround to avoid entering
  // an infinite loop.
  if (node.get_type() == isl_schedule_node_mark) {
    return node;
  }

  for (int i = 0; i < node.n_children(); ++i) {
    node = match(m, node.child(i)).parent();
  }
  return node;       
}

/// Entry point for the user. It allows to check if matcher @m
/// matches any loops in file @p.
static std::string evaluate(
  const matchers::ScheduleNodeMatcher &m, const std::string &p) {

  if (!checkIfValid(p))
    return "Error while opening the file!";

  // get the scop.
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, p);
  auto scop = petScop.getScop();

  // get root isl schedule tree.
  isl::schedule_node root = scop.schedule.get_root();
  
  // match.
  root = match(m, root);

  petScop.schedule() = root.get_schedule();
  return petScop.codegen();
}

} // end namespace lang.

TEST(language, compilation) {

  using namespace lang;

  auto matcher = [&]() {
    using namespace matchers;
      return loop(hasDepth(1), anyTree());
  }();

  using namespace matchers;
  auto m1 = loop(hasDepth(1));
  auto m2 = loop(hasDepth(2), loop(hasDepth(1)));
  auto m3 = loop(hasDepth(2));
  auto m4 = loop(hasStmt(anything()));
  auto m5 = loop(hasStmt(hasAccess(anything())));
}

TEST(language, testOne) {

  using namespace lang;

  // getScop
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/nested.c");
  auto scop = petScop.getScop();
  
  // get root node in ISL schedule tree.
  isl::schedule_node root = scop.schedule.get_root();
  root = root.child(0).child(0);

  auto matcher = [&]() {
    using namespace matchers;
    return loop(hasDepth(3));
  }();

  EXPECT_TRUE(matchers::ScheduleNodeMatcher::isMatching(matcher, root) == 1);
  root = root.parent();
  EXPECT_FALSE(matchers::ScheduleNodeMatcher::isMatching(matcher, root) == 1);
}

TEST(language, testTwo) {

  using namespace lang;
  using namespace matchers;

  auto m = loop(hasDepth(3));
  m.setLabel("__test__");
  auto res = evaluate(m, "inputs/nested.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(res.find(m.getLabel()) != std::string::npos);
}

TEST(language, testThree) {

  using namespace lang;
  using namespace matchers;
  
  auto m = loop(hasDepth(3), loop(hasDepth(2)));
  m.setLabel("__test__");
  auto res = evaluate(m, "inputs/nested.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(res.find(m.getLabel()) != std::string::npos);
}

TEST(language, testFour) {

  using namespace lang;
  using namespace matchers;
  
  auto m = loop(hasDepth(10), loop(hasDepth(2)));
  m.setLabel("__test__");
  auto res = evaluate(m, "inputs/nested.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(res.find(m.getLabel()) == std::string::npos);
}

TEST(language, testFive) {

  using namespace lang;
  using namespace matchers;
  
  auto m = loop(hasDepthLessThan(4));
  m.setLabel("__test__");
  auto res = evaluate(m, "inputs/nested.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(res.find(m.getLabel()) != std::string::npos);
}

TEST(language, testSix) {

  using namespace lang;
  using namespace matchers;
  
  auto m = loop(hasDepthMoreThan(2));
  m.setLabel("__test__");
  auto res = evaluate(m, "inputs/nested.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(res.find(m.getLabel()) != std::string::npos);
}

static bool checkCodeAnnotation(std::string code, int numberOfAnnotation,
  std::string annotation) {

  int counter = 0;
  size_t pos = code.find(annotation);
  while (pos != std::string::npos) {
    counter++;
    pos = code.find(annotation, pos + annotation.size());
  } 
  return counter == numberOfAnnotation;
}

TEST(language, testSeven) {
 
  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(hasOnlyPattern("B(i) = A(i-1) + A(i) + A(i+1)"), anyTree());
  }();
  m.setLabel("__test__");

  // this is clutter. Will be removed.
  pointer = std::unique_ptr<std::string>(new std::string("inputs/stencilMix.c"));

  auto res = evaluate(m, "inputs/stencilMix.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif 
  EXPECT_TRUE(checkCodeAnnotation(res, 2, "__test__"));
}

TEST(language, testEight) {

  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(hasOnlyPattern("B(i) = A(i-1) + A(i) + A(i+1)"));
  }();
  m.setLabel("__test__");

  pointer = 
    std::unique_ptr<std::string>(new std::string("inputs/placeholder_assignment.c"));

  auto res = evaluate(m, "inputs/placeholder_assignment.c");  
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 1, "__test__"));
}

TEST(language, testNine) {

  using namespace lang;
    
  auto m = [&]() {
    using namespace matchers;
    return loop(hasOnlyPattern("B(i) = A(i-2) + A(i-1) + A(i) + A(i+1) + A(i+2)"));
  }();
  m.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/stencil_five_points.c"));

  auto res = evaluate(m, "inputs/stencil_five_points.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 1, "__test__"));
}

TEST(language, testTen) {

  using namespace lang;
  
  auto m = [&]() {
    using namespace matchers; 
    return loop(hasOnlyPattern("C(i,j) += A(i,k) * B(k,j)"));
  }();
  m.setLabel("__test__");
  
  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/2mm.c"));

  auto res = evaluate(m, "inputs/2mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 2, "__test__"));
}

TEST(language, testEleven) {

  using namespace lang;
  
  auto m = [&]() {
    using namespace matchers; 
    return loop(hasOnlyPattern("C(i,j) += A(i,k) * B(k,j)"));
  }();
  m.setLabel("__test__");
  
  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/3mm.c"));

  auto res = evaluate(m, "inputs/3mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 3, "__test__"));
}

TEST(language, testTwelve) {

  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(hasOnlyPattern("x(i) = x(i) + A(i,j) * y(j)"));
  }();
  m.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/mvt.c"));

  auto res = evaluate(m, "inputs/mvt.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 1, "__test__"));
}

TEST(language, testThirteen) {

  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(hasOnlyPattern("x(i) = x(i) + A(j,i) * y(j)"));
  }();
  m.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/mvt.c"));

  auto res = evaluate(m, "inputs/mvt.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 1, "__test__"));
}

TEST(language, testFourteen) {

  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(_and(hasOnlyPattern("x(i) = x(i) + A(j,i) * y(j)"),
                     hasOnlyPattern("x(i) = x(i) + A(i,j) * y(j)")));
  }();
  m.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/mvt.c"));

  auto res = evaluate(m, "inputs/mvt.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 0, "__test__"));
}

/// always true.
std::function<bool(isl::schedule_node)> alwaysTrue() {

  return[](isl::schedule_node node) {
    return true;
  };
}

/// always false.
std::function<bool(isl::schedule_node)> alwaysFalse() {

  return[](isl::schedule_node node) {
    return false;
  };
}

TEST(language, testFifteen) {

  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(_or(hasOnlyPattern("x(i) = x(i) + A(j,i) * y(j)"),
                    hasOnlyPattern("x(i) = x(i) + A(i,j) * y(j)")));
  }();
  m.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/mvt.c"));

  auto res = evaluate(m, "inputs/mvt.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 2, "__test__"));
}

TEST(language, testSixteen) {

  using namespace lang;
  
  auto m = [&]() {
    using namespace matchers;
    return loop(hasOnlyPattern("x(i) = x(i) + A(i,j) * y(j)"));
  }();
  m.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/bicg.c"));

  auto res = evaluate(m, "inputs/bicg.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  // We expect no match, as in bicg there are two statements.
  EXPECT_TRUE(checkCodeAnnotation(res, 0, "__test__"));
}

TEST(language, testSeventeen) {

  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(_or(hasOnlyPattern("x(i) = x(i) + A(i,j) * y(j)"),
                    hasOnlyPattern("x(j) = y(j) + A(i,j) * tmp(i)")));
  }();
  m.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/atax.c"));

  auto res = evaluate(m, "inputs/atax.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 2, "__test__"));
}

TEST(language, testEighteen) {

  using namespace lang;
  
  auto m = [&]() {
    using namespace matchers; 
    return loop(_and(hasDepth(3),
                     hasDescendant(loop(
                                   hasOnlyPattern("C(i,j) += A(i,k) * B(k,j)")))));
  }();
  m.setLabel("__test__");
  auto m1 = [&]() {
    using namespace matchers;
    return loop(_and(hasDepth(3),
                     hasDescendant(loop(
                                   hasPattern("C(i,j) += A(i,k) * B(k,j)")))));
  }();
  m1.setLabel("__test__");
  
  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/2mm.c"));

  auto res = evaluate(m, "inputs/2mm.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 2, "__test__"));
  res = evaluate(m1, "inputs/2mm.c");
  EXPECT_TRUE(checkCodeAnnotation(res, 2, "__test__"));
}

TEST(language, testNineteen) {

  using namespace lang;
  
  auto m = [&]() {
    using namespace matchers;
    return loop(hasPattern("x(i) = x(i) + A(i,j) * y(j)"));
  }();
  m.setLabel("__test__");
  auto m1 = [&]() {
    using namespace matchers;
    return loop(hasOnlyPattern("x(i) = x(i) + A(i,j) * Y(j)"));
  }();
  m1.setLabel("__test__");

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/bicg.c"));

  auto res = evaluate(m, "inputs/bicg.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 1, "__test__"));
  res = evaluate(m1, "inputs/bicg.c");
  EXPECT_TRUE(checkCodeAnnotation(res, 0, "__test__"));
}

TEST(language, testTwenty) {

  using namespace matchers;

  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/bicg.c");
  auto scop = petScop.getScop();
  
  auto root = scop.schedule.get_root();
  root = root.child(0).child(1).child(0)
    .child(0).child(1).child(0).child(0);
  auto prefixSchedule = root.get_prefix_schedule_union_map();
  
  std::vector<isl::map> schedulePrefixAsMap{};
  prefixSchedule.foreach_map([&schedulePrefixAsMap] (isl::map m) {
    schedulePrefixAsMap.push_back(m);
    return isl_stat_ok;
  });
  EXPECT_TRUE(schedulePrefixAsMap.size() == 2); 

  isl::union_map reads = 
    scop.reads.curry().apply_domain(schedulePrefixAsMap[0]);

  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _A = arrayPlaceholder();
  auto _X = arrayPlaceholder();
  auto _Y = arrayPlaceholder();
  auto psRead = 
    allOf(access(_A, _i, _j), access(_X, _i), access(_Y, _j));
  auto readMatches = match(reads, psRead);
  EXPECT_TRUE(readMatches.size() == 1);
  
  reads = 
    scop.reads.curry().apply_domain(schedulePrefixAsMap[1]);
  readMatches = match(reads, psRead);
  EXPECT_TRUE(readMatches.size() == 1);
}

TEST(language, testTwentyOne) {

  using namespace lang;

  auto m = [&]() {
    using namespace matchers;
    return loop(hasNumberOfStmtEqualTo(2));
  }();
  m.setLabel("__test__"); 

  pointer =
    std::unique_ptr<std::string>(new std::string("inputs/bicg.c"));

  auto res = evaluate(m, "inputs/bicg.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(checkCodeAnnotation(res, 1, "__test__"));
}

// start testing new api.

isl::schedule_node insertOnMatch(
  const matchers::ScheduleNodeMatcher &m, isl::schedule_node node,
  lang::MatchedStmts &matchedStmts) {

  using namespace lang;

  // if the matcher matches, promote the candidates to matches
  // and remove the candidates to the no match array.
  if (matchers::ScheduleNodeMatcher::isMatching(m, node)) {
    for (size_t i = 0; i < candidateStmts.matches_.size(); i++) {
      matchedStmts.matches_.push_back(candidateStmts.matches_[i]);
      for (size_t j = 0; j < matchedStmts.noMatches_.size(); j++) {
        if (matchedStmts.noMatches_[j] == candidateStmts.matches_[i])
          matchedStmts.noMatches_.erase(matchedStmts.noMatches_.begin() + j);
      }
    }
    candidateStmts.matches_.clear();
  }

  return node;
}

isl::schedule_node match(
  const matchers::ScheduleNodeMatcher &m, isl::schedule_node node,
  lang::MatchedStmts &matchedStmts) {

  node = insertOnMatch(m, node, matchedStmts);

  for (int i = 0; i < node.n_children(); ++i) {
    node = match(m, node.child(i), matchedStmts).parent();
  }
  return node;
}

/// Collect all the statements within the scop.
/// All the collected stmts are stored as "not matched"
/// in "matchedStmts".
void collectAllStmts(isl::schedule_node root, lang::MatchedStmts &matchedStmts) {

  isl_schedule_node_foreach_descendant_top_down(
    root.get(),
    [](__isl_keep isl_schedule_node *nodePtr, void *user) -> isl_bool {
      
      lang::MatchedStmts *p = static_cast<lang::MatchedStmts *>(user);
      isl::schedule_node node = isl::manage_copy(nodePtr);
      if (node.get_type() == isl_schedule_node_leaf) {
        auto name = lang::getStatementName(node.get_prefix_schedule_union_map());
        p->noMatches_.push_back(name);
      }
      return isl_bool_true;
    }, &matchedStmts);
}   
  

lang::MatchedStmts groupStatements(
  const matchers::ScheduleNodeMatcher &m, const std::string &p) {

  using namespace lang;

  MatchedStmts matchedStmts{};

  if (!checkIfValid(p))
    return matchedStmts;

  // get the scop.
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, p);
  auto scop = petScop.getScop();

  isl::schedule_node root = scop.schedule.get_root();

  collectAllStmts(root, matchedStmts);
  root = match(m, root, matchedStmts);

  return matchedStmts;
}

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
    return loop(hasStmt(hasAccess(anything())));
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
