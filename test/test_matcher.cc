#include <islutils/matchers.h>
#include <islutils/parser.h>

#include "gtest/gtest.h"

TEST(TreeMatcher, ReadFromFile) { 
  Scop S = Parser("inputs/one-dimensional-init.c").getScop();
  EXPECT_TRUE(!S.schedule.is_null());
}

TEST(TreeMatcher, CompileTest) { 
  using namespace matchers;

  auto matcher = domain(context(sequence(band(), band(), filter())));
  auto m2 = sequence();
  auto m3 = sequence(filter(), filter());
  auto m4 = sequence([](isl::schedule_node n) { return true; });
  auto m5 = sequence([](isl::schedule_node n) { return true; }, filter(), filter());

  auto m6 = sequence(filter(hasNextSibling(filter())));
  auto m7 = sequence(filter(hasNextSibling(filter(hasPreviousSibling(filter())))));
  auto m8 = sequence(filter(hasSibling(filter())));

  auto m9 = sequence(hasDescendant(band()));
  auto m10 = band(leaf());
  auto m11 = band([](isl::schedule_node n) { return true;}, leaf());
}
 
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

