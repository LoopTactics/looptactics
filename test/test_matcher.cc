#include <islutils/builders.h>
#include <islutils/matchers.h>
#include <islutils/parser.h>

#include "gtest/gtest.h"

TEST(TreeMatcher, ReadFromFile) {
  Scop S = Parser("inputs/one-dimensional-init.c").getScop();
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
              any()))));
  // clang-format on
  auto m2 = sequence(any());
  auto m3 = sequence(filter(any()), filter(any()));
  auto m4 = sequence([](isl::schedule_node n) { return true; }, any());
  auto m5 = sequence([](isl::schedule_node n) { return true; }, filter(leaf()),
                     filter(leaf()));

  auto m6 = sequence(filter(hasNextSibling(filter(any())), any()));
  auto m7 = sequence(filter(
      hasNextSibling(filter(hasPreviousSibling(filter(any())), any())), any()));
  auto m8 = sequence(filter(hasSibling(filter(any())), any()));

  auto m9 = sequence(hasDescendant(band(leaf())), any());
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
          band(any()))));
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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
