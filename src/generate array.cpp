//
//  generate array.cpp
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

template <>
llvm::Function *stela::genFn<PFGI::arr_dtor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, unaryCtorFor(type), "arr_dtor");
  assignUnaryCtorAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  ptr_dtor(arr_strg_dtor, bitcast obj)
  */
  
  llvm::Function *strgDtor = data.inst.get<PFGI::arr_strg_dtor>(arr);
  llvm::Value *obj = refPtrPtrCast(funcBdr.ir, func->arg_begin());
  funcBdr.ir.CreateCall(data.inst.get<FGI::ptr_dtor>(), {strgDtor, obj});
  funcBdr.ir.CreateRetVoid();
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_def_ctor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, unaryCtorFor(type), "arr_def_ctor");
  assignUnaryCtorAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  ptr_def_ctor(bitcast obj, sizeof storage)
  obj.cap = 0
  obj.len = 0
  obj.dat = null
  */
  
  llvm::Value *arrayPtr = func->arg_begin();
  llvm::Type *storageTy = type->getPointerElementType();
  llvm::Value *array = callAlloc(funcBdr.ir, data.inst.get<FGI::alloc>(), storageTy);
  initRefCount(funcBdr.ir, array);

  llvm::Value *cap = funcBdr.ir.CreateStructGEP(array, array_idx_cap);
  funcBdr.ir.CreateStore(constantForPtr(cap, 0), cap);
  llvm::Value *len = funcBdr.ir.CreateStructGEP(array, array_idx_len);
  funcBdr.ir.CreateStore(constantForPtr(len, 0), len);
  llvm::Value *dat = funcBdr.ir.CreateStructGEP(array, array_idx_dat);
  setNull(funcBdr.ir, dat);
  funcBdr.ir.CreateStore(array, arrayPtr);
  
  funcBdr.ir.CreateRetVoid();
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_cop_ctor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_cop_ctor");
  assignBinaryCtorAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  ptr_cop_ctor(bitcast obj, bitcast other)
  */
  
  llvm::Value *objPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin());
  llvm::Value *otherPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin() + 1);
  funcBdr.ir.CreateCall(data.inst.get<FGI::ptr_cop_ctor>(), {objPtr, otherPtr});
  funcBdr.ir.CreateRetVoid();
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_cop_asgn>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_cop_asgn");
  assignBinaryAliasCtorAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  ptr_cop_asgn(arr_strg_dtor, bitcast left, bitcast right)
  */
  
  llvm::Function *strgDtor = data.inst.get<PFGI::arr_strg_dtor>(arr);
  llvm::Value *leftPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin());
  llvm::Value *rightPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin() + 1);
  funcBdr.ir.CreateCall(data.inst.get<FGI::ptr_cop_asgn>(), {strgDtor, leftPtr, rightPtr});
  funcBdr.ir.CreateRetVoid();
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_mov_ctor>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_mov_ctor");
  assignBinaryCtorAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  ptr_mov_ctor(bitcast obj, bitcast other)
  */
  
  llvm::Value *objPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin());
  llvm::Value *otherPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin() + 1);
  funcBdr.ir.CreateCall(data.inst.get<FGI::ptr_mov_ctor>(), {objPtr, otherPtr});
  funcBdr.ir.CreateRetVoid();
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_mov_asgn>(InstData data, ast::ArrayType *arr) {
  llvm::Type *type = generateType(data.mod->getContext(), arr);
  llvm::Function *func = makeInternalFunc(data.mod, binaryCtorFor(type), "arr_mov_asgn");
  assignBinaryCtorAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  ptr_mov_asgn(arr_strg_dtor, bitcast left, bitcast right)
  */
  
  llvm::Function *strgDtor = data.inst.get<PFGI::arr_strg_dtor>(arr);
  llvm::Value *leftPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin());
  llvm::Value *rightPtr = refPtrPtrCast(funcBdr.ir, func->arg_begin() + 1);
  funcBdr.ir.CreateCall(data.inst.get<FGI::ptr_mov_asgn>(), {strgDtor, leftPtr, rightPtr});
  funcBdr.ir.CreateRetVoid();
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
  FuncBuilder funcBdr{func};
  
  /*
  if checkBounds(idx, array.len)
    return array.dat[idx]
  else
    panic
  */
  
  llvm::BasicBlock *okBlock = funcBdr.makeBlock();
  llvm::BasicBlock *errorBlock = funcBdr.makeBlock();
  llvm::Value *array = funcBdr.ir.CreateLoad(func->arg_begin());
  llvm::Value *len = loadStructElem(funcBdr.ir, array, array_idx_len);
  llvm::Value *inBounds = checkBounds(funcBdr.ir, func->arg_begin() + 1, len);
  likely(funcBdr.ir.CreateCondBr(inBounds, okBlock, errorBlock));
  
  funcBdr.setCurr(okBlock);
  llvm::Value *dat = loadStructElem(funcBdr.ir, array, array_idx_dat);
  funcBdr.ir.CreateRet(arrayIndex(funcBdr.ir, dat, func->arg_begin() + 1));
  
  funcBdr.setCurr(errorBlock);
  callPanic(funcBdr.ir, data.inst.get<FGI::panic>(), "Index out of bounds");
  
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
  FuncBuilder funcBdr{func};
  
  /*
  array = malloc
  array.cap = size
  array.len = size
  array.dat = malloc(size)
  */
  
  llvm::Value *arrayPtr = func->arg_begin();
  llvm::Value *size = func->arg_begin() + 1;
  llvm::Type *storageTy = type->getPointerElementType();
  llvm::Value *array = callAlloc(funcBdr.ir, data.inst.get<FGI::alloc>(), storageTy);
  initRefCount(funcBdr.ir, array);
  
  llvm::Value *cap = funcBdr.ir.CreateStructGEP(array, array_idx_cap);
  funcBdr.ir.CreateStore(size, cap);
  llvm::Value *len = funcBdr.ir.CreateStructGEP(array, array_idx_len);
  funcBdr.ir.CreateStore(size, len);
  llvm::Value *dat = funcBdr.ir.CreateStructGEP(array, array_idx_dat);
  llvm::Value *allocation = callAlloc(funcBdr.ir, data.inst.get<FGI::alloc>(), elem, size);
  funcBdr.ir.CreateStore(allocation, dat);
  funcBdr.ir.CreateStore(array, arrayPtr);
  
  funcBdr.ir.CreateRet(allocation);
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_strg_dtor>(InstData data, ast::ArrayType *arr) {
  llvm::LLVMContext &ctx = data.mod->getContext();
  llvm::Type *type = generateType(ctx, arr);
  llvm::FunctionType *sig = refPtrDtorTy(ctx);
  llvm::Function *func = makeInternalFunc(data.mod, sig, "arr_strg_dtor");
  assignUnaryCtorAttrs(func);
  FuncBuilder funcBdr{func};
  
  llvm::BasicBlock *head = funcBdr.makeBlock();
  llvm::BasicBlock *body = funcBdr.makeBlock();
  llvm::BasicBlock *done = funcBdr.makeBlock();
  llvm::Value *storage = funcBdr.ir.CreatePointerCast(func->arg_begin(), type);
  llvm::Value *len = loadStructElem(funcBdr.ir, storage, array_idx_len);
  llvm::Value *idxPtr = funcBdr.allocStore(constantFor(len, 0));
  llvm::Value *dat = loadStructElem(funcBdr.ir, storage, array_idx_dat);
  
  funcBdr.branch(head);
  llvm::Value *idx = funcBdr.ir.CreateLoad(idxPtr);
  llvm::Value *atEnd = funcBdr.ir.CreateICmpEQ(idx, len);
  llvm::Value *incIdx = funcBdr.ir.CreateNSWAdd(idx, constantFor(idx, 1));
  funcBdr.ir.CreateStore(incIdx, idxPtr);
  funcBdr.ir.CreateCondBr(atEnd, done, body);
  
  funcBdr.setCurr(body);
  // @TODO can we just use idx?
  llvm::Value *idx1 = funcBdr.ir.CreateLoad(idxPtr);
  LifetimeExpr lifetime{data.inst, funcBdr.ir};
  lifetime.destroy(arr->elem.get(), arrayIndex(funcBdr.ir, dat, idx1));
  funcBdr.ir.CreateBr(head);
  
  funcBdr.setCurr(done);
  funcBdr.ir.CreateRetVoid();
  return func;
}

