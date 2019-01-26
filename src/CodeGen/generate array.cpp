//
//  generate array.cpp
//  STELA
//
//  Created by Indi Kernick on 11/1/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include "inst data.hpp"
#include "gen types.hpp"
#include "gen helpers.hpp"
#include "generate type.hpp"
#include "compare exprs.hpp"
#include "lifetime exprs.hpp"
#include "function builder.hpp"

using namespace stela;

template <>
llvm::Function *stela::genFn<PFGI::arr_dtor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, unaryCtorFor(type), "arr_dtor");
  assignUnaryCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  ptr_dtor(arr_strg_dtor, bitcast obj)
  */
  
  llvm::Function *strgDtor = data.inst.get<PFGI::arr_strg_dtor>(arr);
  llvm::Value *objRefPtr = refPtrPtrCast(builder.ir, func->arg_begin());
  builder.ir.CreateCall(data.inst.get<FGI::ptr_dtor>(), {strgDtor, objRefPtr});
  builder.ir.CreateRetVoid();
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_def_ctor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, unaryCtorFor(type), "arr_def_ctor");
  assignUnaryCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  obj = malloc
  obj.ref = 1
  obj.cap = 0
  obj.len = 0
  obj.dat = null
  */
  
  llvm::Value *arrayPtr = func->arg_begin();
  llvm::Type *storageTy = type->getPointerElementType();
  llvm::Value *array = callAlloc(builder.ir, data.inst.get<FGI::alloc>(), storageTy);
  initRefCount(builder.ir, array);

  llvm::Value *cap = builder.ir.CreateStructGEP(array, array_idx_cap);
  builder.ir.CreateStore(constantForPtr(cap, 0), cap);
  llvm::Value *len = builder.ir.CreateStructGEP(array, array_idx_len);
  builder.ir.CreateStore(constantForPtr(len, 0), len);
  llvm::Value *dat = builder.ir.CreateStructGEP(array, array_idx_dat);
  setNull(builder.ir, dat);
  builder.ir.CreateStore(array, arrayPtr);
  builder.ir.CreateRetVoid();
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_cop_ctor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_cop_ctor");
  assignBinaryCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  ptr_cop_ctor(bitcast dst, bitcast src)
  */
  
  llvm::Value *dstPtr = refPtrPtrCast(builder.ir, func->arg_begin());
  llvm::Value *srcPtr = refPtrPtrCast(builder.ir, func->arg_begin() + 1);
  builder.ir.CreateCall(data.inst.get<FGI::ptr_cop_ctor>(), {dstPtr, srcPtr});
  builder.ir.CreateRetVoid();
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_cop_asgn>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_cop_asgn");
  assignBinaryAliasCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  ptr_cop_asgn(arr_strg_dtor, bitcast dst, bitcast src)
  */
  
  llvm::Function *strgDtor = data.inst.get<PFGI::arr_strg_dtor>(arr);
  llvm::Value *dstPtr = refPtrPtrCast(builder.ir, func->arg_begin());
  llvm::Value *srcPtr = refPtrPtrCast(builder.ir, func->arg_begin() + 1);
  builder.ir.CreateCall(data.inst.get<FGI::ptr_cop_asgn>(), {strgDtor, dstPtr, srcPtr});
  builder.ir.CreateRetVoid();
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_mov_ctor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_mov_ctor");
  assignBinaryCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  ptr_mov_ctor(bitcast dst, bitcast src)
  */
  
  llvm::Value *dstPtr = refPtrPtrCast(builder.ir, func->arg_begin());
  llvm::Value *srcPtr = refPtrPtrCast(builder.ir, func->arg_begin() + 1);
  builder.ir.CreateCall(data.inst.get<FGI::ptr_mov_ctor>(), {dstPtr, srcPtr});
  builder.ir.CreateRetVoid();
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_mov_asgn>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_mov_asgn");
  assignBinaryCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  ptr_mov_asgn(arr_strg_dtor, bitcast dst, bitcast src)
  */
  
  llvm::Function *strgDtor = data.inst.get<PFGI::arr_strg_dtor>(arr);
  llvm::Value *dstPtr = refPtrPtrCast(builder.ir, func->arg_begin());
  llvm::Value *srcPtr = refPtrPtrCast(builder.ir, func->arg_begin() + 1);
  builder.ir.CreateCall(data.inst.get<FGI::ptr_mov_asgn>(), {strgDtor, dstPtr, srcPtr});
  builder.ir.CreateRetVoid();
  
  return func;
}

