#include <islutils/builders.h>
#include <islutils/matchers.h>

#include "gtest/gtest.h"

TEST(Transformer, Capture) {
  isl::schedule_node bandNode, filterNode1, filterNode2;
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
            leaf())));
    // clang-format on
  }();

  auto node = [ctx]() {
    using namespace builders;
    auto iterationDomain =
        isl::union_set(ctx, "{S1[i,j]: 0 <= i,j < 10; S2[i,j]: 0 <= i,j < 42}");
    auto sched =
        isl::multi_union_pw_aff(ctx, "[{S1[i,j]->[(i)]; S2[i,j]->[(i)]}, "
                                     "{S1[i,j]->[(j)]; S2[i,j]->[(j)]}]");
    auto filterS1 = isl::union_set(ctx, "{S1[i,j]}");
    auto filterS2 = isl::union_set(ctx, "{S2[i,j]}");

    // clang-format off
    auto builder =
      domain(iterationDomain,
        band(sched,
          sequence(
            filter(filterS1),
            filter(filterS2))));
    // clang-format on

    return builder.build();
  }();

  // Let's find a node.
  // We don't have matcher-based lookups, so lets just use node.child(0) for
  // now.
  ASSERT_TRUE(
      matchers::ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
  isl_schedule_node_dump(node.get());

  // Let's transform!
  auto transformedBuilder = [=]() {
    auto filter1 =
        isl::manage(isl_schedule_node_filter_get_filter(filterNode1.get()));
    auto filter2 =
        isl::manage(isl_schedule_node_filter_get_filter(filterNode2.get()));
    auto schedule = isl::manage(
        isl_schedule_node_band_get_partial_schedule(bandNode.get()));

    using namespace builders;
    // clang-format off
    return
      sequence(
        filter(filter1,
          band(isl::manage(isl_multi_union_pw_aff_intersect_domain(schedule.copy(), filter1.copy())))),
        filter(filter2,
          band(isl::manage(isl_multi_union_pw_aff_intersect_domain(schedule.copy(), filter2.copy())))));
    // clang-format on
  }();
  node = node.child(0);
  node = isl::manage(isl_schedule_node_cut(node.release()));
  node = transformedBuilder.insertAt(node);
  node = node.parent();

  // TODO: how do we keep child subtrees?

  isl_schedule_node_dump(node.get());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
