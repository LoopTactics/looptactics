#include "islutils/access.h"
#include "islutils/ctx.h"
#include "islutils/locus.h"
#include "islutils/parser.h"

#include "gtest/gtest.h"

using util::ScopedCtx;
using namespace matchers;

static matchers::PlaceholderSet<SingleInputDim>
makePlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder<SingleInputDim> p1(ctx, 1);
  Placeholder<SingleInputDim> p2(ctx, 0);
  p1.coefficient_ = isl::val(ctx, 1);
  p2.coefficient_ = isl::val(ctx, 2);
  p1.constant_ = isl::val::zero(ctx);
  p2.constant_ = isl::val::zero(ctx);
  PlaceholderSet<SingleInputDim> ps;
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

  // Check that we can insect the result using placeholder objects.
  for (const auto &m : matches) {
    EXPECT_FALSE(isl::union_map(m[_1].candidateMap_).is_empty());
    EXPECT_TRUE(isl::union_map(m[_1].candidateMap_).is_subset(umap));
    EXPECT_FALSE(isl::union_map(m[_2].candidateMap_).is_empty());
    EXPECT_TRUE(isl::union_map(m[_2].candidateMap_).is_subset(umap));

    // Check that we got the matching right.
    EXPECT_TRUE(
        (m[_1].payload_.inputDimPos_ == 0 && m[_2].payload_.inputDimPos_ == 1) ^
        (m[_1].payload_.inputDimPos_ == 1 && m[_2].payload_.inputDimPos_ == 0));
  }
}

static matchers::PlaceholderSet<SingleInputDim>
makeTwoGroupPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  auto ps = makePlaceholderSet(ctx);

  // Make this similar to p1.
  Placeholder<SingleInputDim> p3(ctx, 1);
  p3.coefficient_ = isl::val(ctx, 1);
  p3.constant_ = isl::val::zero(ctx);
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
  auto ps = makeTwoGroupPlaceholderSet(ctx);
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

static matchers::PlaceholderSet<SingleInputDim>
makeSameGroupSameFoldPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder<SingleInputDim> p1(ctx, 1);
  Placeholder<SingleInputDim> p2(ctx, 0);
  p1.coefficient_ = isl::val(ctx, 1);
  p2.coefficient_ = isl::val(ctx, 1);
  p1.constant_ = isl::val::zero(ctx);
  p2.constant_ = isl::val::zero(ctx);
  PlaceholderSet<SingleInputDim> ps;
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

  auto ctx = ScopedCtx();
  auto scop = Parser("inputs/stencil.c").getScop();
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
  auto psReads = allOf(access(dim(0, _1 + (-1))), access(dim(0, _1)),
                       access(dim(0, _1 + 1)));
  auto psWrites = allOf(access(dim(0, _1)));
  EXPECT_EQ(match(reads, psReads).size(), 1);
  EXPECT_EQ(match(writes, psWrites).size(), 1);
}

template <typename CandidatePayload> struct Replacement {
  Replacement(PlaceholderList<CandidatePayload> &&pattern_,
              PlaceholderList<CandidatePayload> &&replacement_)
      : pattern(pattern_), replacement(replacement_) {}

  PlaceholderList<CandidatePayload> pattern;
  PlaceholderList<CandidatePayload> replacement;
};

template <typename CandidatePayload>
Replacement<CandidatePayload>
replace(PlaceholderList<CandidatePayload> &&pattern,
        PlaceholderList<CandidatePayload> &&replacement) {
  return {std::move(pattern), std::move(replacement)};
}

// Calls like this do not fully make sense: different replacements for
// essentially the same pattern.
// makePSR(replace(access(_1, _2), access(_2, _1)),
//         replace(access(_3, _4), access(_3, _4)))
// They could become useful if access is further constrained to specific arrays
// or statements/schedule points.
//
// Calls like this contain redundant information and should be disallowed.
// makePSR(replace(access(_1, _2), access(_2, _1)),
//         replace(access(_1, _2), access(_2, _1)))
//
// Generally, we should disallow such transformations that affect the same
// relation more than once during the same call to transform.
// For example, access(_1, *), access(_1, _2).
//
// As the first approximation, marking this as undefined behavior and ignoring.

static std::vector<isl::map> listOf1DMaps(isl::map map) {
  std::vector<isl::map> result;
  for (int dim = map.dim(isl::dim::out); dim > 0; --dim) {
    result.push_back(map.project_out(isl::dim::out, 1, dim - 1));
    map = map.project_out(isl::dim::out, 0, 1);
  }
  return result;
}

static inline isl::space addEmptyRange(isl::space space) {
  return space.product(space.params().set_from_params()).unwrap();
}

