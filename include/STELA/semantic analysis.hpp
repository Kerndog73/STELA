//
//  semantic analysis.hpp
//  STELA
//
//  Created by Indi Kernick on 9/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_semantic_analysis_hpp
#define stela_semantic_analysis_hpp

#include "log.hpp"
#include "ast.hpp"
#include "symbols.hpp"

namespace stela {

Symbols createSym(const AST &, LogBuf &);

struct SymbolsAST {
  Symbols sym;
  AST ast;
};

SymbolsAST createSym(std::string_view, LogBuf &);

}

#endif