namespace {

struct ArrPairLoop {
  llvm::BasicBlock *loop;
  llvm::Value *left;
  llvm::Value *right;
  llvm::Value *leftEnd;
  llvm::Value *rightEnd;
};

ArrPairLoop iterArrPair(
  FuncBuilder &funcBdr,
  llvm::Value *leftArr,
  llvm::Value *rightArr,
  llvm::BasicBlock *done
) {
  llvm::BasicBlock *head = funcBdr.makeBlock();
  llvm::BasicBlock *body = funcBdr.makeBlock();
  llvm::BasicBlock *tail = funcBdr.makeBlock();
  
  llvm::Value *leftDat = loadStructElem(funcBdr.ir, leftArr, array_idx_dat);
  llvm::Value *rightDat = loadStructElem(funcBdr.ir, rightArr, array_idx_dat);
  llvm::Value *leftLen = loadStructElem(funcBdr.ir, leftArr, array_idx_len);
  llvm::Value *rightLen = loadStructElem(funcBdr.ir, rightArr, array_idx_len);
  llvm::Value *leftEnd = arrayIndex(funcBdr.ir, leftDat, leftLen);
  llvm::Value *rightEnd = arrayIndex(funcBdr.ir, rightDat, rightLen);
  llvm::Value *leftPtr = funcBdr.allocStore(leftDat);
  llvm::Value *rightPtr = funcBdr.allocStore(rightDat);
  
  funcBdr.branch(head);
  llvm::Value *left = funcBdr.ir.CreateLoad(leftPtr);
  llvm::Value *right = funcBdr.ir.CreateLoad(rightPtr);
  llvm::Value *rightAtEnd = funcBdr.ir.CreateICmpEQ(right, rightEnd);
  funcBdr.ir.CreateCondBr(rightAtEnd, done, body);
  
  funcBdr.setCurr(tail);
  llvm::Value *leftNext = funcBdr.ir.CreateConstInBoundsGEP1_64(left, 1);
  llvm::Value *rightNext = funcBdr.ir.CreateConstInBoundsGEP1_64(right, 1);
  funcBdr.ir.CreateStore(leftNext, leftPtr);
  funcBdr.ir.CreateStore(rightNext, rightPtr);
  funcBdr.ir.CreateBr(head);
  
  funcBdr.setCurr(body);
  return {tail, left, right, leftEnd, rightEnd};
}

}

