#include "gtest/gtest.h"
#include <islutils/ctx.h>
#include <islutils/pet_wrapper.h>


// The function parseFile gives:
// isl_ctx freed, but some objects still reference it
// why???

TEST(alloc, ctx_alloc) {

  // why this does not work
  using namespace util;
  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop = pet::Scop::parseFile(
    ScopedCtx(), "inputs/nested.c").getScop();
}

TEST(alloc, ctx_alloc_working) {

  // why this does not work
  using namespace util;
  ScopedCtx ctx = ScopedCtx(pet::allocCtx());
  auto scop = pet::Scop::parseFile(
    ctx, "inputs/nested.c").getScop();
}
