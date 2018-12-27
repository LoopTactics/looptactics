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

TEST(Parser, simpleParser) {

  std::string s1 = "C[2*i+2][j] += A[i][j] + B[k][j]";

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
    rhs.push_back(assignmentByOperation[0]);
    assignmentByOperation = split(assignmentByOperation[1], "\\+");
    for(const auto &m: assignmentByOperation) {
      lhs.push_back(m);
    }
    lhs.push_back(rhs[0]);
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

  for(const auto &e: lhs) {
    std::cout << "Token lhs:" << e << std::endl;
  }
  for(const auto &e: rhs) {
    std::cout << "Token rhs:" << e << std::endl;
  }
}
