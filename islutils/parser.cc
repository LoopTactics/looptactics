#include "islutils/parser.h"

namespace Lexer {

using namespace std;
Token_value curr_tok;
string string_value;
} // end namespace Lexer

namespace Parser {

using namespace std;
vector<AccessDescriptor> descriptors;
} // end namespace Parser

namespace Driver {

using namespace std;
stringstream ss;
} // end namespace Driver

Lexer::Token_value Lexer::get_token() {

  using namespace Lexer;

  char ch;
  Driver::ss.get(ch);

#ifdef DEBUG
  std::cout << "get_token -> ch = " << ch << "\n";
#endif

  if (!ch) {
    return curr_tok = Token_value::END;
  }
  // https://en.cppreference.com/w/cpp/string/byte/isspace
  // isspace will return true even if the EOF
  // has been reached.
  if (isspace(ch)) {
    if (Driver::ss.eof()) {
      return curr_tok = Token_value::END;
    }
    return curr_tok = Token_value::SPACE;
  }

  switch (ch) {
  case 0:
  case '\n':
    return Token_value::END;
  case '*':
    return curr_tok = Token_value::MUL;
  case '/':
    return curr_tok = Token_value::DIV;
  case '+': {
    string_value = ch;
    Driver::ss.get(ch);
    string_value += ch;
    if (string_value.compare("+=") == 0)
      return curr_tok = Token_value::ASSIGNMENT_BY_ADDITION;
    else
      Driver::ss.putback(ch);
    return curr_tok = Token_value::PLUS;
  }
  case '-':
    return curr_tok = Token_value::MINUS;
  case '(':
    return curr_tok = Token_value::LP;
  case ')':
    return curr_tok = Token_value::RP;
  case '=':
    return curr_tok = Token_value::ASSIGN;
  case ',':
    return curr_tok = Token_value::COMMA;
  case '!':
    return curr_tok = Token_value::EXCLAMATION_POINT;
  default:
    if (isalpha(ch)) {
      string_value = ch;
      while (Driver::ss.get(ch) && isalpha(ch))
        string_value += ch;
      // std::cout << "string_value : " << string_value << std::endl;
      Driver::ss.putback(ch);
      return curr_tok = Token_value::NAME;
    }
    if (isdigit(ch)) {
      string_value = ch;
      while (Driver::ss.get(ch) && isdigit(ch))
        string_value += ch;
      Driver::ss.putback(ch);
      return curr_tok = Token_value::NUMBER;
    }
    throw Error::Error("bad token");
  }
}

void Lexer::print_token() {

  switch (curr_tok) {
  case Token_value::NAME:
    std::cout << "name = " << string_value << "\n";
    break;
  case Token_value::PLUS:
    std::cout << "plus\n";
    break;
  case Token_value::MUL:
    std::cout << "mul\n";
    break;
  case Token_value::ASSIGN:
    std::cout << "assign\n";
    break;
  case Token_value::RP:
    std::cout << "RP\n";
    break;
  case Token_value::END:
    std::cout << "end\n";
    break;
  case Token_value::MINUS:
    std::cout << "minus\n";
    break;
  case Token_value::DIV:
    std::cout << "div\n";
    break;
  case Token_value::LP:
    std::cout << "lp\n";
    break;
  case Token_value::COMMA:
    std::cout << "comma\n";
    break;
  case Token_value::SPACE:
    std::cout << "space\n";
    break;
  case Token_value::EXCLAMATION_POINT:
    std::cout << "exclamation point\n";
    break;
  case Token_value::NUMBER:
    std::cout << "number\n";
    break;
  default:
    std::cout << "Invalid token.\n";
    //assert(0);
  }
}

namespace Parser {
AffineAccess::AffineAccess(const std::string &induction_name, int inc,
                           Increment_type inc_type)
    : induction_var_name_(induction_name), increment_(inc),
      inc_type_(inc_type){}
}


