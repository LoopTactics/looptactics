#include "gtest/gtest.h"
#include "islutils/matchers.h"
#include "islutils/ctx.h"
#include "islutils/pet_wrapper.h"
#include "islutils/builders.h"
#include "islutils/gsl/gsl_assert"
#include "islutils/parser.h"

#include <boost/filesystem.hpp>
#include <stack>
#include <iostream>

using util::ScopedCtx;

namespace lang {

/// Global scop class which contains
/// information for matcher callbacks.
class GlobalScop {
  public:
  isl::union_map reads_;
  isl::union_map writes_;
};

GlobalScop *S;


#ifdef DEBUG
void dump(const std::vector<Parser::AccessDescriptor> & ads) {

  using namespace Parser;
  for (const auto &ad : ads) {
    std::cout << "Array name: " << ad.array_name_ << std::endl;
    if (ad.type_ == Type::READ)
      std::cout << "Read access" << std::endl;
    else std::cout << "Write access" << std::endl;
    for (const auto &af : ad.affine_access_) {
      std::cout << "Induction name: " << af.induction_var_name_ << std::endl;
      std::cout << "Increment: " << af.increment_ << std::endl;
      std::cout << "Coefficient: " << af.coefficient_ << std::endl;
    }
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

/// Implementation of hasPattern.
static bool hasPatternImpl(isl::schedule_node node, const std::string &s) {

  if (node.get_type() != isl_schedule_node_band)
    return false;
  if (!node.has_children())
    return false;
  auto mayBeLeaf = node.child(0);
  if (mayBeLeaf.get_type() != isl_schedule_node_leaf)
    return false;
  auto schedule = mayBeLeaf.get_prefix_schedule_union_map();
  auto filteredReads = S->reads_.apply_domain(schedule);
  auto filteredWrites = S->writes_.apply_domain(schedule);
  auto accessDescr = Parser::parse(s);

  #ifdef DEBUG
    dump(accessDescr);
  #endif
  return false; 
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

  if (matchers::ScheduleNodeMatcher::isMatching(m, node)) 
    node = node.insert_mark(isl::id::alloc(node.get_ctx(), m.getLabel(), nullptr));

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

  for (int i = 0; i < node.n_children(); i++)
    node = match(m, node.child(i)).parent();

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
  #ifdef DEBUG
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
  #ifdef DEBUG
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
  #ifdef DEBUG
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
  #ifdef DEBUG
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
  #ifdef DEBUG
    std::cout << res << std::endl;
  #endif
  EXPECT_TRUE(res.find(m.getLabel()) != std::string::npos);
}

TEST(language, testSeven) {
 
  using namespace lang;
  using namespace matchers;
  // this is clutter and will be removed.
  // Ideally, the only function exposed to the user
  // will be "evaluate". The clutter is needed to give reads and
  // writes infos to "hasPattern" callback.
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/stencilMix.c");
  auto r = petScop.reads(); 
  auto w = petScop.reads();
  auto Gb = GlobalScop();
  S = &Gb;
  S->reads_ = r;
  S->writes_ = w;
  // end clutter.

  auto m = loop(hasPattern("B(i) = A(i-1) + A(i) + A(i+1)"));
  m.setLabel("__test__");
  auto res = evaluate(m, "inputs/stencilMix.c");
  #ifdef DEBUG
    std::cout << res << std::endl;
  #endif 
}
