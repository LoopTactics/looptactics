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

  if (!ch) return curr_tok = Token_value::END;
  if (isspace(ch)) return curr_tok = Token_value::SPACE;
  
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
        //std::cout << "string_value : " << string_value << std::endl;
        Driver::ss.putback(ch);
        return curr_tok = Token_value::NAME;
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
    default:
      std::cout << "??\n";
      assert(0);
  }
}

/// get inductions.
///
/// An induction variable name must be followed
/// by a comma or by an RP. All other cases are 
/// rejected.
void Parser::get_inductions(bool get, std::set<std::string> &c) {

  if (get) get_token();
 
  if (curr_tok == Token_value::SPACE)
    return get_inductions(true, c);
  
  if (curr_tok == Token_value::NAME) {
    while ((curr_tok = get_token()) == Token_value::SPACE) {};
    if ((curr_tok != Token_value::COMMA) && (curr_tok != Token_value::RP)) {  
      throw Error::Error(
        "bad syntax: induction must be followed by ',' or ')'");
    }
    else {
      c.insert(string_value);
    }
  }

  if ((curr_tok != Token_value::NAME) &&
      (curr_tok != Token_value::LP) &&
      (curr_tok != Token_value::SPACE) &&
      (curr_tok != Token_value::RP) &&
      (curr_tok != Token_value::COMMA))
    throw Error::Error(  
      "bad syntax: token not allowed in between '(' and ')'");

  if (curr_tok == Token_value::RP)
    return;
  else
    return get_inductions(true, c);
}

/// Get an access descriptor.
Parser::AccessDescriptor Parser::get_access_descriptor() {

  AccessDescriptor res {};
  res.name_ = std::move(string_value);
  get_inductions(false, res.induction_vars_);  
  return res;
}

/// Parse an expression.
/// We look for a name. If the name is followed by an LP token we hit an array.
/// A name must be followed by an LP to be an array.
/// Name not followed by an LP token are rejected.
void Parser::expr(bool get) {

  if (get) get_token();

  switch (curr_tok) {
    case Lexer::Token_value::NAME: {
      while ((curr_tok = get_token()) == Token_value::SPACE) {};
      if (curr_tok == Token_value::LP) 
        descriptors.push_back(get_access_descriptor());
      else throw Error::Error(
        "bad syntax: array name must be followed by '('");
      return;
    }
    case Lexer::Token_value::ASSIGN: {
      if (descriptors.size() != 1)
        throw Error::Error(
          "bad syntax: no array name (or multiple of them) before =");
      descriptors[0].type_ = Type::WRITE;
      return;
    }
    case Lexer::Token_value::ASSIGNMENT_BY_ADDITION: {
      if (descriptors.size() != 1)
        throw Error::Error(
          "bad syntax: no array name (or multiple of them) before +=");
      else 
        descriptors[0].type_ = Type::READ_AND_WRITE;
      return;
    } 
    case Lexer::Token_value::SPACE: {
      return expr(true);
    } 
    default:
      return;
  }
}

std::vector<Parser::AccessDescriptor> Parser::parse(const std::string &string_to_be_parsed) {

  using namespace Driver;
  ss << string_to_be_parsed;

  // FIXME: why end up in a bad state?
  if (ss.fail()) {
    ss.clear(); 
  }

  while(ss) {
    try {
      Lexer::get_token();
      if (Lexer::curr_tok == Lexer::Token_value::END) break;
      Parser::expr(false);   
    }
    catch (Error::Error e) {
      std::cout << "syntax error: " << e.message_ << std::endl;
      descriptors.erase(descriptors.begin(), descriptors.end());
      ss.clear();
      return descriptors;
    }
  }
  return descriptors;
}


std::ostream &operator<<(std::ostream &os, const std::vector<Parser::AccessDescriptor> &a) {

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
