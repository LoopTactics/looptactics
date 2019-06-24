#include "islutils/parser.h"

Lexer::Token_value Lexer::get_token() {
 
  char ch;
  Driver::ss.get(ch);

  if (!ch) return curr_tok = Token_value::END;
  if (isspace(ch)) return curr_tok = Token_value::SPACE;
  
  switch (ch) {
    case 0:
    case '\n':
      {
        //std::cout << ch << ": is end string\n";
        return Token_value::END;
      }
    case '*':
      return curr_tok = Token_value::MUL;
    case '/':
      return curr_tok = Token_value::DIV;
    case '+':
      return curr_tok = Token_value::PLUS;
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
    default:
      if (isalpha(ch)) {
        string_value = ch;
        while (Driver::ss.get(ch) && isalpha(ch))
          string_value += ch;
        //std::cout << "string_value : " << string_value << std::endl;
        Driver::ss.putback(ch);
        return curr_tok = Token_value::NAME;
      }
      throw Error::Syntax_error("bad token", ch);
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
    default:
      std::cout << "boh\n";
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
      throw Error::Syntax_error(
        "bad syntax: induction must be followed by ',' or ')'", __LINE__);
    }
    else {
      c.insert(string_value);
    }
  }
/*
  // FIXME: you want to do these tests but do not mess-up
  // the driver position in the stream!
  if ((curr_tok != Token_value::NAME) &&
      (curr_tok != Token_value::LP) &&
      (curr_tok != Token_value::SPACE) &&
      (curr_tok != Token_value::RP) &&
      (curr_tok != Token_value::COMMA))
    throw Error::Syntax_error(  
      "bad syntax: token not allowed in between '(' and ')'", __LINE__);

  if (curr_tok == Token_value::COMMA) {
    while ((curr_tok = get_token()) == Token_value::SPACE) {};
    if (curr_tok != Token_value::NAME) {
      throw Error::Syntax_error(
        "bad syntax: ',' must be followed by an array name", __LINE__);
    }
  }
*/

  if (curr_tok == Token_value::RP)
    return;
  else
    return get_inductions(true, c);
}

/// Build an array.
Parser::Array Parser::get_array() {

  Array res {};
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
        arrays.push_back(get_array());
      else throw Error::Syntax_error(
        "bad syntax: array name must be followed by '('", __LINE__);
      return;
    }
    case Lexer::Token_value::SPACE: {
      return expr(true);
    } 
    default:
      return;
  }
}

/*
int main(int argc, char *argv[]) {

  std::string S1 = "CB (ii, jj) += A(i,k) * C(k, j)";

  using namespace Driver;
  ss << S1;

  while(ss) {    
    try {
      Lexer::get_token();
      if (Lexer::curr_tok == Lexer::Token_value::END) break;
      Parser::expr(false);   
    }
    catch (Error::Syntax_error e) {
      std::cout << "syntax error: " << e.p_ << std::endl;
      return -1;
    }
  }

  std::cout << Parser::arrays << "\n";
  return 0;
} 
*/
