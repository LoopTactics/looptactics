#include "islutils/access.h"

#include "gtest/gtest.h"

TEST(AccessMatcher, Simple) {
  using namespace matchers;

  auto ctx = isl::ctx(isl_ctx_alloc());

  Placeholder p1, p2;
  p1.coefficient_ = isl::val(ctx, 1);
  p2.coefficient_ = isl::val(ctx, 2);
  p1.outDimPos_ = 1;
  p2.outDimPos_ = 0;
  PlaceholderSet ps;
  ps.placeholders_.push_back(p1);
  ps.placeholders_.push_back(p2);

  auto umap = isl::union_map(
      ctx, "{[i,j]->[a,b]: a=2*j and b=i; [i,j]->A[x,y]: x=2*j and y=i}");

  // There are 4 matches because we don't force placeholders to be in the same
  // relation (yet).  So we can get p1 matching b from the first relation and
  // p2 matching x from the second relation as a match.
  // TODO: don't consider those as matches.
  EXPECT_EQ(match(umap, ps).size(), 4);

  isl_ctx_free(ctx.release());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
