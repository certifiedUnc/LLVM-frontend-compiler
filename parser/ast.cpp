#include "ast.h"
#include "codegen/codegen.h"
#include "logger/logger.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

using namespace llvm;

DebugInfo KSDbgInfo;

raw_ostream &indent(raw_ostream &O, int size) {
  return O << std::string(size, ' ');
}

// ==== Dump methods ====
raw_ostream &NumberExprAST::dump(raw_ostream &out, int ind) {
  return ExprAST::dump(out << Val, ind);
}

raw_ostream &VariableExprAST::dump(raw_ostream &out, int ind) {
  return ExprAST::dump(out << Name, ind);
}

raw_ostream &UnaryExprAST::dump(raw_ostream &out, int ind) {
  ExprAST::dump(out << "unary" << Opcode, ind);
  Operand->dump(out, ind + 1);
  return out;
}

raw_ostream &BinaryExprAST::dump(raw_ostream &out, int ind) {
  ExprAST::dump(out << "binary" << Op, ind);
  LHS->dump(indent(out, ind) << "LHS:", ind + 1);
  RHS->dump(indent(out, ind) << "RHS:", ind + 1);
  return out;
}

raw_ostream &CallExprAST::dump(raw_ostream &out, int ind) {
  ExprAST::dump(out << "call " << Callee, ind);
  for (const auto &Arg : Args)
    Arg->dump(indent(out, ind + 1), ind + 1);
  return out;
}

raw_ostream &IfExprAST::dump(raw_ostream &out, int ind) {
  ExprAST::dump(out << "if", ind);
  Cond->dump(indent(out, ind) << "Cond:", ind + 1);
  Then->dump(indent(out, ind) << "Then:", ind + 1);
  Else->dump(indent(out, ind) << "Else:", ind + 1);
  return out;
}

raw_ostream &ForExprAST::dump(raw_ostream &out, int ind) {
  ExprAST::dump(out << "for", ind);
  Start->dump(indent(out, ind) << "Cond:", ind + 1);
  End->dump(indent(out, ind) << "End:", ind + 1);
  Step->dump(indent(out, ind) << "Step:", ind + 1);
  Body->dump(indent(out, ind) << "Body:", ind + 1);
  return out;
}

raw_ostream &VarExprAST::dump(raw_ostream &out, int ind) {
  ExprAST::dump(out << "var", ind);
  for (const auto &NamedVar : VarNames)
    NamedVar.second->dump(indent(out, ind) << NamedVar.first << ':', ind + 1);
  Body->dump(indent(out, ind) << "Body:", ind + 1);
  return out;
}

raw_ostream &FunctionAST::dump(raw_ostream &out, int ind) {
  indent(out, ind) << "FunctionAST\n";
  ++ind;
  indent(out, ind) << "Body:";
  return Body ? Body->dump(out, ind) : out << "null\n";
}

// ==== Codegen methods ====
Value *NumberExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *VariableExprAST::codegen() {
  Value *V = NamedValues[Name];
  if (!V)
    return LogErrorV("Unknown variable name");
  KSDbgInfo.emitLocation(this);
  return Builder->CreateLoad(Type::getDoubleTy(*TheContext), V, Name.c_str());
}

Value *UnaryExprAST::codegen() {
  Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;
  Function *F = getFunction(std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("Unknown unary operator");
  KSDbgInfo.emitLocation(this);
  return Builder->CreateCall(F, OperandV, "unop");
}

Value *BinaryExprAST::codegen() {
  KSDbgInfo.emitLocation(this);

  if (Op == '=') {
    VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");
    Value *Val = RHS->codegen();
    if (!Val)
      return nullptr;
    Value *Variable = NamedValues[LHSE->getName()];
    if (!Variable)
      return LogErrorV("Unknown variable name");
    Builder->CreateStore(Val, Variable);
    return Val;
  }

  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  switch (Op) {
  case '+': return Builder->CreateFAdd(L, R, "addtmp");
  case '-': return Builder->CreateFSub(L, R, "subtmp");
  case '*': return Builder->CreateFMul(L, R, "multmp");
  case '<':
    L = Builder->CreateFCmpULT(L, R, "cmptmp");
    return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
  default: break;
  }

  Function *F = getFunction(std::string("binary") + Op);
  assert(F && "binary operator not found!");
  Value *Ops[] = {L, R};
  return Builder->CreateCall(F, Ops, "binop");
}

Value *CallExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  Function *CalleeF = getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("Unknown function referenced");
  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back())
      return nullptr;
  }
  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Value *IfExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  CondV = Builder->CreateFCmpONE(
      CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");

  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);
  Builder->SetInsertPoint(ThenBB);
  Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;
  Builder->CreateBr(MergeBB);
  ThenBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);
  Value *ElseV = Else->codegen();
  if (!ElseV)
    return nullptr;
  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);
  PHINode *PN = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

