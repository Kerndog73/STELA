//
//  semantic analysis.cpp
//  STELA
//
//  Created by Indi Kernick on 9/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "semantic analysis.hpp"

#include "traverse.hpp"
#include "log output.hpp"
#include "builtin symbols.hpp"
#include "syntax analysis.hpp"

stela::Symbols stela::createSym(AST &ast, LogBuf &buf) {
  Log log{buf, LogCat::semantic};
  Symbols syms;
  Builtins types = makeBuiltins(syms.scopes, ast.builtin);
  auto global = std::make_unique<sym::Scope>(syms.scopes.back().get(), sym::Scope::Type::ns);
  syms.scopes.push_back(std::move(global));
  traverse(syms.scopes, ast, log, types);
  return syms;
}

stela::SymbolsAST stela::createSym(const std::string_view source, LogBuf &buf) {
  SymbolsAST sa;
  sa.ast = createAST(source, buf);
  sa.sym = createSym(sa.ast, buf);
  return sa;
}
