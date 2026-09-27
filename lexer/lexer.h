#ifndef __LEXER_H__
#define __LEXER_H__

#include <cstdlib>
#include <string>
#include "token.h"

// Source location tracking
struct SourceLocation {
  int Line;
  int Col;
};

extern SourceLocation CurLoc;
extern SourceLocation LexLoc;

extern int CurTok;
extern std::string IdentifierStr;
extern double NumVal;

int advance();
int gettok();
int getNextToken();
std::string getTokName(int Tok);

#endif