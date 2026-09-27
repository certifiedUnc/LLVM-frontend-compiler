#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <memory>

// Forward declarations
class ExprAST;
class PrototypeAST;
namespace llvm { class Value; }

std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
llvm::Value *LogErrorV(const char *Str);

#endif