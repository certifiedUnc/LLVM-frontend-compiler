#ifndef __TOKEN_H__
#define __TOKEN_H__

// The lexer returns tokens [0-255] if it's an unknown character
// otherwise it returns one of these for known things
enum Token {
  // End Of File
  tok_eof = -1,

  // Commands
  tok_def = -2,
  tok_extern = -3,

  // Primary
  tok_identifier = -4,
  tok_number = -5,

  // Control
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,
  tok_for = -9,
  tok_in = -10,

  // Operators
  tok_binary = -11,
  tok_unary = -12,

  // Var definition
  tok_var = -13
};

#endif