#include <islutils/locus.h>

#include "gtest/gtest.h"

TEST(SetMaker, Equality) {
  isl::ctx ctx = isl_ctx_alloc();
  {
    isl::aff a1(ctx, "{[i] -> [(1)]})");
    isl::aff a2(ctx, "{[i] -> [(1)]})");
    isl::map m(ctx, "{[j] -> [i]})");
    using map_maker::operator==;
    EXPECT_TRUE((a1 == a2).is_equal(m));
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
