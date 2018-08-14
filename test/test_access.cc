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

  auto umap = isl::union_map(ctx, "{[i,j]->[a,b]: a=2*j and b=i}");

  EXPECT_EQ(match(umap, ps).size(), 1);

  isl_ctx_free(ctx.release());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
