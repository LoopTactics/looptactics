#include <islutils/builders.h>
#include <islutils/matchers.h>

#include "gtest/gtest.h"

TEST(Transformer, Capture) {
  isl::schedule_node bandNode, filterNode1, filterNode2, filterSubtree;
  auto ctx = isl::ctx(isl_ctx_alloc());

  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return
      band(bandNode,
        sequence(
          filter(filterNode1,
            leaf()),
          filter(filterNode2,
            anyTree(filterSubtree))));
    // clang-format on
  }();

  auto node = [ctx]() {
    using namespace builders;
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
  }();

  // Let's find a node.
  // We don't have matcher-based lookups, so lets just use node.child(0) for
  // now.
  ASSERT_TRUE(
      matchers::ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
  node.dump();

  // Let's transform!
  auto transformedBuilder = [=]() {
    auto filter1 = filterNode1.filter_get_filter();
    auto filter2 = filterNode2.filter_get_filter();
    auto schedule = bandNode.band_get_partial_schedule();

    using namespace builders;
    // clang-format off
    return
      sequence(
        filter(filter1,
          band(schedule.intersect_domain(filter1))),
        filter(filter2,
          band(schedule.intersect_domain(filter2),
            subtree(filterSubtree))));
    // clang-format on
  }();
  node = node.child(0);
  node = node.cut();
  node = transformedBuilder.insertAt(node);
  node = node.parent();

  node.dump();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
