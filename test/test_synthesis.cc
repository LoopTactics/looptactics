#include "gtest/gtest.h"
#include <islutils/access.h>
#include <islutils/matchers.h>
#include <islutils/pet_wrapper.h>
#include <islutils/ctx.h>
#include <iostream>
#include <islutils/access_patterns.h>

TEST(Synthesis, test_zero) {

  using namespace util; 
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/1mmWithoutInitStmt.c");
  auto scop = petScop.getScop();

  isl::union_map reads = scop.reads.curry();
  isl::union_map writes = scop.mustWrites.curry();

  using namespace matchers;
  typedef Placeholder<SingleInputDim,UnfixedOutDimPattern<SimpleAff>> Placeholder;

  struct PlaceholderSet {
    Placeholder p;
    std::string id;
  };
  struct ArrayPlaceholderSet {
    ArrayPlaceholder ap;
    std::string id;
  };

  std::vector<PlaceholderSet> vectorPlaceholderSet = {};
  std::vector<ArrayPlaceholderSet> vectorArrayPlaceholderSet = {};
  
  std::string id = "_i";
  for (size_t i = 0; i < 3; i++) {
    PlaceholderSet tmp = {placeholder(ctx), id+std::to_string(i)}; 
    vectorPlaceholderSet.push_back(tmp);
  }

  id = "_C";
  for (size_t i = 0; i < 3; i++) {
    ArrayPlaceholderSet tmp = {arrayPlaceholder(), id+std::to_string(i)};
    vectorArrayPlaceholderSet.push_back(tmp);
  }

  typedef ArrayPlaceholderList<SingleInputDim, FixedOutDimPattern<SimpleAff>> Access;
  std::vector<Access> accessList = {};

  accessList.push_back(access(vectorArrayPlaceholderSet[0].ap,  // _C0 
                              vectorPlaceholderSet[0].p,        // _i0 -> i
                              vectorPlaceholderSet[1].p));      // _i1 -> j
  accessList.push_back(access(vectorArrayPlaceholderSet[1].ap,
                              vectorPlaceholderSet[0].p,
                              vectorPlaceholderSet[2].p));
  accessList.push_back(access(vectorArrayPlaceholderSet[2].ap,
                              vectorPlaceholderSet[2].p,
                              vectorPlaceholderSet[1].p));
  auto psRead = allOf(accessList);
  auto readMatches = match(reads, psRead);

  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto _C = arrayPlaceholder();
  auto _B = arrayPlaceholder();
  auto _A = arrayPlaceholder();

  auto psReadOrig = 
    allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
  auto readMatchesOrig = match(reads, psReadOrig);
  
  EXPECT_EQ(readMatches[0][vectorPlaceholderSet[0].p].payload().inputDimPos_,
            readMatchesOrig[0][_i].payload().inputDimPos_);
  EXPECT_EQ(readMatches[0][vectorPlaceholderSet[1].p].payload().inputDimPos_,
            readMatchesOrig[0][_j].payload().inputDimPos_);
  EXPECT_EQ(readMatches[0][vectorPlaceholderSet[2].p].payload().inputDimPos_,
            readMatchesOrig[0][_k].payload().inputDimPos_);


}
