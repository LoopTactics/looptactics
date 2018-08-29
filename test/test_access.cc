#include "islutils/access.h"
#include "islutils/ctx.h"
#include "islutils/parser.h"

#include "gtest/gtest.h"

using util::ScopedCtx;
using namespace matchers;

static Placeholder dim(int pos, Placeholder p) {
  p.outDimPos_ = pos;
  return p;
}

Placeholder operator*(int i, Placeholder p) {
  p.coefficient_ = p.coefficient_.mul(isl::val(p.coefficient_.get_ctx(), i));
  return p;
}

Placeholder operator+(Placeholder p, int i) {
  p.constant_ = p.constant_.add(isl::val(p.constant_.get_ctx(), i));
  return p;
}

using PlaceholderList = std::vector<Placeholder>;

template <typename... Args> static PlaceholderList access(Args... args) {
  static_assert(std::is_same<typename std::common_type<Args...>::type,
                             Placeholder>::value,
                "accesses can only be constructed from named placeholders");

  return {args...};
}

template <typename... Args>
static matchers::PlaceholderSet makePS(Args... args) {
  static_assert(
      std::is_same<typename std::common_type<Args...>::type,
                   PlaceholderList>::value,
      "can only make PlaceholderSet from lists of named placeholders");

  using namespace matchers;

  std::vector<PlaceholderList> placeholderLists = {args...};
  std::vector<std::pair<size_t, size_t>> knownIds;
  PlaceholderSet ps;
  for (const auto &pl : placeholderLists) {
    if (pl.empty()) {
      continue;
    }

    size_t index = ps.placeholders_.size();
    ps.placeholderGroups_.emplace_back();
    for (const auto &p : pl) {
      ps.placeholders_.push_back(p);
      ps.placeholderGroups_.back().push_back(index);
      auto namePos = std::find_if(knownIds.begin(), knownIds.end(),
                                  [p](const std::pair<size_t, size_t> &pair) {
                                    return pair.first == p.id_;
                                  });
      if (namePos == knownIds.end()) {
        knownIds.emplace_back(p.id_, index);
        ps.placeholderFolds_.emplace_back(index);
      } else {
        ps.placeholderFolds_.emplace_back(namePos->second);
      }
      ++index;
    }
  }

  return ps;
}

static matchers::PlaceholderSet makePlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder p1(ctx);
  Placeholder p2(ctx);
  p1.coefficient_ = isl::val(ctx, 1);
  p2.coefficient_ = isl::val(ctx, 2);
  p1.constant_ = isl::val::zero(ctx);
  p2.constant_ = isl::val::zero(ctx);
  p1.outDimPos_ = 1;
  p2.outDimPos_ = 0;
  PlaceholderSet ps;
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

static matchers::PlaceholderSet makeTwoGroupPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  auto ps = makePlaceholderSet(ctx);

  // Make this similar to p1.
  Placeholder p3(ctx);
  p3.coefficient_ = isl::val(ctx, 1);
  p3.constant_ = isl::val::zero(ctx);
  p3.outDimPos_ = 1;
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
  auto _1 = Placeholder(ctx);
  auto _2 = Placeholder(ctx);
  auto matches = match(
      umap, makePS(access(dim(0, 2 * _2), dim(1, _1)), access(dim(1, _1))));
  // Only one match possible: anonymous space to p1,p2, "A" space to p3.
  // Because p3 and p1 belong to different groups, they cannot both match the
  // anonymous space.
  // Because p1 and p2 belong to the same group, only the anonymous space can
  // match them (p2 does not match the "A" space).
  EXPECT_EQ(matches.size(), 1);

  auto _3 = Placeholder(ctx);
  matches = match(
      umap, makePS(access(dim(0, 2 * _2), dim(1, _1)), access(dim(1, _3))));
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

static matchers::PlaceholderSet
makeSameGroupSameFoldPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder p1(ctx);
  Placeholder p2(ctx);
  p1.coefficient_ = isl::val(ctx, 1);
  p2.coefficient_ = isl::val(ctx, 1);
  p1.constant_ = isl::val::zero(ctx);
  p2.constant_ = isl::val::zero(ctx);
  p1.outDimPos_ = 1;
  p2.outDimPos_ = 0;
  PlaceholderSet ps;
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
  auto _1 = Placeholder(ctx);
  auto _2 = Placeholder(ctx);
  auto ps = makePS(access(dim(0, 2 * _2), dim(1, _1)), access(dim(1, _1)));
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
  auto _1 = Placeholder(ctx);
  auto _2 = Placeholder(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=2*j+1 and b=i+42}");
  auto ps = makePS(access(dim(0, 2 * _1 + 1), dim(1, _2 + 42)));
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 1);
  umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=2*j+1 and b=i+43}");
  EXPECT_EQ(match(umap, ps).size(), 0);
}

TEST(AccessMatcher, PlaceholderWithConstantsNoMatch) {
  using namespace matchers;

  auto ctx = ScopedCtx();
  auto _1 = Placeholder(ctx);
  auto _2 = Placeholder(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=2*j+1 and b=i+42}");
  auto ps = makePS(access(dim(0, 2 * _1 + 1), dim(1, _2)));
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

  // Note that placeholders are _not_ reused between different calls to makePS.
  auto _1 = Placeholder(ctx);
  auto psReads = makePS(access(dim(0, _1 + (-1))), access(dim(0, _1)),
                        access(dim(0, _1 + 1)));
  auto psWrites = makePS(access(dim(0, _1)));
  EXPECT_EQ(match(reads, psReads).size(), 1);
  EXPECT_EQ(match(writes, psWrites).size(), 1);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
