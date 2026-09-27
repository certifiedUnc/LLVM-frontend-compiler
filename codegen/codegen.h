#ifndef __CODEGEN_H__
#define __CODEGEN_H__

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DIBuilder.h"
#include <map>
#include <memory>
#include <string>

using namespace llvm;

// Forward declarations
class PrototypeAST;

// Global codegen state
extern std::unique_ptr<LLVMContext> TheContext;
extern std::unique_ptr<Module> TheModule;
extern std::unique_ptr<IRBuilder<>> Builder;
extern std::unique_ptr<DIBuilder> DBuilder;
extern std::map<std::string, AllocaInst *> NamedValues;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
extern std::map<char, int> BinopPrecedence;

// Helpers
Function *getFunction(std::string Name);
AllocaInst *CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName);
DISubroutineType *CreateFunctionType(unsigned NumArgs);

// Module management
void InitializeModule();
void InitializeJIT();

#endif