//
//  parse expr.hpp
//  STELA
//
//  Created by Indi Kernick on 1/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef parse_expr_hpp
#define parse_expr_hpp

#include "ast.hpp"
#include "parse tokens.hpp"

namespace stela {

// parse an expression that is probably an lvalue
// appropriate for left-hand side of assignment
// a is an lvalue
// a.b is an lvalue
// a[b] is an lvalue
// a() might be an lvalue
// a.b() might be an lvalue
// doesn't return expressions that cannot be an lvalue
// a + b cannot be an lvalue for example
ast::ExprPtr parseNested(ParseTokens &);
ast::ExprPtr parseExpr(ParseTokens &);

}

#endif