static isl::map mapFrom1DMaps(isl::space space,
                              const std::vector<isl::map> &list) {
  auto zeroSpace = addEmptyRange(space.domain());
  auto result = isl::map::universe(zeroSpace);
  for (const auto &m : list) {
    result = result.flat_range_product(m);
  }
  result =
      result.set_tuple_id(isl::dim::out, space.get_tuple_id(isl::dim::out));
  return result;
}

static isl::map
make1DMap(const DimCandidate<SingleInputDim> &dimCandidate,
          const UnpositionedPlaceholder<SingleInputDim> &placeholder,
          isl::space space) {
  auto lhs =
      isl::aff::var_on_domain(isl::local_space(space.domain()), isl::dim::set,
                              dimCandidate.payload_.inputDimPos_);
  lhs = lhs.scale(placeholder.coefficient_)
            .add_constant_val(placeholder.constant_);
  auto rhs = isl::aff::var_on_domain(isl::local_space(space.range()),
                                     isl::dim::set, 0);
  using map_maker::operator==;
  return lhs == rhs;
}

template <typename CandidatePayload, typename... Args>
static isl::map
transformOneMap(isl::map map, const Match<CandidatePayload> &oneMatch,
                Replacement<CandidatePayload> arg, Args... args) {
  static_assert(
      std::is_same<typename std::common_type<Replacement<CandidatePayload>,
                                             Args...>::type,
                   Replacement<CandidatePayload>>::value,
      "");

  isl::map result;
  for (const auto &rep : {arg, args...}) {
    // separability of matches is important!
    // if we match here something that we would not have matched with the whole
    // set, it's bad!  But here we know that the map has already matched with
    // one of the groups in the set, we just don't know which one.  If it
    // matches two groups, this means the transformation would happen twice,
    // which we expicitly disallow.
    if (match(isl::union_map(map), allOf(rep.pattern)).empty()) {
      continue;
    }
    if (!result.is_null()) {
      ISLUTILS_DIE("one relation matched multiple patterns\n"
                   "the transformation is undefined");
    }
    // Actual transformation.
    if (map.dim(isl::dim::out) == 0) {
      result = map;
      continue;
    }
    auto list = listOf1DMaps(map);
    auto space1D =
        addEmptyRange(map.get_space().domain()).add_dims(isl::dim::out, 1);
    for (const auto &plh : rep.replacement) {
      list[plh.outDimPos_] = make1DMap(oneMatch[plh], plh, space1D);
    }
    result = mapFrom1DMaps(map.get_space(), list);
  }
  return result;
}

template <typename CandidatePayload, typename... Args>
isl::union_map findAndReplace(isl::union_map umap,
                              Replacement<CandidatePayload> arg, Args... args) {
  static_assert(
      std::is_same<typename std::common_type<Replacement<CandidatePayload>,
                                             Args...>::type,
                   Replacement<CandidatePayload>>::value,
      "");

  // make a vector of maps
  // for each match,
  //   find all corresponding maps,
  //     if not found, the map was deleted already meaning there was an attempt
  //     of double transformation
  //   remove them from vector,
  //   transform them and
  //   add them to the resulting vector
  // finally, copy all the remaining original maps as is into result

  std::vector<isl::map> originalMaps, transformedMaps;
  umap.foreach_map([&originalMaps](isl::map m) { originalMaps.push_back(m); });

  auto getPattern = [](const Replacement<CandidatePayload> &replacement) {
    return replacement.pattern;
  };

  auto ps = allOf(getPattern(arg), getPattern(args)...);
  auto matches = match(umap, ps);

  for (const auto &m : matches) {
    std::vector<isl::map> toTransform;
    for (const auto &plh : ps.placeholders_) {
      auto candidate = m[plh].candidateMap_;
      auto found = std::find_if(
          toTransform.begin(), toTransform.end(),
          [candidate](isl::map map) { return map.is_equal(candidate); });
      if (found != toTransform.end()) {
        continue;
      }
      toTransform.push_back(candidate);
    }

    for (const auto &candidate : toTransform) {
      auto found = std::find_if(
          originalMaps.begin(), originalMaps.end(),
          [candidate](isl::map map) { return map.is_equal(candidate); });
      if (found == originalMaps.end()) {
        ISLUTILS_DIE("could not find the matched map\n"
                     "this typically means a map was matched more than once\n"
                     "in which case the transformation is undefined");
        continue;
      }
      originalMaps.erase(found);

      auto r = transformOneMap<CandidatePayload>(candidate, m, arg, args...);
      transformedMaps.push_back(r);
    }
  }

  for (const auto &map : originalMaps) {
    transformedMaps.push_back(map);
  }

  if (transformedMaps.empty()) {
    return isl::union_map::empty(umap.get_space());
  }
  auto result = isl::union_map(transformedMaps.front());
  for (const auto &map : transformedMaps) {
    result = result.unite(isl::union_map(map));
  }
  return result;
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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
