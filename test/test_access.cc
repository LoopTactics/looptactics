#include "islutils/access.h"
#include "islutils/access_patterns.h"
#include "islutils/ctx.h"
#include "islutils/pet_wrapper.h"

#include "gtest/gtest.h"

using util::ScopedCtx;
using namespace matchers;

static matchers::PlaceholderSet<SingleInputDim, FixedOutDimPattern<SimpleAff>>
makePlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p1(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 1));
  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p2(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 0));
  p1.pattern_.coefficient_ = isl::val(ctx, 1);
  p2.pattern_.coefficient_ = isl::val(ctx, 2);
  p1.pattern_.constant_ = isl::val::zero(ctx);
  p2.pattern_.constant_ = isl::val::zero(ctx);
  PlaceholderSet<SingleInputDim, FixedOutDimPattern<SimpleAff>> ps;
  ps.placeholders_.push_back(p1);
  ps.placeholders_.push_back(p2);
  ps.placeholderFolds_.push_back(0);
  ps.placeholderFolds_.push_back(1);

  // Both placeholders must appear in the same relation.
  ps.placeholderGroups_.emplace_back();
  ps.placeholderGroups_.back().push_back(0);
  ps.placeholderGroups_.back().push_back(1);

  return ps;
}

TEST(AccessMatcher, TwoMapsTwoMatches) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto ps = makePlaceholderSet(ctx);
  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=2*j and y=i}");

  // There are 2 possible matches: the first and the second map of the union.
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 2);
}

TEST(AccessMatcher, PositionalArguments) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=2*j and y=i}");

  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto matches = match(umap, allOf(access(2 * _1, _2)));
  // There are 2 possible matches: the first and the second map of the union.
  EXPECT_EQ(matches.size(), 2);
}

TEST(AccessMatcher, MatchResults) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a,b]: a=i and b=j;"
                                  " [i,j]->B[a,b]: a=j and b=i;"
                                  " [i,j]->C[a,b]: a=i and b=j}");

  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto matches = match(umap, allOf(access(_1, _2)));
  ASSERT_EQ(matches.size(), 3);

  // Check that we can inspect the result using placeholder objects.
  for (const auto &m : matches) {
    EXPECT_FALSE(m[_1].candidateSpaces().empty());
    EXPECT_FALSE(m[_2].candidateSpaces().empty());

    // Check that we got the matching right.
    EXPECT_TRUE((m[_1].payload().inputDimPos_ == 0 &&
                 m[_2].payload().inputDimPos_ == 1) ^
                (m[_1].payload().inputDimPos_ == 1 &&
                 m[_2].payload().inputDimPos_ == 0));
  }
}

TEST(AccessMatcher, MatchResultsThreeDimensional) {
  using namespace matchers;
  
  auto ctx = ScopedCtx(); 
  auto umap = isl::union_map(ctx, "{[i,j,k]->A[a,b]: a=i and b=k;"
                                  " [i,j,k]->B[a,b]: a=k and b=j;"
                                  " [i,j,k]->C[a,b]: a=i and b=j}");
  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto matches = match(umap, allOf(access(_i, _j), access(_i, _k), access(_k, _j)));
  ASSERT_EQ(matches.size(), 1);

  EXPECT_FALSE(matches[0][_i].candidateSpaces().empty());
  EXPECT_FALSE(matches[0][_j].candidateSpaces().empty());
  EXPECT_FALSE(matches[0][_k].candidateSpaces().empty());
  EXPECT_TRUE(matches[0][_i].payload().inputDimPos_ == 0);
  EXPECT_TRUE(matches[0][_j].payload().inputDimPos_ == 1);
  EXPECT_TRUE(matches[0][_k].payload().inputDimPos_ == 2); 
}

TEST(AccessMatcher, MatchResultsMultipleSpaces) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a,b]: a=i and b=j;"
                                  " [i,j]->B[a,b]: a=j and b=i;"
                                  " [i,j]->C[a,b]: a=i and b=j}");
  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto matches = match(umap, allOf(access(_1, _2), access(_1, _2)));
  // Permutations of A,C spaces are allowed.
  ASSERT_EQ(matches.size(), 2);

  // Each match should have both spaces.
  for (const auto &m : matches) {
    EXPECT_EQ(m[_1].candidateSpaces().size(), 2);
    EXPECT_EQ(m[_2].candidateSpaces().size(), 2);
  }
}

