#include <islutils/builders.h>

#include <isl/cpp.h>

#include <iostream>

int main() {
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

  isl_schedule_node_dump(node.get());

  ctx.release();
  return 0;
}
