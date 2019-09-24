#include "gtest/gtest.h"
#include "islutils/parser.h"

std::ostream &operator<<(std::ostream &os,
                         const std::vector<Parser::AccessDescriptor> &a) {
  std::cout << "{\n";
  for (const auto& sa : a) {
    std::cout << sa.array_name_ << std::endl;
    for (const auto &ac : sa.affine_access_) {
      std::cout << "induction var : " << ac.induction_var_name_ << "\n";
      std::cout << "increment : " << ac.increment_ << "\n";
      std::cout << "coefficient : " << ac.coefficient_ << "\n";
      if (ac.inc_type_ == Parser::Increment_type::PLUS) 
        std::cout << "PLUS\n";
      else std::cout << "MINUS\n";
    }
  }
  if (a.size() == 0)
    std::cout << "empty\n";
  std::cout << "} \n";
  return os;
}

TEST(Parser, testOne) {

  using namespace Parser;
  std::string S = "CB (ii, jj) += A(i,k) * C(k, j)";
  auto res = parse(S); 
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 3);
}

TEST(Parser, testTwo) {
  
  using namespace Parser;
  std::string S = "CB          (ii, j) += A(i, k)*C(k,j)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 3);
}

TEST(Parser, testThree) {

  using namespace Parser;
  std::string S = "(ii, jj) += A(i,k) * C(k, j)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testFour) {

  using namespace Parser; 
  std::string S = "AA(ii   ";
  auto res = parse(S);
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testFive) {
  
  using namespace Parser;
  std::string S = "A(ii )";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
}

TEST(Parser, testSix) {

  using namespace Parser;
  std::string S = "A(ii + 6)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
}

TEST(Parser, testSeven) {

  using namespace Parser;
  std::string S = "CB (ii+1, j + 2) += A(i, k)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 2);
}

TEST(Parser, testEight) {

  using namespace Parser;
  std::string S = "CB (ii-1, j + 2) += A(i, k)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 2);
}

TEST(Parser, testNine) {

  using namespace Parser;
  std::string S = "CB (ii*1, j + 2) += A(i, k)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTen) {

  using namespace Parser;
  std::string S = "CB (ii+1 j + 2) += A(i, k)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testEleven) {

  using namespace Parser;
  std::string S = "CB (ii + j) = A(i, j)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTwelve) {

  using namespace Parser;
  std::string S = "CB (ii + 1 + j) = A(i, j)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testThirteen) {

  using namespace Parser;
  std::string S = "CB (i + 2 +1) = A(i,j)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 2);
}

TEST(Parser, testFourteen) {

  using namespace Parser;
  std::string S = "CB (+1)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testFifteen) {

  using namespace Parser;
  std::string S = "CB (1 + i)";
  auto res = parse(S);
  std::cout << res << std::endl;  
  EXPECT_TRUE(res.size() == 1);
}

TEST(Parser, testSixteen) {

  using namespace Parser;
  std::string S = "CB (1 + 1 + i) += A(i, j)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 2);
}

TEST(Parser, testSeventheen) {

  using namespace Parser;
  std::string S = "CB (1 - 1 + i) = A(i,j)";  
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 2);
}

TEST(Parser, testEighteen) {
  
  using namespace Parser;
  std::string S = "CB (1 + i + 2)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
}

TEST(Parser, testNineteen) {
  
  using namespace Parser;
  std::string S = "CB (1 + 3 + 2 ";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTwenty) {
  
  using namespace Parser;
  std::string S = "CB (1 + 3 + 2 )";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTwentyOne) {
  
  using namespace Parser;
  std::string S = "CB (1 + 3 + 2i )";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
}

TEST(Parser, testTwentyTwo) {

  using namespace Parser;
  std::string S = "CB(i+j)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTwentyThree) {

  using namespace Parser;
  std::string S = "CB A(i+1)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTwentyFour) {

  using namespace Parser;
  std::string S = "CB ( A(i + 1)";  
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTwentyFive) {
  
  using namespace Parser;
  std::string S = "B(i) = A(i-1) + A(i) + A(i+1)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 4);
  EXPECT_TRUE(res[0].array_name_ == "B");
  EXPECT_TRUE(res[1].array_name_ == "A");
  EXPECT_TRUE(res[2].array_name_ == "A");
  EXPECT_TRUE(res[3].array_name_ == "A");
  EXPECT_TRUE(res[1].affine_access_[0].inc_type_ == Increment_type::MINUS);
}

TEST(Parser, testTwentySix) {
  
  using namespace Parser;
  std::string S = "B(i) = A(i-4) + A(i-0) + A(i+4)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 4);
  EXPECT_TRUE(res[0].array_name_ == "B");
  EXPECT_TRUE(res[1].array_name_ == "A");
  EXPECT_TRUE(res[2].array_name_ == "A");
  EXPECT_TRUE(res[3].array_name_ == "A");
  EXPECT_TRUE(res[1].affine_access_[0].inc_type_ == Increment_type::MINUS);
}

TEST(Parser, testTwentySeven) {
  
  using namespace Parser;
  std::string S = "B(i) = A(i-4) + A(i-(0)) + A(i+4)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testTwentyEigth) {

  using namespace Parser;
  std::string S = "B(2*i + 4)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
  EXPECT_TRUE(res[0].affine_access_[0].coefficient_ == 2);
  EXPECT_TRUE(res[0].affine_access_[0].increment_ == 4);
}

TEST(Parser, testTwentyNine) {

  using namespace Parser;
  std::string S = "B(4 + 2*i)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
  EXPECT_TRUE(res[0].affine_access_[0].coefficient_ == 2);
  EXPECT_TRUE(res[0].affine_access_[0].increment_ == 4);
}

TEST(Parser, testThirty) {

  using namespace Parser;
  std::string S = "V(4 + -2*i)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}

TEST(Parser, testThirtyOne) {

  using namespace Parser;
  std::string S = "B(4 + 8 + 2*i)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
  EXPECT_TRUE(res[0].affine_access_[0].coefficient_ == 2);
  EXPECT_TRUE(res[0].affine_access_[0].increment_ == 12);
}

TEST(Parser, testThirtyTwo) {
  
  using namespace Parser;
  std::string S = "B(4 -20 + 3*i)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 1);
  EXPECT_TRUE(res[0].affine_access_[0].coefficient_ == 3);
  EXPECT_TRUE(res[0].affine_access_[0].increment_ == -16);
}

TEST(Parser, testThirtyThree) {

  using namespace Parser;
  std::string S = "B(4 + 6)";
  auto res = parse(S);
  std::cout << res << std::endl;
  EXPECT_TRUE(res.size() == 0);
}