/// Get the coefficients. 
/// We expect only numbers and plus/minus operators after the iterator literal.
/// For example,
/// (i + 2 + 3) is OK
/// (i + j + 2) is rejected
std::tuple<int, Parser::Increment_type> Parser::get_coeff_after_induction() {

  #ifdef DEBUG
    std::cout << __func__ << std::endl;
    print_token();
  #endif

  while ((curr_tok = get_token()) == Token_value::SPACE) {};
  
  if ((curr_tok == Token_value::COMMA) || (curr_tok == Token_value::RP)) {
    return std::make_tuple(0, Increment_type::PLUS);
  }

  int offset = 0;
  while ((curr_tok != Token_value::COMMA) && (curr_tok != Token_value::RP)) {

    if (curr_tok == Token_value::END) 
      throw Error::Error(
        "bad syntax while parsing coefficients");
    if ((curr_tok == Token_value::PLUS) || (curr_tok == Token_value::MINUS)) {
      while ((curr_tok = get_token()) == Token_value::SPACE) {};
      if (curr_tok != Token_value::NUMBER) 
        throw Error::Error(
          "bad syntax: induction must be followed only by numbers");
      else
        offset += std::stoi(string_value);
    }
    else {
      if (curr_tok != Token_value::SPACE)
        throw Error::Error("bad syntax");
    }

    get_token();
  }

  if (offset > 0)
    return std::make_tuple(offset, Increment_type::PLUS);
  else
    return std::make_tuple(offset, Increment_type::MINUS);
}

std::tuple<std::string, int, Parser::Increment_type> 
Parser::get_coeff_before_and_after_induction() {

  #ifdef DEBUG
    std::cout << __func__ << std::endl;
    print_token();
  #endif
  
  assert(curr_tok == Token_value::NUMBER && "Expect number");
  int val_coeff = std::stoi(string_value); 

  std::string array_name{};

  auto last_token = curr_tok;
  while ((curr_tok != Token_value::NAME)) {

    if (curr_tok == Token_value::END || curr_tok == Token_value::RP)
      throw Error::Error(
        "bad syntax while parsing coefficients");
    if (curr_tok == Token_value::PLUS) {
      while ((curr_tok = get_token()) == Token_value::SPACE) {};  
      if (curr_tok == Token_value::NAME) {
        last_token = Token_value::PLUS;
        array_name = string_value;
        break;
      }
      if (curr_tok != Token_value::NUMBER) {
        print_token();
        throw Error::Error(
          "bad syntax: plus operator should be followed by a number");
      }
      else val_coeff += std::stoi(string_value);
    }
    if (curr_tok == Token_value::MINUS) {
      while ((curr_tok = get_token()) == Token_value::SPACE) {};
      if (curr_tok == Token_value::NAME) {
        last_token = Token_value::MINUS;
        array_name = string_value;
        break;
      }
      if (curr_tok != Token_value::NUMBER)
        throw Error::Error(
          "bad syntax: minus operator should be followed by a number");
      else val_coeff -= std::stoi(string_value);
    }
    get_token();
  }

  // the induction variable should be preceded by a number.
  if ((last_token != Token_value::PLUS) && 
      (last_token != Token_value::MINUS)) {
    print_token();
    throw Error::Error(
      "bad syntax expect +/- before induction name");
  }

  std::tuple<int, Increment_type> res{0, Parser::Increment_type::PLUS};
  res = get_coeff_after_induction();
  
  if (std::get<1>(res) == Parser::Increment_type::PLUS)
    val_coeff += std::get<0>(res);
  else 
    val_coeff -= std::get<0>(res);

  if (val_coeff > 0)
    return std::make_tuple(array_name, val_coeff, Increment_type::PLUS);
  else 
    return std::make_tuple(array_name, val_coeff, Increment_type::MINUS);
}

/// get inductions.
///
/// An induction variable name must be followed
/// by a comma or by an RP. All other cases are
/// rejected.
void Parser::get_inductions(bool first_call, std::vector<Parser::AffineAccess> &a) {

  get_token();

#ifdef DEBUG
  std::cout << __func__ << std::endl;
  print_token();
#endif

  if ((first_call && curr_tok != Token_value::NUMBER) &&
      (first_call && curr_tok != Token_value::NAME)) {
    std::cout << "not expected token: ";
    print_token();
    std::cout << "\n";
    throw Error::Error("bad syntax: expecting a number or a name");
  }

  if (curr_tok == Token_value::END)
    throw Error::Error("bad syntax: expected ')' to close induction");

  if (curr_tok == Token_value::SPACE)
    return get_inductions(false, a);

  // handle (i + 8 + 9)
  if (curr_tok == Token_value::NAME) {
  
    std::string array_name = string_value;
    std::tuple<int, Increment_type> res_coeff{0, Parser::Increment_type::PLUS};
    try {
      res_coeff = get_coeff_after_induction();
    } catch (Error::Error e) { throw; }

    a.push_back(
      AffineAccess{array_name, std::get<0>(res_coeff), std::get<1>(res_coeff)});
  }

  // handle (8 + 9 + i + 9  + 8)
  if (curr_tok == Token_value::NUMBER) {
    
    std::tuple<std::string, int, Increment_type> 
      res_coeff{"null", 0, Parser::Increment_type::PLUS};
    try {
      res_coeff = get_coeff_before_and_after_induction();
    } catch (Error::Error e) { throw; }

    a.push_back(
      AffineAccess{std::get<0>(res_coeff), 
                    std::get<1>(res_coeff), std::get<2>(res_coeff)});
  }

  if ((curr_tok != Token_value::NAME) && (curr_tok != Token_value::LP) &&
      (curr_tok != Token_value::SPACE) && (curr_tok != Token_value::RP) &&
      (curr_tok != Token_value::COMMA) && (curr_tok != Token_value::PLUS) &&
      (curr_tok != Token_value::MINUS) && (curr_tok != Token_value::NUMBER))
    throw Error::Error("bad syntax: token not allowed in between '(' and ')'");

  if (curr_tok == Token_value::RP)
    return;
  else
    return get_inductions(false, a);
}

