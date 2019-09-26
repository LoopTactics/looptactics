#ifndef ISLUTILS_PARSER_H
#define ISLUTILS_PARSER_H

#include <iostream>     // std::cout 
#include <algorithm>    // std::remove_if
#include <sstream>      // std::stringstream
#include <string>       // std::string
#include <cassert>      // lovely assert
#include <set>          // std::set
#include <vector>       // std::vector
#include <tuple>        // std::tuple
#include "islutils/error.h"

namespace Lexer {

  enum class Token_value {
    NAME, END,
    PLUS, MINUS, MUL, DIV,
    ASSIGN, LP, RP, COMMA,
    SPACE, EXCLAMATION_POINT,
    ASSIGNMENT_BY_ADDITION,
    NUMBER, INIT_REDUCTION,
  };

  Token_value get_token();
  void print_token();
} // end namesapce Lexer


namespace Parser {

  using namespace Lexer;
  using namespace Error;

  enum class Increment_type {PLUS, MINUS};
  class AffineAccess {
    public:
      std::string induction_var_name_;
      int increment_;
      Increment_type inc_type_ = Increment_type::PLUS;
      int coefficient_;
      AffineAccess() = delete;
      AffineAccess(const std::string &, int, Increment_type, int);
  };

  enum class Type { READ, WRITE, READ_AND_WRITE, INIT_REDUCTION};
  class AccessDescriptor {
    public:
      Type type_ = Type::READ;
      std::string array_name_;
      std::vector<AffineAccess> affine_accesses_;
  };

  void expr(bool get);
  void get_inductions(bool first_call, std::vector<AffineAccess> &a);
  std::tuple<int, Increment_type> get_coeff_after_induction();
  std::tuple<std::string, int, Increment_type, int> 
    get_coeff_before_and_after_induction();
  void reset(); 
  AccessDescriptor get_access_descriptor();
  std::vector<AccessDescriptor> parse(const std::string &string_to_be_parsed);

} // end namespace Parser


#endif