Value *ForExprAST::codegen() {
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
  KSDbgInfo.emitLocation(this);

  Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;
  Builder->CreateStore(StartVal, Alloca);

  BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);
  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  AllocaInst *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Alloca;

  if (!Body->codegen())
    return nullptr;

  Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen();
    if (!StepVal)
      return nullptr;
  } else {
    StepVal = ConstantFP::get(*TheContext, APFloat(1.0));
  }

  Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  Value *CurVar = Builder->CreateLoad(Type::getDoubleTy(*TheContext), Alloca,
                                      VarName.c_str());
  Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
  Builder->CreateStore(NextVar, Alloca);

  EndCond = Builder->CreateFCmpONE(
      EndCond, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

  BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop", TheFunction);
  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(AfterBB);

  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value *VarExprAST::codegen() {
  std::vector<AllocaInst *> OldBindings;
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();

    Value *InitVal;
    if (Init) {
      InitVal = Init->codegen();
      if (!InitVal)
        return nullptr;
    } else {
      InitVal = ConstantFP::get(*TheContext, APFloat(0.0));
    }

    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    Builder->CreateStore(InitVal, Alloca);

    OldBindings.push_back(NamedValues[VarName]);
    NamedValues[VarName] = Alloca;
  }

  KSDbgInfo.emitLocation(this);
  Value *BodyVal = Body->codegen();
  if (!BodyVal)
    return nullptr;

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    NamedValues[VarNames[i].first] = OldBindings[i];

  return BodyVal;
}

Function *PrototypeAST::codegen() {
  std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*TheContext));
  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);
  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);
  return F;
}

Function *FunctionAST::codegen() {
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  if (P.isBinaryOp())
    BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

  BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  DIFile *Unit = DBuilder->createFile(KSDbgInfo.TheCU->getFilename(),
                                      KSDbgInfo.TheCU->getDirectory());
  DIScope *FContext = Unit;
  unsigned LineNo = P.getLine();
  unsigned ScopeLine = LineNo;
  DISubprogram *SP = DBuilder->createFunction(
      FContext, P.getName(), StringRef(), Unit, LineNo,
      CreateFunctionType(TheFunction->arg_size()), ScopeLine,
      DINode::FlagPrototyped, DISubprogram::SPFlagDefinition);
  TheFunction->setSubprogram(SP);

  KSDbgInfo.LexicalBlocks.push_back(SP);
  KSDbgInfo.emitLocation(nullptr);

  NamedValues.clear();
  unsigned ArgIdx = 0;
  for (auto &Arg : TheFunction->args()) {
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());
    DILocalVariable *D = DBuilder->createParameterVariable(
        SP, Arg.getName(), ++ArgIdx, Unit, LineNo, KSDbgInfo.getDoubleTy(), true);
    DBuilder->insertDeclare(Alloca, D, DBuilder->createExpression(),
                            DILocation::get(SP->getContext(), LineNo, 0, SP),
                            Builder->GetInsertBlock());
    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = Alloca;
  }

  KSDbgInfo.emitLocation(Body.get());

  if (Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    KSDbgInfo.LexicalBlocks.pop_back();
    verifyFunction(*TheFunction);
    return TheFunction;
  }

  TheFunction->eraseFromParent();
  if (P.isBinaryOp())
    BinopPrecedence.erase(Proto->getOperatorName());
  KSDbgInfo.LexicalBlocks.pop_back();
  return nullptr;
}