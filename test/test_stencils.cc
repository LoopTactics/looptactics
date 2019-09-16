#include "gtest/gtest.h"
#include <iostream>
#include <islutils/access_patterns.h>
#include <islutils/builders.h>
#include <islutils/ctx.h>
#include <islutils/locus.h>
#include <islutils/matchers.h>
#include <islutils/pet_wrapper.h>

using util::ScopedCtx;                                                                            
static inline isl::schedule_node
rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement) {
  // this may not be always legal...
  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}

static isl::schedule_node
replaceOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

static isl::schedule_node
replaceDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement) {
  node = replaceOnce(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderOnce(node.child(i), pattern, replacement).parent();
  }
  return node;
}

bool isNotAnnotated(isl::schedule_node band) {
  if (!band.has_parent())
    return true;
  auto maybeMark = band.parent();
  if (isl_schedule_node_get_type(maybeMark.get()) != isl_schedule_node_mark)
    return true;
  return false;
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

/*
look at the single schedule dimension via dim(...)
we allow multiple dimension for the access pattern.
bool isJacobi1D(isl::ctx ctx, isl::union_map reads, std::string &pattern) {

  using namespace matchers;
  auto i = placeholder(ctx);
  auto arr = arrayPlaceholder();
  // access in range [-1, 1] to the same array for the innermost dimension.
  // Python style (-1).
  auto matches = match(reads,
      allOf(access(arr, dim(-1, i-1)), access(arr, dim(-1, i)), access(arr, dim(-1, i+1))));
  if (matches.size() == 1) {
    pattern = "isJacobi1D";
    return true;
  }
  return false;
}
*/

// the access pattern must have a *single* dimension with [-1:1] access pattern.
// pattern specific matcher.
bool isJacobi1D(isl::ctx ctx, isl::union_map reads, std::string &pattern) {
  
  using namespace matchers;
  auto i = placeholder(ctx);
  auto arr = arrayPlaceholder();
  auto matches = match(reads,
    allOf(access(arr, i-1), access(arr, i), access(arr, i+1)));
  if (matches.size() == 1) {
    pattern = "isJacobi1D";
    return true;
  }
  return false;
}

// pattern specific matcher.
bool isJacobi2D(isl::ctx ctx, isl::union_map reads, std::string &pattern) {

  using namespace matchers;
  auto i = placeholder(ctx);
  auto j = placeholder(ctx);
  auto arr = arrayPlaceholder();  
  auto matches = match(reads,
    allOf(access(arr, i, j), access(arr, i, j-1), access(arr, i, j+1),
          access(arr, i+1, j), access(arr, i-1, j)));
  if (matches.size() == 1) {
    pattern = "isJacobi2D";
    return true;
  } 
  return false;
}

TEST(Stencil, stencilMix) {

  // get Scop.
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/stencilMix.c");
  auto scop = petScop.getScop();
  
  // get root node in ISL schedule tree.
  isl::schedule_node root = scop.schedule.get_root();

  // specific category pattern
  std::string pattern{};

  // callback to check if the pattern is stencil-like (category)
  auto isStencilLike = [&] (isl::schedule_node band) {
    auto prefixSchedule = band.child(0).get_prefix_schedule_union_map();
    auto scheduledReads = scop.reads.curry().apply_domain(prefixSchedule);
    return isJacobi1D(ctx, scheduledReads, pattern) ||
           isJacobi2D(ctx, scheduledReads, pattern);
  };

  // matcher.
  isl::schedule_node bandSchedule, continuation;
  auto matcher = [&]() {
    using namespace matchers;
    return band(_and(isStencilLike, isNotAnnotated),
      bandSchedule, anyTree(continuation));
  }();

  // builder. We rebuild the tree insertin an annotation "isStencilLike"
  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto marker = [&]() {
      return isl::id::alloc(bandSchedule.get_ctx(), "isStencilLike: "+pattern, nullptr);
    };
    auto originalSchedule = [&]() {
      auto descr = BandDescriptor(bandSchedule);
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(continuation); };
    builder = mark(marker, band(originalSchedule, subtree(st)));
  }

  root = replaceDFSPreorderOnce(root, matcher, builder).root();

  // codegen.
  petScop.schedule() = root.get_schedule();
  std::cout << petScop.codegen() << std::endl;
  EXPECT_TRUE(checkCodeAnnotation(petScop.codegen(), 2, "isJacobi1D"));
  EXPECT_TRUE(checkCodeAnnotation(petScop.codegen(), 2, "isJacobi2D"));
}
