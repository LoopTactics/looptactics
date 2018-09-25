#include "gtest/gtest.h"
#include <iostream>
#include <islutils/access_patterns.h>
#include <islutils/builders.h>
#include <islutils/ctx.h>
#include <islutils/locus.h>
#include <islutils/matchers.h>
#include <islutils/pet_wrapper.h>

using util::ScopedCtx;

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
  virtual void SetUp() override {
    scop_ = pet::Scop::parseFile(ctx_, "inputs/nested.c").getScop();
  }

  isl::schedule_node topmostBand() {
    return scop_.schedule.get_root().child(0);
  }

  void expectSingleBand(isl::schedule_node node) {
    using namespace matchers;
    EXPECT_TRUE(ScheduleNodeMatcher::isMatching(band(leaf()), node));
  }

  Scop scop_;
  ScopedCtx ctx_ = ScopedCtx(pet::allocCtx());
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
  auto scheduleSpace = isl::set(schedule.range()).get_space();
  auto positiveOrthant =
      isl::set(isl::basic_set::positive_orthant(scheduleSpace));
  dependences = filterOutCarriedDependences(dependences, parentBand);
  return dependences.apply_domain(schedule)
      .apply_range(schedule)
      .deltas()
      .is_subset(positiveOrthant);
}

static inline isl::schedule_node
rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement) {
  // this may not be always legal...
  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}

isl::schedule_node
replaceOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
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

isl::schedule_node
replaceDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement) {
  node = replaceOnce(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderOnce(node.child(i), pattern, replacement).parent();
  }
  return node;
}

isl::schedule_node mergeIfTilable(isl::schedule_node node,
                                  isl::union_map dependences) {
  isl::schedule_node parent, child, grandchild;

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

  return replaceDFSPreorderRepeatedly(node, matcher, declarativeMerger);
}

TEST_F(Schedule, MergeBandsIfTilable) {
  auto dependences = computeAllDependences(scop_);
  auto node = mergeIfTilable(topmostBand(), dependences);
  expectSingleBand(node);
  EXPECT_EQ(isl_schedule_node_band_get_permutable(node.get()), isl_bool_true);
}

static std::vector<bool> detectCoincidence(isl::schedule_node band,
                                           isl::union_map dependences) {
  std::vector<bool> result;
  auto schedule = band.band_get_partial_schedule();
  int dim = schedule.dim(isl::dim::set);
  if (dependences.is_empty()) {
    result.resize(dim, true);
    return result;
  }
  for (int i = 0; i < dim; ++i) {
    auto upa = schedule.get_union_pw_aff(i);
    auto partialSchedule = isl::union_map::from_union_pw_aff(upa);
    auto deltas = isl::set(dependences.apply_domain(partialSchedule)
                               .apply_range(partialSchedule)
                               .deltas());
    auto zeroSet = [&]() {
      auto lspace = isl::local_space(deltas.get_space());
      auto aff = isl::aff::var_on_domain(lspace, isl::dim::set, 0);
      auto zeroAff = isl::aff(lspace);
      using set_maker::operator==;
      return isl::set(aff == zeroAff);
    }();
    result.push_back(deltas.is_subset(zeroSet));
  }
  return result;
}

isl::schedule_node markCoincident(isl::schedule_node root,
                                  isl::union_map dependences) {
  isl::schedule_node node, child;
  auto matcher = [&]() {
    using namespace matchers;
    return band(node, anyTree(child));
  }();

  auto marker = [&]() {
    auto descr = builders::BandDescriptor(node.band_get_partial_schedule());
    descr.coincident = detectCoincidence(node, dependences);
    return descr;
  };

  auto builder = [&]() {
    using namespace builders;
    return band(marker, subtree(child));
  }();

  return replaceDFSPreorderOnce(root, matcher, builder);
}

TEST_F(Schedule, MarkCoincident) {
  auto dependences = computeAllDependences(scop_);
  markCoincident(scop_.schedule.get_root(), dependences).dump();
}

static bool canSink(isl::schedule_node band) {
  auto dim = band.band_get_partial_schedule().dim(isl::dim::set);
  if (dim < 2) {
    return false;
  }

  auto permutable =
      isl_schedule_node_band_get_permutable(band.get()) == isl_bool_true;
  if (!permutable) {
    return false;
  }

  return true;
}