/// Get an access descriptor.
Parser::AccessDescriptor Parser::get_access_descriptor() {

  AccessDescriptor res{};
  res.array_name_ = std::move(string_value);
  get_inductions(true, res.affine_access_);
  return res;
}

/// Parse an expression.
/// We look for a name. If the name is followed by an LP token we hit an array.
/// A name must be followed by an LP to be an array.
/// Name not followed by an LP token are rejected.
void Parser::expr(bool get) {

#ifdef DEBUG
  print_token();
#endif

  if (get)
    get_token();

  switch (curr_tok) {
  case Lexer::Token_value::NAME: {
#ifdef DEBUG
    std::cout << "Lexer::Token_value::NAME\n";
#endif
    while ((curr_tok = get_token()) == Token_value::SPACE) {
    };
    if (curr_tok == Token_value::LP)
      descriptors.push_back(get_access_descriptor());
    else
      throw Error::Error("bad syntax: array name must be followed by '('");
    return;
  }
  case Lexer::Token_value::ASSIGN: {
#ifdef DEBUG
    std::cout << "Lexer::Token_value::ASSIGN\n";
#endif
    if (descriptors.size() != 1)
      throw Error::Error(
          "bad syntax: no array name (or multiple of them) before =");
    descriptors[0].type_ = Type::WRITE;
    return;
  }
  case Lexer::Token_value::ASSIGNMENT_BY_ADDITION: {
#ifdef DEBUG
    std::cout << "Lexer::Token_value::ASSIGNMENT_BY_ADDITION\n";
#endif
    if (descriptors.size() != 1)
      throw Error::Error(
          "bad syntax: no array name (or multiple of them) before +=");
    else
      descriptors[0].type_ = Type::READ_AND_WRITE;
    return;
  }
  case Lexer::Token_value::SPACE: {
#ifdef DEBUG
    std::cout << "Lexer::Token_value::SPACE\n";
#endif
    return expr(true);
  }
  default:
    return;
  }
}

void Parser::reset() {

  using namespace Driver;
  ss.str("");
  ss.clear();
  descriptors.erase(descriptors.begin(), descriptors.end());
}

std::vector<Parser::AccessDescriptor>
Parser::parse(const std::string &string_to_be_parsed) {

#ifdef DEBUG
  std::cout << string_to_be_parsed << "\n";
#endif

  Parser::reset();

  using namespace Driver;
  ss << string_to_be_parsed;

  // FIXME: why end up in a bad state?
  if (ss.fail()) {
    ss.clear();
    descriptors.clear();
    ss << string_to_be_parsed;
  }

  while (ss) {
    try {
      Lexer::get_token();
#ifdef DEBUG
      std::cout << "Token parse function: ";
      print_token();
#endif
      if (Lexer::curr_tok == Lexer::Token_value::END)
        break;
      Parser::expr(false);
    } catch (Error::Error e) {
      std::cout << "syntax error: " << e.message_ << std::endl;
      descriptors.erase(descriptors.begin(), descriptors.end());
      ss.clear();
      return descriptors;
    }
  }
#ifdef DEBUG
  std::cout << __func__ << "\n";
  std::cout << "# returned descr from parser: " << descriptors.size() << "\n";
#endif
  return descriptors;
}
/*
std::ostream &operator<<(std::ostream &os,
                         const std::vector<Parser::AccessDescriptor> &a) {

  std::cout << "{ \n";
  for (size_t i = 0; i < a.size(); i++) {
    std::cout << a[i].name_ << " ";
    auto set = a[i].induction_vars_;
    for (auto it = set.begin(); it != set.end(); it++) {
      std::cout << *it << " ";
    }
    std::cout << "\n";
  }
  std::cout << "} \n";
  return os;
}
*/