TEST(AccessMatcher, MatchResultsNoDuplicateSpace) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a,b]: a=i and b=i}");
  auto _1 = placeholder(ctx);
  auto matches = match(umap, allOf(access(_1, _1)));
  ASSERT_EQ(matches.size(), 1);

  // Do not expect the same space twice.
  auto m = matches.front();
  EXPECT_EQ(m[_1].candidateSpaces().size(), 1);
}

static matchers::PlaceholderSet<SingleInputDim, FixedOutDimPattern<SimpleAff>>
makeTwoGroupPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  auto ps = makePlaceholderSet(ctx);

  // Make this similar to p1.
  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p3(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 1));
  p3.pattern_.coefficient_ = isl::val(ctx, 1);
  p3.pattern_.constant_ = isl::val::zero(ctx);
  ps.placeholders_.push_back(p3);
  ps.placeholderFolds_.push_back(2);

  // Create a new group for p3.
  ps.placeholderGroups_.emplace_back();
  ps.placeholderGroups_.back().push_back(2);

  return ps;
}

TEST(AccessMatcher, TwoGroups) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=42*j and y=i}");
  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto matches = match(
      umap, allOf(access(dim(0, 2 * _2), dim(1, _1)), access(dim(1, _1))));
  // Only one match possible: anonymous space to p1,p2, "A" space to p3.
  // Because p3 and p1 belong to different groups, they cannot both match the
  // anonymous space.
  // Because p1 and p2 belong to the same group, only the anonymous space can
  // match them (p2 does not match the "A" space).
  EXPECT_EQ(matches.size(), 1);

  auto _3 = placeholder(ctx);
  matches = match(
      umap, allOf(access(dim(0, 2 * _2), dim(1, _1)), access(dim(1, _3))));
  // No matches possible because _3 cannot be assigned the same candidate as _1.
  EXPECT_EQ(matches.size(), 0);
}

TEST(AccessMatcher, TwoMapsOneMatch) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto ps = makePlaceholderSet(ctx);

  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=j and y=i}");
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 1);
}

static matchers::PlaceholderSet<SingleInputDim, FixedOutDimPattern<SimpleAff>>
makeSameGroupSameFoldPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p1(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 1));
  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p2(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 0));
  ;
  p1.pattern_.coefficient_ = isl::val(ctx, 1);
  p2.pattern_.coefficient_ = isl::val(ctx, 1);
  p1.pattern_.constant_ = isl::val::zero(ctx);
  p2.pattern_.constant_ = isl::val::zero(ctx);
  PlaceholderSet<SingleInputDim, FixedOutDimPattern<SimpleAff>> ps;
  ps.placeholders_.push_back(p1);
  ps.placeholders_.push_back(p2);

  // Placeholders belong to the same fold.
  ps.placeholderFolds_.push_back(0);
  ps.placeholderFolds_.push_back(0);

  // Both placeholders must appear in the same relation.
  ps.placeholderGroups_.emplace_back();
  ps.placeholderGroups_.back().push_back(0);
  ps.placeholderGroups_.back().push_back(1);

  return ps;
}

TEST(AccessMatcher, FoldDiagonalAccess) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto ps = makeSameGroupSameFoldPlaceholderSet(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=i and b=i}");
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 1);
}

TEST(AccessMatcher, FoldNonDiagonalAccess) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto ps = makeSameGroupSameFoldPlaceholderSet(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=i and b=j}");
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 0);
}

TEST(AccessMatcher, FoldAcrossGroupsSame) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto ps = allOf(access(dim(0, 2 * _2), dim(1, _1)), access(dim(1, _1)));
  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=j and y=i}");
  auto matches = match(umap, ps);
  // Expect to have a match because b=i and y=i are properly folded.
  EXPECT_EQ(matches.size(), 1);
}

