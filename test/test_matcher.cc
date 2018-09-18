#include <islutils/builders.h>
#include <islutils/ctx.h>
#include <islutils/matchers.h>
#include <islutils/pet_wrapper.h>

#include "gtest/gtest.h"

using util::ScopedCtx;

TEST(TreeMatcher, ReadFromFile) {
  auto ctx = ScopedCtx(pet::allocCtx());
  Scop S = pet::Scop::parseFile(ctx, "inputs/one-dimensional-init.c").getScop();
  EXPECT_TRUE(!S.schedule.is_null());
}

TEST(TreeMatcher, CompileTest) {
  using namespace matchers;

  // clang-format off
  auto matcher =
      domain(
        context(
          sequence(
            band(
              leaf()),
            band(
              leaf()),
            filter(
              anyTree()))));
  // clang-format on
  auto m2 = sequence(anyTree());
  auto m3 = sequence(filter(anyTree()), filter(anyTree()));
  auto m4 = sequence([](isl::schedule_node n) { return true; }, anyTree());
  auto m5 = sequence([](isl::schedule_node n) { return true; }, filter(leaf()),
                     filter(leaf()));

  auto m6 = sequence(filter(hasNextSibling(filter(anyTree())), anyTree()));
  auto m7 = sequence(filter(
      hasNextSibling(filter(hasPreviousSibling(filter(anyTree())), anyTree())),
      anyTree()));
  auto m8 = sequence(filter(hasSibling(filter(anyTree())), anyTree()));

  auto m9 = sequence(hasDescendant(band(leaf())), anyTree());
  auto m10 = band(leaf());
  auto m11 = band([](isl::schedule_node n) { return true; }, leaf());
}

static isl::schedule_node makeGemmTree() {
  using namespace builders;
  auto ctx = isl::ctx(isl_ctx_alloc());
  auto iterationDomain = isl::union_set(
      ctx, "{S1[i,j]: 0 <= i,j < 10; S2[i,j,k]: 0 <= i,j,k < 42}");
  auto sched =
      isl::multi_union_pw_aff(ctx, "[{S1[i,j]->[(i)]; S2[i,j]->[(i)]}, "
                                   "{S1[i,j]->[(j)]; S2[i,j]->[(j)]}]");
  auto filterS1 = isl::union_set(ctx, "{S1[i,j]}");
  auto filterS2 = isl::union_set(ctx, "{S2[i,j]}");
  auto innerSched = isl::multi_union_pw_aff(ctx, "[{S2[i,j,k]->[(k)]}]");

  // clang-format off
  auto builder =
    domain(iterationDomain,
      band(sched,
        sequence(
          filter(filterS1),
          filter(filterS2,
            band(innerSched)))));
  // clang-format on

  return builder.build();
}

TEST(TreeMatcher, AnyMatchesLeaf) {
  using namespace matchers;
  // clang-format off
  auto matcher =
    band(
      sequence(
        filter(
          leaf()),
        filter(
          band(anyTree()))));
  // clang-format on

  auto node = makeGemmTree();
  EXPECT_TRUE(ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
}

TEST(TreeMatcher, LeafMatchesLeaf) {
  using namespace matchers;
  // clang-format off
  auto matcher =
    band(
      sequence(
        filter(
          leaf()),
        filter(
          band(
            leaf()))));
  // clang-format on

  auto node = makeGemmTree();
  EXPECT_TRUE(ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
}

TEST(TreeMatcher, AnyForestMatchesMultiple) {
  using namespace matchers;
  // clang-format off
  auto matcher =
    band(
      sequence(
        anyForest()));
  // clang-format on

  auto node = makeGemmTree();
  EXPECT_TRUE(ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
}

TEST(TreeMatcher, AnyForestMatchesOne) {
  using namespace matchers;
  // clang-format off
  auto matcher =
    band(
      anyForest());
  // clang-format on

  auto node = makeGemmTree();
  EXPECT_TRUE(ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
}

TEST(TreeMatcher, AnyForestMatchesLeaf) {
  using namespace matchers;
  // clang-format off
  auto matcher =
    band(
      sequence(
        filter(
          anyForest()),
        filter(
          band(
            anyForest()))));
  // clang-format on

  auto node = makeGemmTree();
  EXPECT_TRUE(ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
}

TEST(TreeMatcher, AnyForestCapture) {
  using namespace matchers;
  std::vector<isl::schedule_node> captures;
  isl::schedule_node first, second;

  // clang-format off
  auto matcher =
    band(
      sequence(
        anyForest(captures)));
  // clang-format on

  auto node = makeGemmTree();
  ASSERT_TRUE(ScheduleNodeMatcher::isMatching(matcher, node.child(0)));

  // clang-format off
  auto matcher2 =
    band(
      sequence(
        anyTree(first),
        anyTree(second)));
  // clang-format on
  ASSERT_TRUE(ScheduleNodeMatcher::isMatching(matcher2, node.child(0)));

  ASSERT_EQ(captures.size(), 2u);
  EXPECT_TRUE(captures[0].is_equal(first));
  EXPECT_TRUE(captures[1].is_equal(second));
}