template <>
llvm::Function *stela::genFn<PFGI::arr_eq>(InstData data, ast::ArrayType *arr) {
  llvm::LLVMContext &ctx = data.mod->getContext();
  llvm::Type *type = generateType(ctx, arr);
  llvm::Function *func = makeInternalFunc(data.mod, compareFor(type), "arr_eq");
  assignCompareAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  if leftArr.len == rightArr.len
    for left, right in leftArr, rightArr
      if left == right
        continue
      else
        return false
    return true
  else
    return false
  */

  llvm::BasicBlock *compareBlock = funcBdr.makeBlock();
  llvm::BasicBlock *equalBlock = funcBdr.makeBlock();
  llvm::BasicBlock *diffBlock = funcBdr.makeBlock();
  llvm::Value *leftArr = funcBdr.ir.CreateLoad(func->arg_begin());
  llvm::Value *rightArr = funcBdr.ir.CreateLoad(func->arg_begin() + 1);
  llvm::Value *leftLen = loadStructElem(funcBdr.ir, leftArr, array_idx_len);
  llvm::Value *rightLen = loadStructElem(funcBdr.ir, rightArr, array_idx_len);
  llvm::Value *sameLen = funcBdr.ir.CreateICmpEQ(leftLen, rightLen);
  funcBdr.ir.CreateCondBr(sameLen, compareBlock, diffBlock);
  
  funcBdr.setCurr(compareBlock);
  ArrPairLoop comp = iterArrPair(funcBdr, leftArr, rightArr, equalBlock);
  CompareExpr compare{data.inst, funcBdr.ir};
  llvm::Value *eq = compare.equal(arr->elem.get(), lvalue(comp.left), lvalue(comp.right));
  funcBdr.ir.CreateCondBr(eq, comp.loop, diffBlock);
  
  funcBdr.setCurr(equalBlock);
  returnBool(funcBdr.ir, true);
  funcBdr.setCurr(diffBlock);
  returnBool(funcBdr.ir, false);
  
  return func;
}

template <>
llvm::Function *stela::genFn<PFGI::arr_lt>(InstData data, ast::ArrayType *arr) {
  llvm::LLVMContext &ctx = data.mod->getContext();
  llvm::Type *type = generateType(ctx, arr);
  llvm::Function *func = makeInternalFunc(data.mod, compareFor(type), "arr_lt");
  assignCompareAttrs(func);
  FuncBuilder funcBdr{func};
  
  /*
  for left, right in leftArr, rightArr
    if left == leftEnd
      return true
    else if left < right
      return true
    else if right < left
      return false
    else
      continue
  return false
  */
  
  llvm::BasicBlock *ltBlock = funcBdr.makeBlock();
  llvm::BasicBlock *geBlock = funcBdr.makeBlock();
  llvm::BasicBlock *notEndBlock = funcBdr.makeBlock();
  llvm::BasicBlock *notLtBlock = funcBdr.makeBlock();
  llvm::Value *leftArr = funcBdr.ir.CreateLoad(func->arg_begin());
  llvm::Value *rightArr = funcBdr.ir.CreateLoad(func->arg_begin() + 1);
  ArrPairLoop comp = iterArrPair(funcBdr, leftArr, rightArr, geBlock);
  
  llvm::Value *leftAtEnd = funcBdr.ir.CreateICmpEQ(comp.left, comp.leftEnd);
  funcBdr.ir.CreateCondBr(leftAtEnd, ltBlock, notEndBlock);
  
  funcBdr.setCurr(notEndBlock);
  CompareExpr compare{data.inst, funcBdr.ir};
  llvm::Value *lt = compare.less(arr->elem.get(), lvalue(comp.left), lvalue(comp.right));
  funcBdr.ir.CreateCondBr(lt, ltBlock, notLtBlock);
  
  funcBdr.setCurr(notLtBlock);
  llvm::Value *gt = compare.less(arr->elem.get(), lvalue(comp.right), lvalue(comp.left));
  funcBdr.ir.CreateCondBr(gt, geBlock, comp.loop);
  
  funcBdr.setCurr(ltBlock);
  returnBool(funcBdr.ir, true);
  funcBdr.setCurr(geBlock);
  returnBool(funcBdr.ir, false);
  
  return func;
}
