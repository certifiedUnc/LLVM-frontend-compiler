#include "lexer/lexer.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "codegen/codegen.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include <cstdio>

using namespace llvm;

// ==== Handle functions ====
static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (!FnAST->codegen())
      fprintf(stderr, "Error reading function definition\n");
  } else {
    getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (!ProtoAST->codegen())
      fprintf(stderr, "Error reading extern\n");
    else
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
  } else {
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  if (auto FnAST = ParseTopLevelExpr()) {
    if (!FnAST->codegen())
      fprintf(stderr, "Error generating code for top level expr\n");
  } else {
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    switch (CurTok) {
    case tok_eof: return;
    case ';':     getNextToken(); break;
    case tok_def: HandleDefinition(); break;
    case tok_extern: HandleExtern(); break;
    default: HandleTopLevelExpression(); break;
    }
  }
}

// ==== Library functions ====
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

// ==== Main ====
int main() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  // Install standard binary operators
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;

  // Prime the first token
  getNextToken();

  InitializeJIT();
  InitializeModule();

  TheModule->addModuleFlag(Module::Warning, "Debug Info Version",
                           DEBUG_METADATA_VERSION);

  if (Triple(sys::getProcessTriple()).isOSDarwin())
    TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);

  DBuilder = std::make_unique<DIBuilder>(*TheModule);
  KSDbgInfo.TheCU = DBuilder->createCompileUnit(
      dwarf::DW_LANG_C, DBuilder->createFile("fib.ks", "."),
      "Kaleidoscope Compiler", false, "", 0);

  MainLoop();

  DBuilder->finalize();
  TheModule->print(errs(), nullptr);

  return 0;
}