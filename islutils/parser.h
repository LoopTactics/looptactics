#ifndef PARSER_H
#define PARSER_H

#include "scop.h"

#include <string>

class Parser {
  std::string Filename;

public:
  Parser(std::string Filename);
  Scop getScop();
};

#endif // PARSER_H