// pluto-style sinking
// assuming access relations with tags in the range
static int findSinkable(isl::union_map accesses, isl::schedule_node band) {
  auto schedule = band.band_get_partial_schedule();
  auto nDim = schedule.dim(isl::dim::set);
  auto ctx = accesses.get_ctx();

  std::vector<int64_t> weights;
  weights.reserve(nDim);
  for (unsigned i = 0; i < nDim; ++i) {

    auto schedule1D = schedule.get_union_pw_aff(i);
    auto scheduleMap1D = isl::union_map::from_union_pw_aff(schedule1D);
    auto scheduledAccess = accesses.apply_domain(scheduleMap1D);

    using namespace matchers;
    int nRepeated =
        match(scheduledAccess, allOf(access(dim(-1, stride(ctx, 0))))).size();
    int nLocal = 0;
    for (int s = 1; s <= 4; ++s) {
      nLocal +=
          match(scheduledAccess, allOf(access(dim(-1, stride(ctx, s))))).size();
    }
    int nAccesses = scheduledAccess.n_map();
    int nNonLocal = nAccesses - nRepeated - nLocal;
    bool isVectorizable = nNonLocal == 0;

    // count # of stride-zero (+4 per access)
    // count # of stride-one (+2 per access)
    // is vectorizable <= # of stride-zero + # of stride-one = # of accesses
    // (bonus 8) all other strides (-16 per access)
    weights.push_back(2 * nLocal + 4 * nRepeated + 8 * isVectorizable -
                      16 * nNonLocal);
  }

  auto maxWeightIter = std::max_element(weights.begin(), weights.end());
  return std::distance(weights.begin(), maxWeightIter);
}

TEST(Transformer, SinkLocal) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop = pet::Scop::parseFile(ctx, "inputs/1mm_fused.c").getScop();

  auto dependences = computeAllDependences(scop);
  scop.schedule =
      mergeIfTilable(scop.schedule.get_root(), dependences).get_schedule();

  isl::schedule_node node, child;
  auto matcher = matchers::band(
      [&node](isl::schedule_node n) {
        if (canSink(n)) {
          node = n;
          return true;
        }
        return false;
      },
      matchers::anyTree(child));

  isl::union_map accesses =
      scop.reads.unite(scop.mayWrites).unite(scop.mustWrites).curry();

  builders::ScheduleNodeBuilder builder = builders::band(
      [&node, &accesses]() {
        int pos = findSinkable(accesses, node);
        auto schedule = node.band_get_partial_schedule();
        auto scheduleAtPos = schedule.get_union_pw_aff(pos);
        schedule = schedule.drop_dims(isl::dim::set, pos, 1);
        schedule =
            schedule.flat_range_product(isl::multi_union_pw_aff(scheduleAtPos));

        builders::BandDescriptor descriptor(node);
        descriptor.partialSchedule = schedule;
        auto isCoincident = descriptor.coincident.at(pos);
        descriptor.coincident.erase(descriptor.coincident.begin() + pos);
        descriptor.coincident.push_back(isCoincident);
        return descriptor;
      },
      builders::subtree(child));

  node = replaceDFSPreorderOnce(scop.schedule.get_root(), matcher, builder);

  // Check that we indeed sink the "j" loop.
  // clang-format off
  auto expected = isl::union_map(ctx,
      "{ S_0[i, j, k] -> [o0, o1, o2, o3] : o0 = i and o1 = k and o2 = j and o3 = 0;"
        "S_1[i, j, k] -> [o0, o1, o2, o3] : o0 = i and o1 = k and o2 = j and o3 = 1 }");
  // clang-format on
  EXPECT_TRUE(node.get_schedule().get_map().is_subset(expected));
}

