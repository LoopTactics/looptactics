#include "gtest/gtest.h"
#include <vector>
#include <regex>
#include <string>
#include <iostream>
#include <cassert>

/*
The general idea is:
1. Allow the user to specify the matchers
2. Automatically instantiate those matchers.

#pragma matcher 
  C[i][j] += A[i][k] + B[k][j]
#pragma endmatcher

will instantiate:

  arrayPlaceholder C_
  arrayPlaceholder B_
  arrayPlaceholder A_
  
  Placeholder i_
  Placeholder j_
  Placeholder k_

  match(reads, allOf(access(C_, i_, j_) ...
*/

class AccessInfo {
  public:
    // 2 * i (2 is the coefficient)
    std::vector<int> coefficients;
    // is the increment positive or negative?
    // i + 1 (positive: 1)
    // i - 1 (negative: 0)
    // i (no increment: -1)
    std::vector<int> signs;
    // actual value for the increment
    std::vector<int> increments;
    // induction variable(s)
    std::vector<std::string> inductions;
    // array name
    std::string arrayId;
};
    

std::vector<std::string>
split(const std::string &s, std::string regexAsString) {

  std::vector<std::string> res;
  std::regex regex(regexAsString);

  std::sregex_token_iterator iter(s.begin(), s.end(), regex, -1);
  std::sregex_token_iterator end;
  while(iter != end) {
    res.push_back(*iter);
    ++iter;
  }
  return res;
}

static AccessInfo
getAccessInfo(std::string a) {

  AccessInfo ai;

  // step 1. get array name.
  std::regex regexArrayId(R"((\w+)\[+.*)");
  std::smatch match;
  std::regex_match(a, match, regexArrayId);

  ai.arrayId = match.str(1);

  // step 2. array subscripts.
  // match : alpha*induction+beta
  // alpha group 1
  // induction group 2
  // beta group 3
  std::regex regexSubscriptCoefficientAndIncrementAddition(R"(\[+(\d)\*([a-z])\+(\d)+\])");
  while(std::regex_search(a, match, regexSubscriptCoefficientAndIncrementAddition)) {
    ai.coefficients.push_back(std::stoi(match[1].str()));
    ai.inductions.push_back(match[2].str());
    ai.increments.push_back(std::stoi(match[3].str()));
    ai.signs.push_back(1);
    a = match.suffix().str();
  }

  // match : alpha*induction-beta
  // alpha group 1
  // induction group 2 
  // beta group 3
  std::regex regexSubscriptCoefficientAndIncrementSubtraction(R"(\[+(\d)\*([a-z])\-(\d)+\])");
  while(std::regex_search(a, match, regexSubscriptCoefficientAndIncrementSubtraction)) {
    ai.coefficients.push_back(std::stoi(match[1].str()));
    ai.inductions.push_back(match[2].str());
    ai.increments.push_back(std::stoi(match[3].str()));
    ai.signs.push_back(0);
    a = match.suffix().str();
  }

  // match : induction+beta
  // induction group 1
  // beta group 2
  std::regex regexSubscriptIncrementAddition(R"(\[+([a-z])\+(\d)+\])");
  while(std::regex_search(a, match, regexSubscriptIncrementAddition)) {
    ai.coefficients.push_back(1);
    ai.inductions.push_back(match[1].str());
    ai.increments.push_back(std::stoi(match[2].str()));
    ai.signs.push_back(1);
    a = match.suffix().str();
  }

  // match : induction-beta
  // induction group 1
  // beta group 2
  std::regex regexSubscriptIncrementSubtraction(R"(\[+([a-z])\-(\d)+\])");
  while(std::regex_search(a, match, regexSubscriptIncrementSubtraction)) {
    ai.coefficients.push_back(1);
    ai.inductions.push_back(match[1].str());
    ai.increments.push_back(std::stoi(match[2].str()));
    ai.signs.push_back(0);
    a = match.suffix().str();
  }

  // match : induction
  // induction group 1
  std::regex regexSubscript(R"(\[+([a-z])+\])");
  while(std::regex_search(a, match, regexSubscript)) {
    ai.coefficients.push_back(1);
    ai.inductions.push_back(match[1].str());
    ai.increments.push_back(0);
    ai.signs.push_back(-1);
    a = match.suffix().str();
  }

  return ai;
}

TEST(Parser, simpleParser) {

  std::string s1 = "C[2*i+2][j] += AA[i][j] - B[k][j] * alpha";

  // remove white spaces.
  // https://stackoverflow.com/questions/83439/remove-spaces-from-stdstring-in-c
  s1.erase(std::remove_if(s1.begin(), s1.end(), isspace), s1.end());

  std::vector<std::string> lhs, rhs;

  // split the string based on:
  // assignment by addition, multiplication subtraction and division.
  std::vector<std::string> assignmentByOperation = split(s1, "\\+=");

  // expect 0 or two matches. 
  // 0 match if the string does not contain "+="
  // 2 matches if the string contains "+="
  assert(assignmentByOperation.size() == 0 || assignmentByOperation.size() == 2
         && "zero or two matches");

  if(assignmentByOperation.size()) {
    lhs.push_back(assignmentByOperation[0]);
    assignmentByOperation = split(assignmentByOperation[1], "\\+|\\-|\\*");
    for(const auto &m: assignmentByOperation) {
      rhs.push_back(m);
    }
    rhs.push_back(lhs[0]);
  }

  else {
    // if we do not match any assignment by operation we 
    // split the string based on an assignment.
    std::vector<std::string> assignment = split(s1, "=");
    rhs.push_back(assignment[0]);
    assignment = split(assignment[1], "\\+");
    for(const auto &m: assignment) {
      lhs.push_back(m);
    }
  }

  // print lhs-rhs
  for(const auto &e: lhs) {
    std::cout << "Token lhs:" << e << std::endl;
  }
  for(const auto &e: rhs) {
    std::cout << "Token rhs:" << e << std::endl;
  }

  auto accessInfo = getAccessInfo(lhs[0]);
  EXPECT_TRUE(accessInfo.inductions.size() == 2);
  EXPECT_TRUE(accessInfo.arrayId == "C");
  EXPECT_TRUE(accessInfo.coefficients[0] == 2);
  EXPECT_TRUE(accessInfo.coefficients[1] == 1);
  EXPECT_TRUE(accessInfo.increments[0] == 2);
  EXPECT_TRUE(accessInfo.increments[1] == 0);
}
