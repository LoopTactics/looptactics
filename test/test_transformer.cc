#include <islutils/builders.h>
#include <islutils/locus.h>
#include <islutils/matchers.h>
#include <islutils/parser.h>

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
  auto transformedBuilder = [&]() {
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

struct Schedule : public ::testing::Test {
  virtual void SetUp() override { scop_ = Parser("inputs/nested.c").getScop(); }

  isl::schedule_node topmostBand() {
    return scop_.schedule.get_root().child(0);
  }

  void expectSingleBand(isl::schedule_node node) {
    using namespace matchers;
    EXPECT_TRUE(ScheduleNodeMatcher::isMatching(band(leaf()), node));
  }

  Scop scop_;
};

TEST_F(Schedule, MergeBandsCallLambda) {
  isl::schedule_node parent, child, grandchild;
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(child,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Capturing the matched nodes by-reference since they don't have any values
  // until the matcher was called on a tree.
  // Note that we don't call the lambda yet.
  auto merger = [&]() {
    using namespace builders;
    auto schedule = parent.band_get_partial_schedule().flat_range_product(
        child.band_get_partial_schedule());
    // clang-format off
    return band(schedule,
             subtree(grandchild));
    // clang-format on
  };

  // Keep transforming the tree while possible.
  // Call the builder lambda each time to construct a new builder based on the
  // currently matched nodes (captured by-reference above).
  auto node = topmostBand();
  while (matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
    node = node.cut();
    node = merger().insertAt(node);
  }

  expectSingleBand(node);
}

TEST_F(Schedule, MergeBandsDeclarative) {
  isl::schedule_node parent, child, grandchild;
  // Note that the lambda is called immediately and is only necessary for
  // compound initialization (matchers are not copyable).
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(child,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Use lambdas to lazily initialize the builder with the nodes and values yet
  // to be captured by the matcher.
  auto declarativeMerger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule = [&]() {
      return parent.band_get_partial_schedule().flat_range_product(
          child.band_get_partial_schedule());
    };
    auto st = [&]() { return subtreeBuilder(grandchild); };
    declarativeMerger = band(schedule, subtree(st));
  }

  // Keep transforming the tree while possible.
  auto node = topmostBand();
  while (matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
    node = node.cut();
    node = declarativeMerger.insertAt(node);
  }

  expectSingleBand(node);
}

static isl::union_map computeAllDependences(const Scop &scop) {
  // For the simplest possible dependence analysis, get rid of reference tags.
  auto reads = scop.reads.domain_factor_domain();
  auto mayWrites = scop.mayWrites.domain_factor_domain();
  auto mustWrites = scop.mustWrites.domain_factor_domain();

  // False dependences (output and anti).
  // Sinks are writes, sources are reads and writes.
  auto falseDepsFlow = isl::union_access_info(mayWrites.unite(mustWrites))
                           .set_may_source(mayWrites.unite(reads))
                           .set_must_source(mustWrites)
                           .set_schedule(scop.schedule)
                           .compute_flow();

  isl::union_map falseDeps = falseDepsFlow.get_may_dependence();

  // Flow dependences.
  // Sinks are reads and sources are writes.
  auto flowDepsFlow = isl::union_access_info(reads)
                          .set_may_source(mayWrites)
                          .set_must_source(mustWrites)
                          .set_schedule(scop.schedule)
                          .compute_flow();

  isl::union_map flowDeps = flowDepsFlow.get_may_dependence();

  return flowDeps.unite(falseDeps);
}

// The partial schedule is only defined for those domain elements that passed
// through filters until "node".  Therefore, there is no need to explicitly
// introduce auxiliary dimensions for the filters.
static inline isl::union_map
filterOutCarriedDependences(isl::union_map dependences,
                            isl::schedule_node node) {
  auto partialSchedule = node.get_prefix_schedule_multi_union_pw_aff();
  return dependences.eq_at(partialSchedule);
}

static bool canMerge(isl::schedule_node parentBand,
                     isl::union_map dependences) {
  // Permutability condition: there are no negative distances along the
  // dimensions that are not carried until now by any of dimensions.
  auto t1 = parentBand.band_get_partial_schedule();
  auto t2 = parentBand.child(0).band_get_partial_schedule();
  auto schedule = isl::union_map::from(t1.flat_range_product(t2));
  auto scheduleSpace = isl::map::from_union_map(schedule).get_space();
  auto positiveOrthant =
      isl::set(isl::basic_set::positive_orthant(scheduleSpace.range()));
  dependences = filterOutCarriedDependences(dependences, parentBand);
  return dependences.apply_domain(schedule)
      .apply_range(schedule)
      .deltas()
      .is_subset(positiveOrthant);
}

isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    // this may not be always legal...
    node = node.cut();
    node = replacement.insertAt(node);
  }
  return node;
}

isl::schedule_node
replaceDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement) {
  node = replaceRepeatedly(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderRepeatedly(node.child(i), pattern, replacement)
               .parent();
  }
  return node;
}

TEST_F(Schedule, MergeBandsIfTilable) {
  isl::schedule_node parent, child, grandchild;
  auto dependences = computeAllDependences(scop_);

  auto canMergeCaptureChild = [&child, dependences](isl::schedule_node node) {
    if (canMerge(node.parent(), dependences)) {
      child = node;
      return true;
    }
    return false;
  };

  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(canMergeCaptureChild,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Use lambdas to lazily initialize the builder with the nodes and values yet
  // to be captured by the matcher.
  auto declarativeMerger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule = [&]() {
      auto descr =
          BandDescriptor(parent.band_get_partial_schedule().flat_range_product(
              child.band_get_partial_schedule()));
      descr.permutable = 1;
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(grandchild); };
    declarativeMerger = band(schedule, subtree(st));
  }

  auto node = topmostBand();
  node = replaceDFSPreorderRepeatedly(node, matcher, declarativeMerger);
  expectSingleBand(node);
  EXPECT_EQ(isl_schedule_node_band_get_permutable(node.get()), isl_bool_true);
}

static std::vector<bool> detectCoincidence(isl::schedule_node band,
                                           isl::union_map dependences) {
  std::vector<bool> result;
  auto schedule = band.band_get_partial_schedule();
  int dim = schedule.dim(isl::dim::set);
  result.resize(dim);
  for (int i = 0; i < dim; ++i) {
    auto upa = schedule.get_union_pw_aff(dim);
    auto partialSchedule = isl::union_map::from_union_pw_aff(upa);
    auto deltas = isl::set(dependences.apply_domain(partialSchedule)
                               .apply_range(partialSchedule)
                               .deltas());
    auto zeroSet = [&]() {
      auto aff = isl::aff::var_on_domain(isl::local_space(deltas.get_space()),
                                         isl::dim::set, 0);
      using set_maker::operator==;
      return isl::set(aff == 0);
    }();
    result[i] = deltas.is_subset(zeroSet);
  }
  return result;
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