// Check that all relevant parts of the code (loops and transformed statements)
// are correctly generated.  In particular, check that loops are generated in
// the right order.  Whitespace is ignored.
TEST(Transformer, Codegen) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/nested.c");

  std::string loop1 = "for (int c0 = 0; c0 <= min(1023, n - 2); c0 += 1)";
  std::string loop2 = "for (int c1 = 0; c1 < n - c0 - 1; c1 += 1)";
  std::string loop3 = "for (int c2 = n - 1; c2 <= n + 41; c2 += 1)";
  std::string loop4 = "for (int c3 = c0 + 1; c3 < n - c1; c3 += 1)";
  std::string stmt = "foo((c0), (c1), (c2), (c3));";
  auto result = petScop.codegen();

  auto loop1pos = result.find(loop1);
  auto loop2pos = result.find(loop2, loop1pos + loop1.length());
  auto loop3pos = result.find(loop3, loop2pos + loop2.length());
  auto loop4pos = result.find(loop4, loop3pos + loop3.length());
  auto stmtpos = result.find(stmt, loop4pos + loop4.length());

  // Note that we don't care about the particular positions in the string, only
  // that the relation between them holds. Therefore we use ASSERT_TRUE on
  // relations to avoid useless and potentially large (npos) numbers output in
  // case an assertion fails.
  ASSERT_TRUE(loop1pos != std::string::npos);
  ASSERT_TRUE(loop2pos != std::string::npos);
  ASSERT_TRUE(loop3pos != std::string::npos);
  ASSERT_TRUE(loop4pos != std::string::npos);
  ASSERT_TRUE(stmtpos != std::string::npos);

  ASSERT_TRUE(loop2pos > loop1pos);
  ASSERT_TRUE(loop3pos > loop2pos);
  ASSERT_TRUE(loop4pos > loop3pos);
  ASSERT_TRUE(stmtpos > loop4pos);
}

TEST(Transformer, InjectStatement) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/stencil.c");

  isl::schedule_node node;
  auto matcher = [&]() {
    using namespace matchers;
    return anyTree(node);
  }();

  matchers::ScheduleNodeMatcher::isMatching(
      matcher, petScop.getScop().schedule.get_root().child(0));

  auto builder = [&]() {
    using namespace builders;
    return extension(
        isl::union_map(ctx, "[] -> {[]->someLongAndHopefullyUniqueName[]:}"),
        sequence(filter(isl::union_set(
                     ctx, "[] -> {someLongAndHopefullyUniqueName[]:}")),
                 filter(petScop.getScop().domain().universe(), subtree(node))));
  }();

  auto sched = builder.insertAt(petScop.getScop().schedule.get_root().child(0))
                   .get_schedule();
  petScop.schedule() = sched;
  auto code = petScop.codegen();
  EXPECT_TRUE(code.find("someLongAndHopefullyUniqueName") != std::string::npos);
}

static isl::multi_union_pw_aff getSchedulePointTile(isl::schedule_node node,
                                                    isl::multi_union_pw_aff t) {
  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  return sched.sub(t);
}

static isl::multi_union_pw_aff getScheduleTile(isl::schedule_node node,
                                               std::vector<int> tileSizes) {
  assert(tileSizes.size() != 0 && "empty tileSizes array");
  isl::space space = isl::manage(isl_schedule_node_band_get_space(node.get()));
  unsigned dims = space.dim(isl::dim::set);
  assert(dims == tileSizes.size() &&
         "number of dimensions should match tileSizes size");

  isl::multi_val sizes = isl::multi_val::zero(space);
  for (unsigned i = 0; i < dims; ++i) {
    int tileSize = tileSizes[i];
    sizes = sizes.set_val(i, isl::val(node.get_ctx(), tileSize));
  }

  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  for (unsigned i = 0; i < dims; ++i) {

    isl::union_pw_aff upa = sched.get_union_pw_aff(i);
    isl::val v = sizes.get_val(i);
    upa = upa.scale_down_val(v);
    upa = upa.floor();
    sched = sched.set_union_pw_aff(i, upa);
  }
  return sched;
}

static isl::multi_union_pw_aff swapDims(isl::multi_union_pw_aff ps,
                                        int firstDim, int secondDim) {
  auto scheduleFirstDim = ps.get_union_pw_aff(firstDim);
  auto scheduleSecondDim = ps.get_union_pw_aff(secondDim);
  ps = ps.set_union_pw_aff(secondDim, scheduleFirstDim);
  ps = ps.set_union_pw_aff(firstDim, scheduleSecondDim);
  return ps;
}

