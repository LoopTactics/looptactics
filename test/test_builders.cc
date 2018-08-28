#include <islutils/builders.h>

#include <isl/cpp.h>

#include <iostream>

#include <gtest/gtest.h>

TEST(Builders, SimpleMatmul) {
  using namespace builders;
  auto ctx = isl::ctx(isl_ctx_alloc());
  auto iterationDomain = isl::union_set(
      ctx, "{S1[i,j]: 0 <= i,j < 10; S2[i,j,k]: 0 <= i,j,k < 42}");
  auto firstBand =
      isl::multi_union_pw_aff(ctx, "[{S1[i,j]->[(i)]; S2[i,j,k]->[(i)]}, "
                                   "{S1[i,j]->[(j)]; S2[i,j,k]->[(j)]}]");
  auto secondBand = isl::multi_union_pw_aff(ctx, "[{S2[i,j,k]->[(k)]}]");
  auto filterS1 = isl::union_set(ctx, "{S1[i,j]}");
  auto filterS2 = isl::union_set(ctx, "{S2[i,j]}");

  // clang-format off
  auto node =
      domain(iterationDomain,
        band(firstBand,
          sequence(
            filter(filterS2,
              band(secondBand)),
            filter(filterS1)))).build();
  // clang-format on

  ctx.release();
}

TEST(Builders, ExtensionAlone) {
  using namespace builders;
  auto ctx = isl::ctx(isl_ctx_alloc());
  auto iterationDomain = isl::union_set(ctx, "{S1[i]: 0 <= i < 42}");
  auto extensionMap = isl::union_map(ctx, "{[]->test[]:}");

  // clang-format off
  auto node = domain(iterationDomain,
                extension(extensionMap)).build();
  // clang-format on

  auto m = isl::map::from_union_map(node.child(0).extension_get_extension());
  EXPECT_EQ(m.get_tuple_name(isl::dim::out), "test");

  ctx.release();
}

// Insert a set instead of a more traditional sequence below the extension
// node.
TEST(Builders, ExtensionSetMixed) {
  using namespace builders;
  auto ctx = isl::ctx(isl_ctx_alloc());
  auto iterationDomain = isl::union_set(ctx, "{S1[i]: 0 <= i < 42}");
  auto extensionMap = isl::union_map(ctx, "{[]->test[]:}");
  auto filterS1 = isl::union_set(ctx, "{S1[i]}");
  auto filterTest = isl::union_set(ctx, "{test[]}");

  // clang-format off
  auto node = domain(iterationDomain,
                extension(extensionMap,
                  set(
                    filter(filterS1),
                    filter(filterTest)))).build();
  // clang-format on

  EXPECT_EQ(isl_schedule_node_get_type(node.child(0).child(0).get()),
            isl_schedule_node_set);
  EXPECT_EQ(node.child(0).child(0).n_children(), 2);

  ctx.release();
}
