#ifndef ISLUTILS_PARSER_H
#define ISLUTILS_PARSER_H

#include <iostream>     // std::cout 
#include <algorithm>    // std::remove_if
#include <sstream>      // std::stringstream
#include <string>       // std::string
#include <cassert>      // lovely assert
#include <set>          // std::set
#include <vector>       // std::vector

namespace Error {
  
  class Syntax_error {
    public:
      const char *p_;
      char ch_;
      Syntax_error(const char *q, char ch) { p_ = q; ch_ = ch; };
  };
} // end namespace error


namespace Driver {
  std::stringstream ss;
} // end namespace driver

namespace Lexer {

  enum class Token_value {
    NAME, END,
    PLUS, MINUS, MUL, DIV,
    ASSIGN, LP, RP, COMMA,
    SPACE,
  };

  Token_value curr_tok;
  std::string string_value;

  Token_value get_token();
  void print_token();
} // end namesapce Lexer


namespace Parser {

  class Array {
    public:
      std::string name_;
      std::set<std::string> induction_vars_;
  };
  std::vector<Array> arrays;

  void expr(bool get);
  Array get_array();
  void get_inductions(bool get, std::set<std::string> &c);
  using namespace Lexer;
  using namespace Error;

  std::ostream &operator<<(std::ostream &os, const std::vector<Array> &a) {

    std::cout << "{ \n";
    for (size_t i = 0; i < a.size(); i++) {
      std::cout << a[i].name_ << " ";
      auto set = a[i].induction_vars_;
      for(auto it = set.begin(); it != set.end(); it++) {
        std::cout << *it << " ";
      }
      std::cout << "\n";
    }
    std::cout << "} \n";
    return os;
  }

} // end namespace Parser


#endif