TEST(AccessMatcher, FoldAcrossGroupsDifferent) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto ps = makeTwoGroupPlaceholderSet(ctx);
  // Rewrite two-group placeholder set to have the same fold for p1 and p3.
  ps.placeholderFolds_[2] = 0;

  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=i and y=j}");
  auto matches = match(umap, ps);
  // Expect not to have a match because b=i and y=j are not properly folded.
  EXPECT_EQ(matches.size(), 0);
}

TEST(AccessMatcher, PlaceholderWithConstants) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=2*j+1 and b=i+42}");
  auto ps = allOf(access(dim(0, 2 * _1 + 1), dim(1, _2 + 42)));
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 1);
  umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=2*j+1 and b=i+43}");
  EXPECT_EQ(match(umap, ps).size(), 0);
}

TEST(AccessMatcher, PlaceholderWithConstantsNoMatch) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=2*j+1 and b=i+42}");
  auto ps = allOf(access(dim(0, 2 * _1 + 1), dim(1, _2)));
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 0);
}

TEST(AccessMatcher, Stencil) {
  using namespace matchers;

  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop = pet::Scop::parseFile(ctx, "inputs/stencil.c").getScop();
  ASSERT_FALSE(scop.schedule.is_null());

  // Don't want to include tree matchers in this _unit_ test, go to the first
  // leaf. This should go into integration tests.
  auto node =
      scop.schedule.get_root().child(0).child(0).child(0).child(0).child(0);
  auto schedule = node.get_prefix_schedule_union_map();
  auto reads = scop.reads.curry().apply_domain(schedule);
  auto writes = scop.mustWrites.curry().apply_domain(schedule);

  // Note that placeholders are _not_ reused between different calls to allOf.
  auto _1 = placeholder(ctx);
  auto psReads =
      allOf(access(dim(0, _1 - 1)), access(dim(0, _1)), access(dim(0, _1 + 1)));
  auto psWrites = allOf(access(dim(0, _1)));
  EXPECT_EQ(match(reads, psReads).size(), 1);
  EXPECT_EQ(match(writes, psWrites).size(), 1);
}

TEST(AccessMatcher, ThreeIdentical) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a,b]: a=i and b=j;"
                                  " [i,j]->B[a,b]: a=j and b=i;"
                                  " [i,j]->C[a,b]: a=i and b=j}");

  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto result = findAndReplace(umap, replace(access(_1, _2), access(_2, _1)));
  auto expected = isl::union_map(ctx, "{[i,j]->A[a,b]: a=j and b=i;"
                                      " [i,j]->B[a,b]: a=i and b=j;"
                                      " [i,j]->C[a,b]: a=j and b=i}");
  EXPECT_TRUE(result.is_equal(expected));
}

TEST(AccessMatcher, Strides) {
  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a,b]: a=42*i and b=j;"
                                  " [i,j]->B[a,b]: a=42*i and b=2*j}");
  EXPECT_EQ(match(umap, allOf(access(dim(1, stride(ctx, 1))))).size(), 1);
  EXPECT_EQ(match(umap, allOf(access(dim(1, stride(ctx, 2))))).size(), 1);
  // Stride is only implemented for the last input dim, here "j", so "a" in
  // outputs does not change with "j" and thus has stride 0.  Therefore, no
  // match is expected.
  EXPECT_EQ(match(umap, allOf(access(dim(0, stride(ctx, 42))))).size(), 0);

  // Here, on the contrary, "i" is the last input dimension that changes and
  // therefore both maps are expected to match.
  umap = isl::union_map(ctx, "{[j,i]->A[a,b]: a=42*i and b=j;"
                             " [j,i]->B[a,b]: a=42*i and b=2*j}");
  EXPECT_EQ(match(umap, allOf(access(dim(0, stride(ctx, 42))))).size(), 2);

  umap = isl::union_map(ctx, "[N,M] -> {[i,j,k]->A[a]: a=42*i+3*j+k+N}");
  EXPECT_EQ(match(umap, allOf(access(dim(0, stride(ctx, 1))))).size(), 1);
}

