//
//  generate decl.cpp
//  STELA
//
//  Created by Indi Kernick on 30/11/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "generate decl.hpp"

#include "symbols.hpp"
#include "generate type.hpp"
#include "generate stat.hpp"
#include "generate expr.hpp"
#include <llvm/IR/Function.h>
#include "lifetime exprs.hpp"

using namespace stela;

namespace {

class Visitor final : public ast::Visitor {
public:
  Visitor(gen::Ctx ctx, llvm::Module *module)
    : ctx{ctx}, module{module} {}
  
  llvm::GlobalObject::LinkageTypes linkage(const bool ext) {
    return ext ? llvm::GlobalObject::ExternalLinkage : llvm::GlobalObject::InternalLinkage;
  }
  
  void visit(ast::Func &func) override {
    llvm::FunctionType *fnType = generateFuncSig(ctx, func);
    func.llvmFunc = llvm::Function::Create(
      fnType,
      linkage(func.external),
      llvm::StringRef{func.name.data(), func.name.size()},
      module
    );
    assignAttributes(func.llvmFunc, func.symbol->params);
    
    generateStat(ctx, func.llvmFunc, func.receiver, func.params, func.body);
  }
  
  llvm::Twine ctorName(const llvm::StringRef &objName) {
    return llvm::Twine{objName, "_ctor"};
  }
  llvm::Twine dtorName(const llvm::StringRef &objName) {
    return llvm::Twine{objName, "_dtor"};
  }
  llvm::FunctionType *ctorDtorSig() {
    return llvm::FunctionType::get(
      llvm::Type::getVoidTy(ctx.llvm), false
    );
  }
  llvm::Function *makeCtorDtor(const llvm::Twine &name) {
    llvm::Function *func = llvm::Function::Create(
      ctorDtorSig(),
      llvm::Function::InternalLinkage,
      name,
      module
    );
    func->addFnAttr(llvm::Attribute::NoUnwind);
    func->addFnAttr(llvm::Attribute::NoRecurse);
    func->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    return func;
  }
  
  llvm::Value *createGlobalVar(
    ast::Type *type,
    const std::string_view name,
    ast::Expression *expr,
    const bool external
  ) {
    llvm::Type *llvmType = generateType(ctx, type);
    llvm::Value *llvmAddr = new llvm::GlobalVariable{
      *module,
      llvmType,
      false,
      linkage(external),
      llvm::UndefValue::get(llvmType),
      llvm::StringRef{name.data(), name.size()}
    };
    llvm::StringRef nameRef{name.data(), name.size()};
    
    llvm::Function *ctor = makeCtorDtor(ctorName(nameRef));
    ctor->addFnAttr(llvm::Attribute::ReadNone);
    FuncBuilder ctorBuilder{ctor};
    if (expr) {
      generateExpr(ctx, ctorBuilder, expr, llvmAddr);
    } else {
      LifetimeExpr lifetime{ctx.inst, ctorBuilder.ir};
      lifetime.defConstruct(type, llvmAddr);
    }
    ctorBuilder.ir.CreateRetVoid();
    ctors.push_back(ctor);
    
    llvm::Function *dtor = makeCtorDtor(dtorName(nameRef));
    dtor->addFnAttr(llvm::Attribute::ReadOnly);
    FuncBuilder dtorBuilder{dtor};
    LifetimeExpr lifetime{ctx.inst, dtorBuilder.ir};
    lifetime.destroy(type, llvmAddr);
    dtorBuilder.ir.CreateRetVoid();
    dtors.push_back(dtor);
    
    return llvmAddr;
  }
  void visit(ast::Var &var) override {
    var.llvmAddr = createGlobalVar(
      var.symbol->etype.type.get(), var.name, var.expr.get(), var.external
    );
  }
  void visit(ast::Let &let) override {
    let.llvmAddr = createGlobalVar(
      let.symbol->etype.type.get(), let.name, let.expr.get(), let.external
    );
  }
  
  llvm::StructType *getEntryType() {
    return llvm::StructType::get(
      llvm::IntegerType::getInt32Ty(ctx.llvm),
      ctorDtorSig()->getPointerTo()
    );
  }
  std::vector<llvm::Constant *> createEntries(
    const std::vector<llvm::Function *> &funcs,
    llvm::StructType *entryType
  ) {
    std::vector<llvm::Constant *> ctorEntries;
    ctorEntries.reserve(funcs.size());
    for (size_t c = 0; c != funcs.size(); ++c) {
      ctorEntries.push_back(llvm::ConstantStruct::get(entryType,
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ctx.llvm), c),
        funcs[c]
      ));
    }
    return ctorEntries;
  }
  void createListVar(
    const std::vector<llvm::Constant *> &entries,
    llvm::ArrayType *listType,
    const llvm::Twine &name
  ) {
    new llvm::GlobalVariable{
      *module,
      listType,
      false,
      llvm::Function::AppendingLinkage,
      llvm::ConstantArray::get(listType, entries),
      name
    };
  }
  
  void writeCtorList() {
    if (ctors.empty()) {
      return;
    }
    llvm::StructType *entryType = getEntryType();
    llvm::ArrayType *listType = llvm::ArrayType::get(entryType, ctors.size());
    createListVar(createEntries(ctors, entryType), listType, "llvm.global_ctors");
    createListVar(createEntries(dtors, entryType), listType, "llvm.global_dtors");
  }
  
private:
  gen::Ctx ctx;
  llvm::Module *module;
  std::vector<llvm::Function *> ctors;
  std::vector<llvm::Function *> dtors;
};

}

void stela::generateDecl(gen::Ctx ctx, llvm::Module *module, const ast::Decls &decls) {
  Visitor visitor{ctx, module};
  for (const ast::DeclPtr &decl : decls) {
    decl->accept(visitor);
  }
  visitor.writeCtorList();
}
