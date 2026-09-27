#include "codegen.h"
#include "parser/ast.h"
#include "KaleidoscopeJIT.h"
#include "llvm/Support/Error.h"

using namespace llvm;
using namespace llvm::orc;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;
std::unique_ptr<DIBuilder> DBuilder;
std::map<std::string, AllocaInst *> NamedValues;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
std::map<char, int> BinopPrecedence;

static std::unique_ptr<KaleidoscopeJIT> TheJIT;
static ExitOnError ExitOnErr;

void InitializeJIT() {
  TheJIT = ExitOnErr(KaleidoscopeJIT::Create());
}

void InitializeModule() {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("my cool jit", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

Function *getFunction(std::string Name) {
  if (auto *F = TheModule->getFunction(Name))
    return F;

  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  return nullptr;
}

AllocaInst *CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                   TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, VarName);
}

DISubroutineType *CreateFunctionType(unsigned NumArgs) {
  SmallVector<Metadata *, 8> EltTys;
  DIType *DblTy = KSDbgInfo.getDoubleTy();
  EltTys.push_back(DblTy);
  for (unsigned i = 0, e = NumArgs; i != e; ++i)
    EltTys.push_back(DblTy);
  return DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(EltTys));
}

// Debug info
DIType *DebugInfo::getDoubleTy() {
  if (DblTy)
    return DblTy;
  DblTy = DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);
  return DblTy;
}

void DebugInfo::emitLocation(ExprAST *AST) {
  if (!AST)
    return Builder->SetCurrentDebugLocation(DebugLoc());
  DIScope *Scope;
  if (LexicalBlocks.empty())
    Scope = TheCU;
  else
    Scope = LexicalBlocks.back();
  Builder->SetCurrentDebugLocation(DILocation::get(
      Scope->getContext(), AST->getLine(), AST->getCol(), Scope));
}