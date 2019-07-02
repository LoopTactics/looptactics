#include "gtest/gtest.h"
#include "islutils/parser.h"

/*

Grammar 24 June 19

*****************************************
file: gemm.c

// more code
// ...

for (int i = 0; i < 1024; i++)
  for (int j = 0; j < 1024; j++)
    for (int k = 0; k < 1024; k++)
      C[i][j] += A[i][k] * B[k][j];

// more code
// ...
*****************************************

*****************************************
file: main.cc

1) the user describe the patter to detect

// pattern description.
std::string pattern = R"( C(i, j) += A (i, k) * B(k, j))";


2) the user create a tactic

// create tactic t
tactic t = {"gemm_tactic", pattern, "./path_to_file/gemm.c};
t.show();

output of show:


// more code
// ...
// gemm_tactic [created using band node annotation]
for (int i = 0; i < 1024; i++)
  for (int j = 0; j < 1024; j++)
    for (int k = 0; k < 1024; k++)
      C[i][j] += A[i][k] * B[k][j];

// more code
// ...

3) the user apply more transformations

auto loops = t.tile(32, 32, 32);
t.show()

output of show

// more code
// ...
// gemm_tactic
for (int i 
  for (int j 
    for (int k 
      for (ii
        for (jj
          for (kk
            C[ii][jj] += A[ii][kk] * B[kk][kj];

// more code
// ...


*/

TEST(Parser, testOne) {

  using namespace Parser;
  std::string S1 = "CB (ii, jj) += A(i,k) * C(k, j)";
  auto res = parse(S1); 
  EXPECT_TRUE(res.size() == 3);

}

