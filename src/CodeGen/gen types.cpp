//
//  gen types.cpp
//  STELA
//
//  Created by Indi Kernick on 17/12/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "gen types.hpp"

#include <llvm/IR/Constants.h>
#include "Utils/unreachable.hpp"
#include <llvm/IR/DerivedTypes.h>

using namespace stela;

template <>
llvm::IntegerType *stela::getSizedType<1>(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt8Ty(ctx);
}

template <>
llvm::IntegerType *stela::getSizedType<2>(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt16Ty(ctx);
}

template <>
llvm::IntegerType *stela::getSizedType<4>(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt32Ty(ctx);
}

template <>
llvm::IntegerType *stela::getSizedType<8>(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt64Ty(ctx);
}

template <>
llvm::IntegerType *stela::getSizedType<16>(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt128Ty(ctx);
}

llvm::IntegerType *stela::getSizedType(llvm::LLVMContext &ctx, const size_t size) {
  switch (size) {
    case 1: return getSizedType<1>(ctx);
    case 2: return getSizedType<2>(ctx);
    case 4: return getSizedType<4>(ctx);
    case 8: return getSizedType<8>(ctx);
    case 16: return getSizedType<16>(ctx);
  }
  UNREACHABLE();
}

llvm::IntegerType *stela::lenTy(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt32Ty(ctx);
}

llvm::IntegerType *stela::refTy(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt64Ty(ctx);
}

llvm::Type *stela::voidTy(llvm::LLVMContext &ctx) {
  return llvm::Type::getVoidTy(ctx);
}

llvm::PointerType *stela::voidPtrTy(llvm::LLVMContext &ctx) {
  return llvm::Type::getInt8PtrTy(ctx);
}

llvm::FunctionType *stela::dtorTy(llvm::LLVMContext &ctx) {
  return llvm::FunctionType::get(
    voidTy(ctx), {voidPtrTy(ctx)}, false
  );
}

llvm::PointerType *stela::ptrToDtorTy(llvm::LLVMContext &ctx) {
  return dtorTy(ctx)->getPointerTo();
}

llvm::StructType *stela::cloDataTy(llvm::LLVMContext &ctx) {
  return llvm::StructType::get(ctx, {
    refTy(ctx),      // reference count
    ptrToDtorTy(ctx) // virtual destructor
  });
}

llvm::PointerType *stela::ptrToCloDataTy(llvm::LLVMContext &ctx) {
  return cloDataTy(ctx)->getPointerTo();
}

llvm::StructType *stela::arrayTy(llvm::Type *elem) {
  llvm::LLVMContext &ctx = elem->getContext();
  return llvm::StructType::get(ctx, {
    refTy(ctx),          // reference count
    lenTy(ctx),          // capacity
    lenTy(ctx),          // length
    elem->getPointerTo() // data
  }, true);
}

llvm::PointerType *stela::ptrToArrayTy(llvm::Type *elem) {
  return arrayTy(elem)->getPointerTo();
}

llvm::ConstantPointerNull *stela::nullPtr(llvm::PointerType *ptr) {
  return llvm::ConstantPointerNull::get(ptr);
}

llvm::ConstantPointerNull *stela::nullPtrTo(llvm::Type *type) {
  return nullPtr(type->getPointerTo());
}
