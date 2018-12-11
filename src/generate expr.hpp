//
//  generate expr.hpp
//  STELA
//
//  Created by Indi Kernick on 1/12/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_generate_expr_hpp
#define stela_generate_expr_hpp

#include "ast.hpp"
#include "gen context.hpp"
#include <llvm/Ir/irbuilder.h>

namespace stela {

llvm::Value *generateExpr(gen::Ctx, llvm::IRBuilder<> &, ast::Expression *);

}

#endif
