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

namespace Lexer {

  enum class Token_value {
    NAME, END,
    PLUS, MINUS, MUL, DIV,
    ASSIGN, LP, RP, COMMA,
    SPACE,
  };

  //Token_value curr_tok;
  //std::string string_value;

  Token_value get_token();
  void print_token();
} // end namesapce Lexer


namespace Parser {

  class Array {
    public:
      std::string name_;
      std::set<std::string> induction_vars_;
  };
  //std::vector<Array> arrays;

  void expr(bool get);
  Array get_array();
  void get_inductions(bool get, std::set<std::string> &c);
  using namespace Lexer;
  using namespace Error;
  std::vector<Array> parse(const std::string &string_to_be_parsed);

} // end namespace Parser


#endif
