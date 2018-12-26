//
//  generate func.hpp
//  STELA
//
//  Created by Indi Kernick on 2/12/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_generate_func_hpp
#define stela_generate_func_hpp

#include "ast.hpp"
#include "symbols.hpp"
#include "gen context.hpp"

namespace llvm {

class Type;
class Function;
class Module;

}

namespace stela {

namespace gen {

class FuncInst;

}

std::string generateNullFunc(gen::Ctx, const ast::FuncType &);
std::string generateMakeFunc(gen::Ctx, ast::FuncType &);
std::string generateLambda(gen::Ctx, const ast::Lambda &);
std::string generateMakeLam(gen::Ctx, const ast::Lambda &);

llvm::Function *generatePanic(llvm::Module *);
llvm::Function *generateAlloc(gen::FuncInst &, llvm::Module *);

llvm::Function *generateArrayDtor(gen::FuncInst &, llvm::Module *, llvm::Type *);
llvm::Function *generateArrayDefCtor(gen::FuncInst &, llvm::Module *, llvm::Type *);
llvm::Function *generateArrayCopCtor(gen::FuncInst &, llvm::Module *, llvm::Type *);
llvm::Function *generateArrayCopAsgn(gen::FuncInst &, llvm::Module *, llvm::Type *);
llvm::Function *generateArrayMovCtor(gen::FuncInst &, llvm::Module *, llvm::Type *);
llvm::Function *generateArrayMovAsgn(gen::FuncInst &, llvm::Module *, llvm::Type *);

}

#endif