TEST(Transformer, MatchMatmul) {

  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/1mmWithoutInitStmt.c");
  auto scop = petScop.getScop();

  auto dependences = computeAllDependences(scop);
  scop.schedule =
      mergeIfTilable(scop.schedule.get_root(), dependences).get_schedule();

  isl::schedule_node root = scop.schedule.get_root();

  using namespace matchers;
  isl::schedule_node node;
  // clang-format off
  auto matcher = band(
    [&node] (isl::schedule_node n) {
      if (isl_schedule_node_band_n_member(n.get()) < 3) {
        return false;
      } else {
        node = n;
        return true;
      }
    },
    leaf());
  // clang-format on

  ASSERT_TRUE(ScheduleNodeMatcher::isMatching(matcher, root.child(0)));

  isl::union_map reads = scop.reads.curry();
  isl::union_map writes = scop.mustWrites.curry();

  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto _ii = placeholder(ctx);
  auto _jj = placeholder(ctx);

  auto _A = arrayPlaceholder();
  auto _B = arrayPlaceholder();
  auto _C = arrayPlaceholder();

  auto psRead =
      allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
  auto readMatches = match(reads, psRead);
  auto psWrite = allOf(access(_A, _ii, _jj));
  auto writeMatches = match(writes, psWrite);

  // tmp[j][i] = alpha * A[i][k] * B[k][j] + tmp[i][j]
  // pass the checks. this is because we do not link
  // read and write at the moment. Placeholder are _not_
  // reused between different calls to allOf. We can overcome
  // this inspecting the placeholder for the write and the read.
  // They should be equal.

  ASSERT_EQ(readMatches.size(), 1u);
  ASSERT_EQ(writeMatches.size(), 1u);

  // check index for read and write are equal
  ASSERT_TRUE(writeMatches[0][_ii].payload().inputDimPos_ ==
              readMatches[0][_i].payload().inputDimPos_);
  ASSERT_TRUE(writeMatches[0][_jj].payload().inputDimPos_ ==
              readMatches[0][_j].payload().inputDimPos_);

  // D[i][j] = alpha * A[i][k] * B[k][j] + tmp[i][j]
  // pass the test. We may want to apply the same check
  // as before also for the accessed array.

  // step 1. Loop interchange.
  // Interchange the loops in the loop nest such that
  // j is the outermost loop followed by k and i.
  int iPosOriginal = readMatches[0][_i].payload().inputDimPos_;
  int jPosOriginal = readMatches[0][_j].payload().inputDimPos_;
  int kPosOriginal = readMatches[0][_k].payload().inputDimPos_;

  // transformer to interchange dimensions
  using namespace builders;
  ScheduleNodeBuilder swapDimensions =
      band([&node, &iPosOriginal, &jPosOriginal, &kPosOriginal]() {
        auto originalSchedule = node.band_get_partial_schedule();
        auto newSchedule = originalSchedule;
        if (jPosOriginal != 0) {
          if (iPosOriginal == 0) {
            newSchedule = swapDims(newSchedule, jPosOriginal, iPosOriginal);
            iPosOriginal = jPosOriginal;
            jPosOriginal = 0;
          }
          if (kPosOriginal == 0) {
            newSchedule = swapDims(newSchedule, jPosOriginal, kPosOriginal);
            kPosOriginal = jPosOriginal;
            jPosOriginal = 0;
          }
        }
        if (kPosOriginal != 1) {
          newSchedule = swapDims(newSchedule, kPosOriginal, iPosOriginal);
        }
        return newSchedule;
      });

  node = rebuild(node, swapDimensions);
  root = node.root();
  petScop.schedule() = root.get_schedule();
  std::string loopJ = "for (int c0 = 0; c0 <= 1023; c0 += 1)";
  std::string loopK = "for (int c1 = 0; c1 <= 1023; c1 += 1)";
  std::string loopI = "for (int c2 = 0; c2 <= 1023; c2 += 1)";
  std::string stmt =
      "tmp[c2][c0] = ((((alpha) * A[c2][c1]) * B[c1][c0]) + tmp[c2][c0]);";
  auto result = petScop.codegen();
  auto loopJPos = result.find(loopJ);
  auto loopKPos = result.find(loopK, loopJPos + loopJ.length());
  auto loopIPos = result.find(loopI, loopKPos + loopK.length());
  auto stmtPos = result.find(stmt, loopIPos + loopI.length());
  ASSERT_TRUE(loopJPos != std::string::npos);
  ASSERT_TRUE(loopKPos != std::string::npos);
  ASSERT_TRUE(loopIPos != std::string::npos);
  ASSERT_TRUE(stmtPos != std::string::npos);
  ASSERT_TRUE(loopKPos > loopJPos);
  ASSERT_TRUE(loopIPos > loopKPos);
  ASSERT_TRUE(stmtPos > loopIPos);

  // step 2. create macro-kernel
  // For the micro and macro kernels we assume
  // given values for the tile size.
  // Note: In polly the interchange is performed
  // on the tile loops, while in the paper on the
  // point loops we follow the paper.
  // We tile all the three loops j p and i
  // to create jc pc and ic and we interchange
  // pc and ic. We use the same tile factor of 32
  // for all the dimensions.
  using namespace builders;
  // set tile values manually
  int dimOutNum = isl_schedule_node_band_n_member(node.get());
  std::vector<int> tileSizes(dimOutNum);
  tileSizes = {32, 32, 32};

  // tile node and get partial schedule
  auto tileSchedule = getScheduleTile(node, tileSizes);
  auto pointSchedule = getSchedulePointTile(node, tileSchedule);

  // clang-format off
  ScheduleNodeBuilder macroKernel =
    band(tileSchedule,
      band(
        [&pointSchedule, &dimOutNum]() {
          auto newPartialSchedule =
              swapDims(pointSchedule, dimOutNum - 2, dimOutNum - 1);
          return newPartialSchedule;
        }));
  // clang-format on

  node = rebuild(node, macroKernel);
  root = node.root();
  petScop.schedule() = root.get_schedule();
  loopJ = "for (int c0 = 0; c0 <= 31; c0 += 1)";
  loopK = "for (int c1 = 0; c1 <= 31; c1 += 1)";
  loopI = "for (int c2 = 0; c2 <= 31; c2 += 1)";
  std::string loopJc = "for (int c3 = 31 * c0; c3 <= 31 * c0 + 31; c3 += 1)";
  std::string loopIc = "for (int c4 = 31 * c2; c4 <= 31 * c2 + 31; c4 += 1)";
  std::string loopKc = "for (int c5 = 31 * c1; c5 <= 31 * c1 + 31; c5 += 1)";
  stmt = "tmp[c2 + c4][c0 + c3] = ((((alpha) * A[c2 + c4][c1 + c5]) * B[c1 + "
         "c5][c0 + c3]) + tmp[c2 + c4][c0 + c3]);";
  result = petScop.codegen();
  auto loopJcPos = result.find(loopJc);
  auto loopIcPos = result.find(loopIc);
  auto loopKcPos = result.find(loopKc);
  ASSERT_TRUE(loopJcPos != std::string::npos);
  ASSERT_TRUE(loopKcPos != std::string::npos);
  ASSERT_TRUE(loopIcPos != std::string::npos);
  ASSERT_TRUE(loopKcPos > loopIcPos);
  ASSERT_TRUE(loopIcPos > loopJcPos);

  // match micro-kernel
  auto matcherMicroKernel = [&]() {
    using namespace matchers;
    return band(node, leaf());
  }();

  ASSERT_TRUE(ScheduleNodeMatcher::isMatching(matcherMicroKernel,
                                              root.child(0).child(0)));

  // create micro-kernel
  // tile ic and jc with a tile factor of 2
  // do not tile pc. The tiling produces two new
  // loops ir and jr.
  tileSizes = {2, 2, 1};
  // tile node and get partial schedule
  tileSchedule = getScheduleTile(node, tileSizes);
  pointSchedule = getSchedulePointTile(node, tileSchedule);

  // clang-format off
  ScheduleNodeBuilder microKernel =
      band(tileSchedule,
        band([&node, &pointSchedule]() {
          auto descr = BandDescriptor(pointSchedule);
          descr.astOptions =
            isl::union_set(node.get_ctx(), "{unroll[x]}");
          return descr;
        }));
  // clang-format on

  node = rebuild(node, microKernel);
  root = node.root();
  petScop.schedule() = root.get_schedule();
  result = petScop.codegen();
  std::cout << result << std::endl;
}