TEST(AccessMatcher, NegativeIndexMatch) {
  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a]: a=j;"
                                  " [i,j]->B[a]: a=i;"
                                  " [i,j]->C[a,b]: a=42*i and b=j;"
                                  " [i,j]->D[a,b]: a=42*i and b=2*j;"
                                  " [i,j]->E[a,b,e,f,g]: g=j;"
                                  " [i,j]->F[a,b,e,f,g]: g=i;"
                                  " [i,j]->G[a,b,e,f,g]: f=j}");
  EXPECT_EQ(match(umap, allOf(access(dim(-1, stride(ctx, 1))))).size(), 3);
  EXPECT_EQ(match(umap, allOf(access(dim(-1, stride(ctx, 2))))).size(), 1);
  EXPECT_EQ(match(umap, allOf(access(dim(-2, stride(ctx, 1))))).size(), 1);
}

TEST(AccessMatcher, NegativeIndexTransform) {
  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a]: a=j;"
                                  " [i,j]->B[a]: a=i;"
                                  " [i,j]->C[a,b]: a=42*i and b=j;"
                                  " [i,j]->D[a,b]: a=42*i and b=2*j;"
                                  " [i,j]->E[a,b,e,f,g]: f=42*i and g=j;"
                                  " [i,j]->F[a,b,e,f,g]: g=i;"
                                  " [i,j]->G[a,b,e,f,g]: f=j}");
  auto i = placeholder(ctx);
  auto j = placeholder(ctx);
  auto result =
      findAndReplace(umap, replace(access(dim(-2, 42 * i), dim(-1, j)),
                                   access(dim(-2, j), dim(-1, 42 * i))));

  EXPECT_EQ(match(result, allOf(access(dim(-1, 42 * placeholder(ctx))))).size(),
            2);
}

namespace {

class FuncStyleListPattern {
public:
  bool isFirst;
};

class FuncStyleListCandidate {
public:
  static std::vector<FuncStyleListCandidate>
  candidates(isl::map map, const FuncStyleListPattern &pattern) {
    auto dim = map.dim(isl::dim::out);
    if (dim == 0) {
      return {};
    }
    FuncStyleListCandidate candidate;
    if (pattern.isFirst) {
      candidate.partialMap = map.project_out(isl::dim::out, 0, 1);
    } else {
      candidate.partialMap = map.project_out(isl::dim::out, 1, dim - 1);
    }
    return {candidate};
  }

  static isl::map transformMap(isl::map map,
                               const FuncStyleListCandidate &candidate,
                               const FuncStyleListPattern &pattern) {
    if (pattern.isFirst) {
      auto id = map.get_tuple_id(isl::dim::out);
      return candidate.partialMap
          .flat_range_product(map.project_out(isl::dim::out, 0, 1))
          .set_tuple_id(isl::dim::out, id);
    } else {
      auto id = map.get_tuple_id(isl::dim::out);
      auto dim = map.dim(isl::dim::out);
      return map.project_out(isl::dim::out, 1, dim - 1)
          .flat_range_product(candidate.partialMap)
          .set_tuple_id(isl::dim::out, id);
    }
  }

  bool operator==(const FuncStyleListCandidate &) const { return false; }

  isl::map partialMap;
};

template <typename CandidateTy, typename PatternTy>
Placeholder<CandidateTy, PatternTy> makePlaceholder(PatternTy pattern) {
  return Placeholder<CandidateTy, PatternTy>(pattern);
}

auto head = []() {
  FuncStyleListPattern pattern;
  pattern.isFirst = true;
  return makePlaceholder<FuncStyleListCandidate>(pattern);
};

auto tail = []() {
  FuncStyleListPattern pattern;
  pattern.isFirst = false;
  return makePlaceholder<FuncStyleListCandidate>(pattern);
};
} // namespace

TEST(AccessMatcher, UserDefinedPatterns) {
  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a,b]: a=i and b=j;"
                                  " [i,j]->B[a,b,c,d]: a=j and b=i}");

  EXPECT_EQ(match(umap, allOf(access(head(), tail()))).size(), 2);
}

TEST(AccessMatcher, MultiDimensionalReplace) {
  auto ctx = ScopedCtx();
  auto umap = isl::union_map(ctx, "{[i,j]->A[a,b]: a=i and b=j;"
                                  " [i,j]->B[a,b,c,d]: a=j and b=i}");

  auto h = head();
  auto t = tail();
  umap = findAndReplace(umap, replace(access(h, t), access(t, h)));
  auto expected = isl::union_map(ctx, "{[i,j]->A[b,a]: a=i and b=j;"
                                      " [i,j]->B[b,c,d,a]: a=j and b=i}");
  EXPECT_TRUE(umap.is_equal(expected));
}

