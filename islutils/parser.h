#ifndef ISLUTILS_PARSER_H
#define ISLUTILS_PARSER_H

#include <iostream>     // std::cout 
#include <algorithm>    // std::remove_if
#include <sstream>      // std::stringstream
#include <string>       // std::string
#include <cassert>      // lovely assert
#include <set>          // std::set
#include <vector>       // std::vector
#include "islutils/error.h"

namespace Lexer {

  enum class Token_value {
    NAME, END,
    PLUS, MINUS, MUL, DIV,
    ASSIGN, LP, RP, COMMA,
    SPACE,
  };

  Token_value get_token();
  void print_token();
} // end namesapce Lexer


namespace Parser {

  using namespace Lexer;
  using namespace Error;

  class AccessDescriptor {
    public:
      std::string name_;
      std::set<std::string> induction_vars_;
  };

  void expr(bool get);
  void get_inductions(bool get, std::set<std::string> &c); 
  AccessDescriptor get_access_descriptor();
  std::vector<AccessDescriptor> parse(const std::string &string_to_be_parsed);

} // end namespace Parser


#endif
