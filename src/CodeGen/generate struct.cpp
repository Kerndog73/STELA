//
//  generate struct.cpp
//  STELA
//
//  Created by Indi Kernick on 11/1/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include "inst data.hpp"
#include "gen helpers.hpp"
#include "generate type.hpp"
#include "compare exprs.hpp"
#include "lifetime exprs.hpp"
#include "function builder.hpp"

using namespace stela;

namespace {

llvm::Function *unarySrt(
  InstData data,
  ast::StructType *srt,
  const llvm::Twine &name,
  void (LifetimeExpr::*memFun)(ast::Type *, llvm::Value *)
) {
  llvm::Type *type = generateType(data.mod->getContext(), srt);
  llvm::Function *func = makeInternalFunc(data.mod, unaryCtorFor(type), name);
  assignUnaryCtorAttrs(func);
  FuncBuilder builder{func};
  LifetimeExpr lifetime{data.inst, builder.ir};
  
  const unsigned fields = type->getNumContainedTypes();
  for (unsigned m = 0; m != fields; ++m) {
    llvm::Value *memPtr = builder.ir.CreateStructGEP(func->arg_begin(), m);
    (lifetime.*memFun)(srt->fields[m].type.get(), memPtr);
  }
  builder.ir.CreateRetVoid();
  
  return func;
}

llvm::Function *binarySrt(
  InstData data,
  ast::StructType *srt,
  const llvm::Twine &name,
  void (LifetimeExpr::*memFun)(ast::Type *, llvm::Value *, llvm::Value *)
) {
  llvm::Type *type = generateType(data.mod->getContext(), srt);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), name);
  if (memFun == &LifetimeExpr::copyAssign) {
    assignBinaryAliasCtorAttrs(func);
  } else {
    assignBinaryCtorAttrs(func);
  }
  FuncBuilder builder{func};
  LifetimeExpr lifetime{data.inst, builder.ir};
  
  const unsigned fields = type->getNumContainedTypes();
  for (unsigned m = 0; m != fields; ++m) {
    llvm::Value *dstPtr = builder.ir.CreateStructGEP(func->arg_begin(), m);
    llvm::Value *srcPtr = builder.ir.CreateStructGEP(func->arg_begin() + 1, m);
    (lifetime.*memFun)(srt->fields[m].type.get(), dstPtr, srcPtr);
  }
  builder.ir.CreateRetVoid();
  
  return func;
}

}

template <>
llvm::Function *stela::genFn<PFGI::srt_dtor>(InstData data, ast::StructType *srt) {
  return unarySrt(data, srt, "srt_dtor", &LifetimeExpr::destroy);
}

template <>
llvm::Function *stela::genFn<PFGI::srt_def_ctor>(InstData data, ast::StructType *srt) {
  return unarySrt(data, srt, "srt_def_ctor", &LifetimeExpr::defConstruct);
}

template <>
llvm::Function *stela::genFn<PFGI::srt_cop_ctor>(InstData data, ast::StructType *srt) {
  return binarySrt(data, srt, "srt_cop_ctor", &LifetimeExpr::copyConstruct);
}

template <>
llvm::Function *stela::genFn<PFGI::srt_cop_asgn>(InstData data, ast::StructType *srt) {
  return binarySrt(data, srt, "srt_cop_asgn", &LifetimeExpr::copyAssign);
}

template <>
llvm::Function *stela::genFn<PFGI::srt_mov_ctor>(InstData data, ast::StructType *srt) {
  return binarySrt(data, srt, "srt_mov_ctor", &LifetimeExpr::moveConstruct);
}

template <>
llvm::Function *stela::genFn<PFGI::srt_mov_asgn>(InstData data, ast::StructType *srt) {
  return binarySrt(data, srt, "srt_mov_asgn", &LifetimeExpr::moveAssign);
}

template <>
llvm::Function *stela::genFn<PFGI::srt_eq>(InstData data, ast::StructType *srt) {
  llvm::Type *type = generateType(data.mod->getContext(), srt);
  llvm::Function *func = makeInternalFunc(data.mod, compareFor(type), "srt_eq");
  assignCompareAttrs(func);
  FuncBuilder builder{func};
  CompareExpr compare{data.inst, builder.ir};
  llvm::BasicBlock *diffBlock = builder.makeBlock();
  
  /*
  for m in struct
    if lhs.m == rhs.m
      continue
    else
      return false
  return true
  */
  
  const unsigned fields = type->getNumContainedTypes();
  for (unsigned m = 0; m != fields; ++m) {
    llvm::Value *lhsMPtr = builder.ir.CreateStructGEP(func->arg_begin(), m);
    llvm::Value *rhsMPtr = builder.ir.CreateStructGEP(func->arg_begin() + 1, m);
    ast::Type *field = srt->fields[m].type.get();
    llvm::Value *eq = compare.eq(field, lvalue(lhsMPtr), lvalue(rhsMPtr));
    llvm::BasicBlock *equalBlock = builder.makeBlock();
    builder.ir.CreateCondBr(eq, equalBlock, diffBlock);
    builder.setCurr(equalBlock);
  }
  
  returnBool(builder.ir, true);
  builder.setCurr(diffBlock);
  returnBool(builder.ir, false);
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::srt_lt>(InstData data, ast::StructType *srt) {
  llvm::Type *type = generateType(data.mod->getContext(), srt);
  llvm::Function *func = makeInternalFunc(data.mod, compareFor(type), "srt_lt");
  assignCompareAttrs(func);
  FuncBuilder builder{func};
  CompareExpr compare{data.inst, builder.ir};
  llvm::BasicBlock *ltBlock = builder.makeBlock();
  llvm::BasicBlock *geBlock = builder.makeBlock();
  
  /*
  for m in struct
    if lhs.m < rhs.m
      return true
    else
      if rhs.m < lhs.m
        return false
      else
        continue
  return false
  */
  
  const unsigned fields = type->getNumContainedTypes();
  for (unsigned m = 0; m != fields; ++m) {
    llvm::Value *lhsMPtr = builder.ir.CreateStructGEP(func->arg_begin(), m);
    llvm::Value *rhsMPtr = builder.ir.CreateStructGEP(func->arg_begin() + 1, m);
    ast::Type *field = srt->fields[m].type.get();
    llvm::Value *less = compare.lt(field, lvalue(lhsMPtr), lvalue(rhsMPtr));
    llvm::BasicBlock *notLessBlock = builder.makeBlock();
    builder.ir.CreateCondBr(less, ltBlock, notLessBlock);
    builder.setCurr(notLessBlock);
    llvm::Value *greater = compare.lt(field, lvalue(rhsMPtr), lvalue(lhsMPtr));
    llvm::BasicBlock *equalBlock = builder.makeBlock();
    builder.ir.CreateCondBr(greater, geBlock, equalBlock);
    builder.setCurr(equalBlock);
  }
  
  builder.ir.CreateBr(geBlock);
  builder.setCurr(ltBlock);
  returnBool(builder.ir, true);
  builder.setCurr(geBlock);
  returnBool(builder.ir, false);
  
  return func;
}
