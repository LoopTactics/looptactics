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

  auto matcher = domain(context(sequence(band(), band(), filter())));
  auto m2 = sequence();
  auto m3 = sequence(filter(), filter());
  auto m4 = sequence([](isl::schedule_node n) { return true; });
  auto m5 = sequence([](isl::schedule_node n) { return true; }, filter(), filter());

  auto m6 = sequence(filter(hasNextSibling(filter())));
  auto m7 = sequence(filter(hasNextSibling(filter(hasPreviousSibling(filter())))));
  auto m8 = sequence(filter(hasSibling(filter())));

  auto m9 = sequence(hasDescendant(band()));
  auto m10 = band(leaf());
  auto m11 = band([](isl::schedule_node n) { return true;}, leaf());

  // access pattern matchers.
  auto m12 = read('X');
  auto m13 = read('X','Y');
  auto m14 = write('Z');
  auto m15 = write('S','Y');
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

TEST(TreeMatcher, EmptyNotMatchesLeaf) {
  using namespace matchers;
  // clang-format off
  auto matcher =
    band(
      sequence(
        filter(
          leaf()),
        filter(
          band())));
  // clang-format on

  auto node = makeGemmTree();
  EXPECT_FALSE(ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