// Access strides may be caused by strides in the iteration domain.
// Check that, for strided domains, we can detect strides properly, given the
// information on the sparseness of the domain.
TEST(AccessMatcher, StrideInStridedDomain) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop = pet::Scop::parseFile(ctx, "inputs/strided_domain.c").getScop();
  // Since the input is a simple 1d loop, we can get the schedule without
  // auxiliary dimensions directly without traversing the tree.
  auto schedule = scop.schedule.get_map();
  auto nonEmptySchedulePoints = isl::set(scop.domain().apply(schedule));
  auto sHolder = stride(nonEmptySchedulePoints.get_ctx(), 3);
  auto reads = scop.reads.curry().apply_domain(schedule);

  // Expected a match when sparseness information is provided.
  sHolder.pattern_.nonEmptySchedulePoints = nonEmptySchedulePoints;
  EXPECT_EQ(match(reads, allOf(access(sHolder))).size(), 1);

  // Expected no match when sparseness information is not provided.
  sHolder.pattern_.nonEmptySchedulePoints = isl::set();
  EXPECT_EQ(match(reads, allOf(access(sHolder))).size(), 0);
}

TEST(AccessMatcher, StrideInStridedDomainWithMultiDimensionalAccess) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop =
      pet::Scop::parseFile(ctx, "inputs/strided_domain_multi_dimensions.c")
          .getScop();
  auto schedule = scop.schedule.get_map();
  auto reads = scop.reads.curry().apply_domain(schedule);
  auto writes = scop.mustWrites.curry().apply_domain(schedule);
  auto nonEmptySchedulePoints = isl::set(scop.domain().apply(schedule));
  // expect stride equals to 2
  auto sHolder = stride(nonEmptySchedulePoints.get_ctx(), 2);
  sHolder.pattern_.nonEmptySchedulePoints = nonEmptySchedulePoints;

  // Expect to match when sparseness information is provided.
  EXPECT_EQ(match(reads, allOf(access(dim(1, sHolder)))).size(), 1);

  // Expect to match when sparseness information is provided.
  EXPECT_EQ(match(writes, allOf(access(dim(1, sHolder)))).size(), 1);
  // Stride computation is done only for the innermost loop.
  EXPECT_EQ(match(writes, allOf(access(dim(0, sHolder)))).size(), 0);
}

TEST(AccessMatcher, StrideInStridedDomainWithDimensionCoefficients) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop =
      pet::Scop::parseFile(ctx, "inputs/strided_domain_with_coefficients.c")
          .getScop();
  auto schedule = scop.schedule.get_map();
  auto reads = scop.reads.curry().apply_domain(schedule);
  auto writes = scop.mustWrites.curry().apply_domain(schedule);
  auto nonEmptySchedulePoints = isl::set(scop.domain().apply(schedule));
  // Expect stride equals to 4
  auto sHolder = stride(nonEmptySchedulePoints.get_ctx(), 4);
  sHolder.pattern_.nonEmptySchedulePoints = nonEmptySchedulePoints;

  // Expect to match when sparseness information is provided.
  EXPECT_EQ(match(writes, allOf(access(sHolder))).size(), 1);
  // Expect stride equals to 6
  auto newStride = isl::val(nonEmptySchedulePoints.get_ctx(), 6);
  sHolder.pattern_.stride = newStride;
  EXPECT_EQ(match(reads, allOf(access(sHolder))).size(), 2);
  // Expect zero stride
  newStride = isl::val(nonEmptySchedulePoints.get_ctx(), 0);
  sHolder.pattern_.stride = newStride;
  EXPECT_EQ(match(reads, allOf(access(sHolder))).size(), 1);
}

static matchers::PlaceholderGroupedSet<SingleInputDim,
                                       FixedOutDimPattern<SimpleAff>>
