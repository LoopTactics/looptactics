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
struct MatchingResult {
  bool isValid_ = false;
  std::vector<std::pair<std::string, int>> boundedInduction_;
}; 

/// global pointer. It is clutter and will be removed.
/// It is used to pass reads and writes information
/// to matcher callbacks such as "hasPattern"
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

void dump(const MatchingResult &res) {

  std::cout << "is valid: " << res.isValid_ << std::endl;
  for (const auto &r: res.boundedInduction_) {
    std::cout << " { ";
    std::cout << "induction : " << std::get<0>(r) << std::endl;
    std::cout << "schedule dim assigned : " << std::get<1>(r) << std::endl;
    std::cout << " } \n";
  }
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
  if (node.band_n_member() != 1)
    return false;
  return countDepth(node) == depth;
}

/// Implementation of hasDepthLessThan.
static bool hasDepthLessThanImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)
    return false;
  if (node.band_n_member() != 1)
    return false;
  return countDepth(node) < depth;
}

/// Implementation of hasDepthLessThanOrEqual.
static bool hasDepthLessThanOrEqualToImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)  
    return false;
  if (node.band_n_member() != 1)
    return false;
  return countDepth(node) < depth || countDepth(node) == depth;
}

/// Implementation of hasDepthMoreThan.
static bool hasDepthMoreThanImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)  
    return false;
  if (node.band_n_member() != 1)
    return false;
  return countDepth(node) > depth;
}

/// Implementation of hasDepthMoreThanOrEqualTo.
static bool hasDepthMoreThanOrEqualToImpl(isl::schedule_node node, const int depth) {

  if (node.get_type() != isl_schedule_node_band)  
    return false;
  if (node.band_n_member() != 1)
    return false;
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
    dump(results);
  #endif

  return results;
}

/// Extract array name from the arrays obtained from parser.
/// i.e., C(i, j) += A(i, k) * B(k, j) will return A, B and C
static std::set<std::string> extractArrayNames(
  const std::vector<Parser::AccessDescriptor> &accesses) {

  Expects(!accesses.size() == 0);

  std::set<std::string> results{};  
  for (const auto &access : accesses) {
    results.insert(access.array_name_);
  }

  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(results);
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
    default :
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
  for (auto d : ds) {
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
  for (auto d : ds) {
    Expects(d.type_ == Type::READ || d.type_ == Type::WRITE ||
            d.type_ == Type::READ_AND_WRITE);
    if (d.type_ == Type::WRITE || d.type_ == Type::READ_AND_WRITE)
      res.push_back(d);
  }
  return std::move(res);
}

/// Helper function for matching the access patterns.
static MatchingResult match(isl::ctx ctx, isl::union_map accesses, 
  const std::vector<Parser::AccessDescriptor> &ds) {

  Expects(!ds.size() == 0);

  auto res = MatchingResult{};

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

  size_t nMaps = accesses.n_map();
  if (accessSet.size() != nMaps) {
    res.isValid_ = false;
    return res;
  }

  auto matches = match(accesses, allOf(accessSet));
  // we always expect 1 match.
  if (matches.size() == 1) {
    res.isValid_ = true;
    for (auto const &ps : vectorPlaceholderSet) {
      if (!matches[0][ps.p_].candidateSpaces().empty())
        res.boundedInduction_.push_back(
          std::make_pair(ps.id_, matches[0][ps.p_].payload().inputDimPos_));
      else res.isValid_ = false;
    }
  }
  else {
    res.isValid_ = false;
  }
  return res;
}

/// Helper function for implementation of hasPattern.
static bool hasPatternImplHelper(isl::ctx ctx, 
  isl::union_map reads, isl::union_map writes, const std::vector<Parser::AccessDescriptor> &ds) {

  Expects(!reads.is_empty());
  Expects(!writes.is_empty());
  Expects(!ds.size() == 0);

  auto resultMatchingReads = match(ctx, reads, getReadAccessDescriptors(ds));
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(resultMatchingReads);
  #endif

  auto resultMatchingWrites = match(ctx, writes, getWriteAccessDescriptors(ds));
  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(resultMatchingWrites);
  #endif
  
  return resultMatchingReads.isValid_;
}

/// Implementation of hasPattern.
static bool hasPatternImpl(isl::schedule_node node, const std::string &s) {

  if (node.get_type() != isl_schedule_node_band)
    return false;

  // A band node always have a child (may be a leaf), and the prefix
  // schedule of the child includes the partial schedule of the node.
  auto prefixSchedule = node.child(0).get_prefix_schedule_union_map();
  // this is clutter. 
  auto petScop = pet::Scop::parseFile(node.get_ctx(), *pointer);
  // end clutter
  auto scheduledReads = petScop.reads().apply_domain(prefixSchedule); 
  auto scheduledWrites = petScop.writes().apply_domain(prefixSchedule);

  if (scheduledReads.is_empty() || scheduledWrites.is_empty())
    return false;

  std::vector<Parser::AccessDescriptor> accessDescrs{};
  try {
    accessDescrs = Parser::parse(s);
  } catch(...) {
    std::cerr << "Error while parsing: " << s << std::endl;
    std::cout << "parser error!\n";
    return false;
  }

  if (accessDescrs.size() == 0)
    return false;

  #if defined(DEBUG) && defined(LEVEL_ONE)
    dump(accessDescrs);
  #endif
 
  return hasPatternImplHelper(node.get_ctx(), 
    scheduledReads, scheduledWrites, accessDescrs);
}

/// check if the loop has depth @depth
std::function<bool(isl::schedule_node)> hasDepth(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthImpl(node, depth);
  };
}

/// check if the the loop has depth less than @depth
std::function<bool(isl::schedule_node)> hasDepthLessThan(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthLessThanImpl(node, depth);
  };
}

/// check if the loop has depth less than or equal to @depth
std::function<bool(isl::schedule_node)> hasDepthLessThanOrEqualTo(const int depth) {

  return [depth](isl::schedule_node node) {
    return hasDepthLessThanOrEqualToImpl(node, depth);
  };
}

/// check if the loop has depth more than @depth
std::function<bool(isl::schedule_node)> hasDepthMoreThan(const int depth) {

  return[depth](isl::schedule_node node) {
    return hasDepthMoreThanImpl(node, depth);
  };
}

/// check if the loop has depth more or equal than @depth
std::function<bool(isl::schedule_node)> hasDepthMoreThanOrEqualTo(const int depth) {

  return[depth](isl::schedule_node node) {
    return hasDepthMoreThanOrEqualToImpl(node, depth);
  };
}

/// check if the loop has access pattern @s
std::function<bool(isl::schedule_node)> hasPattern(const std::string &s) {

  return[s](isl::schedule_node node) {
    return hasPatternImpl(node, s);
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


/// Utility function
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
  if (node.get_type() == isl_schedule_node_mark)
    return node;

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

TEST(language, testSeven) {
 
  using namespace lang;
  using namespace matchers;

  auto m = [&]() {
    using namespace matchers;
    return band(hasPattern("B(i) = A(i-1) + A(i) + A(i+1)"), anyTree());
  }();
  m.setLabel("__test__");

  // this is clutter. Will be removed.
  pointer = std::unique_ptr<std::string>(new std::string("inputs/stencilMix.c"));

  auto res = evaluate(m, "inputs/stencilMix.c");
  #if defined(DEBUG) && defined(LEVEL_ONE)
    std::cout << res << std::endl;
  #endif 
}