namespace {

using BoundsChecker = llvm::Value *(llvm::IRBuilder<> &, llvm::Value *, llvm::Value *);

llvm::Function *generateArrayIdx(
  InstData data,
  ast::ArrayType *arr,
  BoundsChecker *checkBounds,
  const llvm::Twine &name
) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Type *arrayStruct = type->getPointerElementType();
  llvm::Type *elemPtr = arrayStruct->getStructElementType(array_idx_dat);
  llvm::Type *lenType = arrayStruct->getStructElementType(array_idx_len);
  llvm::FunctionType *fnType = llvm::FunctionType::get(
    elemPtr,
    {type->getPointerTo(), lenType},
    false
  );
  llvm::Function *func = makeInternalFunc(data.mod, fnType, name);
  assignUnaryCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  if checkBounds(idx, array.len)
    return array.dat[idx]
  else
    panic
  */
  
  llvm::BasicBlock *okBlock = builder.makeBlock();
  llvm::BasicBlock *errorBlock = builder.makeBlock();
  llvm::Value *array = builder.ir.CreateLoad(func->arg_begin());
  llvm::Value *len = loadStructElem(builder.ir, array, array_idx_len);
  llvm::Value *inBounds = checkBounds(builder.ir, func->arg_begin() + 1, len);
  likely(builder.ir.CreateCondBr(inBounds, okBlock, errorBlock));
  
  builder.setCurr(okBlock);
  llvm::Value *dat = loadStructElem(builder.ir, array, array_idx_dat);
  builder.ir.CreateRet(arrayIndex(builder.ir, dat, func->arg_begin() + 1));
  
  builder.setCurr(errorBlock);
  callPanic(builder.ir, data.inst.get<FGI::panic>(), "Index out of bounds");
  
  return func;
}

llvm::Value *checkSignedBounds(llvm::IRBuilder<> &ir, llvm::Value *index, llvm::Value *size) {
  llvm::Value *aboveZero = ir.CreateICmpSGE(index, constantFor(index, 0));
  llvm::Value *belowSize = ir.CreateICmpSLT(index, size);
  return ir.CreateAnd(aboveZero, belowSize);
}

llvm::Value *checkUnsignedBounds(llvm::IRBuilder<> &ir, llvm::Value *index, llvm::Value *size) {
  return ir.CreateICmpULT(index, size);
}

}

template <>
llvm::Function *stela::genFn<PFGI::arr_idx_s>(InstData data, ast::ArrayType *arr) {
  return generateArrayIdx(data, arr, checkSignedBounds, "arr_idx_s");
}

template <>
llvm::Function *stela::genFn<PFGI::arr_idx_u>(InstData data, ast::ArrayType *arr) {
  return generateArrayIdx(data, arr, checkUnsignedBounds, "arr_idx_u");
}