makeTwoGroupsPlaceholderGroupedSet(isl::ctx ctx, bool sameArray) {
  using namespace matchers;

  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p1(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 0));
  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p2(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 1));
  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p3(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 0));
  Placeholder<SingleInputDim, FixedOutDimPattern<SimpleAff>> p4(
      FixedOutDimPattern<SimpleAff>(SimpleAff(ctx), 1));
  p1.pattern_.coefficient_ = isl::val(ctx, 1);
  p2.pattern_.coefficient_ = isl::val(ctx, 1);
  p3.pattern_.coefficient_ = isl::val(ctx, 1);
  p4.pattern_.coefficient_ = isl::val(ctx, 1);
  p1.pattern_.constant_ = isl::val::zero(ctx);
  p2.pattern_.constant_ = isl::val::zero(ctx);
  p3.pattern_.constant_ = isl::val::zero(ctx);
  p4.pattern_.constant_ = isl::val::zero(ctx);
  PlaceholderGroupedSet<SingleInputDim, FixedOutDimPattern<SimpleAff>> ps;
  ps.placeholders_.push_back(p1);
  ps.placeholders_.push_back(p2);
  ps.placeholders_.push_back(p3);
  ps.placeholders_.push_back(p4);

  // Placeholders belong to the different folds fold.
  ps.placeholderFolds_.push_back(0);
  ps.placeholderFolds_.push_back(1);
  ps.placeholderFolds_.push_back(0);
  ps.placeholderFolds_.push_back(1);

  // Pairs of placeholders must appear in the same relation.
  ps.placeholderGroups_.emplace_back();
  ps.placeholderGroups_.back().push_back(0);
  ps.placeholderGroups_.back().push_back(1);
  ps.placeholderGroups_.emplace_back();
  ps.placeholderGroups_.back().push_back(2);
  ps.placeholderGroups_.back().push_back(3);

  // Groups must match the same array (belong to the same group fold) if
  // "sameArray" is set and different arrays otherwise.
  ps.placeholderGroupFolds_.push_back(0);
  ps.placeholderGroupFolds_.push_back(sameArray ? 0 : 1);

  return ps;
}

TEST(AccessMatcher, GroupFolds) {
  auto ctx = ScopedCtx();
  auto umapSame = isl::union_map(ctx, "{[i,j]->[ref1[]->A[a,b]]: a=i and b=j;"
                                      " [i,j]->[ref2[]->A[a,b]]: a=i and b=j}");
  auto umapDiff = isl::union_map(ctx, "{[i,j]->[ref1[]->A[a,b]]: a=i and b=j;"
                                      " [i,j]->[ref2[]->B[a,b]]: a=i and b=j}");

  auto psSame = makeTwoGroupsPlaceholderGroupedSet(ctx, true);
  auto psDiff = makeTwoGroupsPlaceholderGroupedSet(ctx, false);
  // permutations are possible, so 2 matches
  EXPECT_EQ(match(umapSame, psSame).size(), 2);
  EXPECT_EQ(match(umapDiff, psSame).size(), 0);
  EXPECT_EQ(match(umapSame, psDiff).size(), 0);
  EXPECT_EQ(match(umapDiff, psDiff).size(), 2);
}

TEST(AccessMatcher, GroupFoldsAPI) {
  auto ctx = ScopedCtx();
  auto umapSame = isl::union_map(ctx, "{[i,j]->[ref1[]->A[a,b]]: a=i and b=j;"
                                      " [i,j]->[ref2[]->A[a,b]]: a=i and b=j}");
  auto umapDiff = isl::union_map(ctx, "{[i,j]->[ref1[]->A[a,b]]: a=i and b=j;"
                                      " [i,j]->[ref2[]->B[a,b]]: a=i and b=j}");

  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto Arr = arrayPlaceholder();
  auto Other = arrayPlaceholder();
  auto psSame = allOf(access(Arr, _i, _j), access(Arr, _i, _j));
  auto psDiff = allOf(access(Arr, _i, _j), access(Other, _i, _j));
  // permutations are possible, so 2 matches
  EXPECT_EQ(match(umapSame, psSame).size(), 2);
  EXPECT_EQ(match(umapDiff, psSame).size(), 0);
  EXPECT_EQ(match(umapSame, psDiff).size(), 0);
  EXPECT_EQ(match(umapDiff, psDiff).size(), 2);
}

