#include "islutils/access.h"

#include "gtest/gtest.h"

static matchers::PlaceholderSet makePlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder p1, p2;
  p1.coefficient_ = isl::val(ctx, 1);
  p2.coefficient_ = isl::val(ctx, 2);
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

  auto ctx = isl::ctx(isl_ctx_alloc());
  auto ps = makePlaceholderSet(ctx);
  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=2*j and y=i}");

  // There are 2 possible matches: the first and the second map of the union.
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 2);

  isl_ctx_free(ctx.release());
}

static matchers::PlaceholderSet makeTwoGroupPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  auto ps = makePlaceholderSet(ctx);

  // Make this similar to p1.
  Placeholder p3;
  p3.coefficient_ = isl::val(ctx, 1);
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

  auto ctx = isl::ctx(isl_ctx_alloc());
  auto ps = makeTwoGroupPlaceholderSet(ctx);
  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=42*j and y=i}");
  auto matches = match(umap, ps);
  // Only one match possible: anonymous space to p1,p2, "A" space to p3.
  // Because p3 and p1 belong to different groups, they cannot both match the
  // anonymous space.
  // Because p1 and p2 belong to the same group, only the anonymous space can
  // match them (p2 does not match the "A" space).
  EXPECT_EQ(matches.size(), 1);

  isl_ctx_free(ctx.release());
}

TEST(AccessMatcher, TwoMapsOneMatch) {
  using namespace matchers;

  auto ctx = isl::ctx(isl_ctx_alloc());
  auto ps = makePlaceholderSet(ctx);

  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=j and y=i}");
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 1);

  isl_ctx_free(ctx.release());
}

static matchers::PlaceholderSet
makeSameGroupSameFoldPlaceholderSet(isl::ctx ctx) {
  using namespace matchers;

  Placeholder p1, p2;
  p1.coefficient_ = isl::val(ctx, 1);
  p2.coefficient_ = isl::val(ctx, 1);
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

  auto ctx = isl::ctx(isl_ctx_alloc());
  auto ps = makeSameGroupSameFoldPlaceholderSet(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=i and b=i}");
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 1);

  isl_ctx_free(ctx.release());
}

TEST(AccessMatcher, FoldNonDiagonalAccess) {
  using namespace matchers;

  auto ctx = isl::ctx(isl_ctx_alloc());
  auto ps = makeSameGroupSameFoldPlaceholderSet(ctx);
  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=i and b=j}");
  auto matches = match(umap, ps);
  EXPECT_EQ(matches.size(), 0);

  isl_ctx_free(ctx.release());
}

struct NamedPlaceholder {
  matchers::Placeholder p;
  std::string name;
};

NamedPlaceholder placeholder(isl::ctx ctx, const std::string n) {
  NamedPlaceholder result;
  result.p.coefficient_ = isl::val::one(ctx);
  result.p.outDimPos_ = -1;
  result.name = n;
  return result;
}

NamedPlaceholder placeholder(isl::ctx ctx) {
  static thread_local size_t counter = 0;
  return placeholder(ctx, std::to_string(counter++));
}

static NamedPlaceholder dim(int pos, NamedPlaceholder np) {
  np.p.outDimPos_ = pos;
  return np;
}

NamedPlaceholder operator*(int i, NamedPlaceholder np) {
  // FIXME: assuming val is always initialized...
  np.p.coefficient_ =
      np.p.coefficient_.mul(isl::val(np.p.coefficient_.get_ctx(), i));
  return np;
}

using NamedPlaceholderList = std::vector<NamedPlaceholder>;

template <typename... Args> static NamedPlaceholderList access(Args... args) {
  static_assert(std::is_same<typename std::common_type<Args...>::type,
                             NamedPlaceholder>::value,
                "accesses can only be constructed from named placeholders");

  return {args...};
}

template <typename... Args>
static matchers::PlaceholderSet makePS(Args... args) {
  static_assert(
      std::is_same<typename std::common_type<Args...>::type,
                   NamedPlaceholderList>::value,
      "can only make PlaceholderSet from lists of named placeholders");

  using namespace matchers;

  std::vector<NamedPlaceholderList> placeholderLists = {args...};
  std::vector<std::pair<std::string, size_t>> knownNames;
  PlaceholderSet ps;
  for (const auto &npl : placeholderLists) {
    if (npl.empty()) {
      continue;
    }

    size_t index = ps.placeholders_.size();
    ps.placeholderGroups_.emplace_back();
    for (const auto &np : npl) {
      ps.placeholders_.push_back(np.p);
      ps.placeholderGroups_.back().push_back(index);
      auto namePos =
          std::find_if(knownNames.begin(), knownNames.end(),
                       [np](const std::pair<std::string, size_t> &pair) {
                         return pair.first == np.name;
                       });
      if (namePos == knownNames.end()) {
        knownNames.emplace_back(np.name, index);
        ps.placeholderFolds_.emplace_back(index);
      } else {
        ps.placeholderFolds_.emplace_back(namePos->second);
      }
      ++index;
    }
  }

  return ps;
}

TEST(AccessMatcher, FoldAcrossGroupsSame) {
  using namespace matchers;

  auto ctx = isl::ctx(isl_ctx_alloc());
  auto _1 = placeholder(ctx);
  auto _2 = placeholder(ctx);
  auto ps = makePS(access(dim(0, 2 * _2), dim(1, _1)), access(dim(1, _1)));
  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=j and y=i}");
  auto matches = match(umap, ps);
  // Expect to have a match because b=i and y=i are properly folded.
  EXPECT_EQ(matches.size(), 1);

  isl_ctx_free(ctx.release());
}

TEST(AccessMatcher, FoldAcrossGroupsDifferent) {
  using namespace matchers;

  auto ctx = isl::ctx(isl_ctx_alloc());
  auto ps = makeTwoGroupPlaceholderSet(ctx);
  // Rewrite two-group placeholder set to have the same fold for p1 and p3.
  ps.placeholderFolds_[2] = 0;

  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=i and y=j}");
  auto matches = match(umap, ps);
  // Expect not to have a match because b=i and y=j are not properly folded.
  EXPECT_EQ(matches.size(), 0);

  isl_ctx_free(ctx.release());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