template <>
llvm::Function *stela::genFn<PFGI::arr_len_ctor>(InstData data, ast::ArrayType *arr) {
  llvm::LLVMContext &ctx = data.mod->getContext();
  llvm::Type *type = generateType(ctx, arr);
  llvm::Type *arrayStructType = type->getPointerElementType();
  llvm::Type *elemPtr = arrayStructType->getStructElementType(array_idx_dat);
  llvm::Type *elem = elemPtr->getPointerElementType();
  llvm::FunctionType *sig = llvm::FunctionType::get(
    elemPtr,
    {type->getPointerTo(), arrayStructType->getStructElementType(array_idx_len)},
    false
  );
  llvm::Function *func = makeInternalFunc(data.mod, sig, "arr_len_ctor");
  func->addParamAttr(0, llvm::Attribute::NonNull);
  FuncBuilder builder{func};
  
  /*
  obj = malloc
  obj.ref = 1
  obj.cap = size
  obj.len = size
  obj.dat = malloc(size)
  */
  
  llvm::Value *objPtr = func->arg_begin();
  llvm::Value *size = func->arg_begin() + 1;
  llvm::Type *storageTy = type->getPointerElementType();
  llvm::Value *obj = callAlloc(builder.ir, data.inst.get<FGI::alloc>(), storageTy);
  initRefCount(builder.ir, obj);
  
  llvm::Value *objCapPtr = builder.ir.CreateStructGEP(obj, array_idx_cap);
  builder.ir.CreateStore(size, objCapPtr);
  llvm::Value *objLenPtr = builder.ir.CreateStructGEP(obj, array_idx_len);
  builder.ir.CreateStore(size, objLenPtr);
  llvm::Value *objDatPtr = builder.ir.CreateStructGEP(obj, array_idx_dat);
  llvm::Value *dat = callAlloc(builder.ir, data.inst.get<FGI::alloc>(), elem, size);
  builder.ir.CreateStore(dat, objDatPtr);
  builder.ir.CreateStore(obj, objPtr);
  builder.ir.CreateRet(dat);
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_strg_dtor>(InstData data, ast::ArrayType *arr) {
  llvm::LLVMContext &ctx = data.mod->getContext();
  llvm::Type *type = generateType(ctx, arr);
  llvm::FunctionType *sig = dtorTy(ctx);
  llvm::Function *func = makeInternalFunc(data.mod, sig, "arr_strg_dtor");
  assignUnaryCtorAttrs(func);
  FuncBuilder builder{func};
  
  /*
  destroy_n(obj.dat, obj.len)
  free(storage.dat)
  */
  
  llvm::Value *obj = builder.ir.CreatePointerCast(func->arg_begin(), type);
  llvm::Value *objLen = loadStructElem(builder.ir, obj, array_idx_len);
  llvm::Value *objDat = loadStructElem(builder.ir, obj, array_idx_dat);
  llvm::Value *destroy_n = data.inst.get<PFGI::destroy_n>(arr->elem.get());
  builder.ir.CreateCall(destroy_n, {objDat, objLen});
  callFree(builder.ir, data.inst.get<FGI::free>(), objDat);
  builder.ir.CreateRetVoid();
  
  return func;
}

namespace {

struct ArrPairLoop {
  llvm::BasicBlock *loop;
  llvm::Value *lhs;
  llvm::Value *rhs;
  llvm::Value *lhsEnd;
  llvm::Value *rhsEnd;
};

ArrPairLoop iterArrPair(
  FuncBuilder &builder,
  llvm::Value *lhs,
  llvm::Value *rhs,
  llvm::BasicBlock *done
) {
  llvm::BasicBlock *head = builder.makeBlock();
  llvm::BasicBlock *body = builder.makeBlock();
  llvm::BasicBlock *tail = builder.makeBlock();
  
  llvm::Value *lhsDat = loadStructElem(builder.ir, lhs, array_idx_dat);
  llvm::Value *rhsDat = loadStructElem(builder.ir, rhs, array_idx_dat);
  llvm::Value *lhsLen = loadStructElem(builder.ir, lhs, array_idx_len);
  llvm::Value *rhsLen = loadStructElem(builder.ir, rhs, array_idx_len);
  llvm::Value *lhsEnd = arrayIndex(builder.ir, lhsDat, lhsLen);
  llvm::Value *rhsEnd = arrayIndex(builder.ir, rhsDat, rhsLen);
  llvm::Value *lhsElemPtr = builder.allocStore(lhsDat);
  llvm::Value *rhsElemPtr = builder.allocStore(rhsDat);
  builder.ir.CreateBr(head);
  
  builder.setCurr(head);
  llvm::Value *lhsElem = builder.ir.CreateLoad(lhsElemPtr);
  llvm::Value *rhsElem = builder.ir.CreateLoad(rhsElemPtr);
  llvm::Value *rhsAtEnd = builder.ir.CreateICmpEQ(rhsElem, rhsEnd);
  builder.ir.CreateCondBr(rhsAtEnd, done, body);
  
  builder.setCurr(tail);
  llvm::Value *lhsElemNext = builder.ir.CreateConstInBoundsGEP1_64(lhsElem, 1);
  llvm::Value *rhsElemNext = builder.ir.CreateConstInBoundsGEP1_64(rhsElem, 1);
  builder.ir.CreateStore(lhsElemNext, lhsElemPtr);
  builder.ir.CreateStore(rhsElemNext, rhsElemPtr);
  builder.ir.CreateBr(head);
  
  builder.setCurr(body);
  return {tail, lhsElem, rhsElem, lhsEnd, rhsEnd};
}

}

template <>
llvm::Function *stela::genFn<PFGI::arr_eq>(InstData data, ast::ArrayType *arr) {
  llvm::LLVMContext &ctx = data.mod->getContext();
  llvm::Type *type = generateType(ctx, arr);
  llvm::Function *func = makeInternalFunc(data.mod, compareFor(type), "arr_eq", Inline::hint);
  assignCompareAttrs(func);
  FuncBuilder builder{func};
  
  /*
  if lhs.len == rhs.len
    for lhsElem, rhsElem in lhs, rhs
      if lhsElem == rhsElem
        continue
      else
        return false
    return true
  else
    return false
  */

  llvm::BasicBlock *compareBlock = builder.makeBlock();
  llvm::BasicBlock *equalBlock = builder.makeBlock();
  llvm::BasicBlock *diffBlock = builder.makeBlock();
  llvm::Value *lhs = builder.ir.CreateLoad(func->arg_begin());
  llvm::Value *rhs = builder.ir.CreateLoad(func->arg_begin() + 1);
  llvm::Value *lhsLen = loadStructElem(builder.ir, lhs, array_idx_len);
  llvm::Value *rhsLen = loadStructElem(builder.ir, rhs, array_idx_len);
  llvm::Value *sameLen = builder.ir.CreateICmpEQ(lhsLen, rhsLen);
  builder.ir.CreateCondBr(sameLen, compareBlock, diffBlock);
  
  builder.setCurr(compareBlock);
  ArrPairLoop comp = iterArrPair(builder, lhs, rhs, equalBlock);
  CompareExpr compare{data.inst, builder.ir};
  llvm::Value *eq = compare.eq(arr->elem.get(), lvalue(comp.lhs), lvalue(comp.rhs));
  builder.ir.CreateCondBr(eq, comp.loop, diffBlock);
  
  builder.setCurr(equalBlock);
  returnBool(builder.ir, true);
  builder.setCurr(diffBlock);
  returnBool(builder.ir, false);
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_lt>(InstData data, ast::ArrayType *arr) {
  llvm::LLVMContext &ctx = data.mod->getContext();
  llvm::Type *type = generateType(ctx, arr);
  llvm::Function *func = makeInternalFunc(data.mod, compareFor(type), "arr_lt", Inline::hint);
  assignCompareAttrs(func);
  FuncBuilder builder{func};
  
  /*
  for lhsElem, rhsElem in lhs, rhs
    if lhsElem == lhsEnd
      return true
    else if lhsElem < rhsElem
      return true
    else if rhsElem < lhsElem
      return false
    else
      continue
  return false
  */
  
  llvm::BasicBlock *ltBlock = builder.makeBlock();
  llvm::BasicBlock *geBlock = builder.makeBlock();
  llvm::BasicBlock *notEndBlock = builder.makeBlock();
  llvm::BasicBlock *notLtBlock = builder.makeBlock();
  llvm::Value *lhs = builder.ir.CreateLoad(func->arg_begin());
  llvm::Value *rhs = builder.ir.CreateLoad(func->arg_begin() + 1);
  ArrPairLoop comp = iterArrPair(builder, lhs, rhs, geBlock);
  
  llvm::Value *lhsAtEnd = builder.ir.CreateICmpEQ(comp.lhs, comp.lhsEnd);
  builder.ir.CreateCondBr(lhsAtEnd, ltBlock, notEndBlock);
  
  builder.setCurr(notEndBlock);
  CompareExpr compare{data.inst, builder.ir};
  llvm::Value *lt = compare.lt(arr->elem.get(), lvalue(comp.lhs), lvalue(comp.rhs));
  builder.ir.CreateCondBr(lt, ltBlock, notLtBlock);
  
  builder.setCurr(notLtBlock);
  llvm::Value *gt = compare.lt(arr->elem.get(), lvalue(comp.rhs), lvalue(comp.lhs));
  builder.ir.CreateCondBr(gt, geBlock, comp.loop);
  
  builder.setCurr(ltBlock);
  returnBool(builder.ir, true);
  builder.setCurr(geBlock);
  returnBool(builder.ir, false);
  
  return func;
}
