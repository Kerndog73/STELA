//
//  ast.cpp
//  STELA
//
//  Created by Indi Kernick on 9/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "ast.hpp"

#define ACCEPT(TYPE)                                                            \
  void stela::ast::TYPE::accept(Visitor &visitor) {                             \
    visitor.visit(*this);                                                       \
  }

/* LCOV_EXCL_START */

ACCEPT(ArrayType)
ACCEPT(MapType)
ACCEPT(FuncType)
ACCEPT(NamedType)
ACCEPT(NestedType)

ACCEPT(Assignment)
ACCEPT(BinaryExpr)
ACCEPT(UnaryExpr)
ACCEPT(FuncCall)
ACCEPT(MemberIdent)
ACCEPT(Subscript)
ACCEPT(Identifier)
ACCEPT(Self)
ACCEPT(Ternary)

ACCEPT(Block)
ACCEPT(EmptyStatement)
ACCEPT(If)
ACCEPT(Switch)
ACCEPT(Break)
ACCEPT(Continue)
ACCEPT(Return)
ACCEPT(While)
ACCEPT(RepeatWhile)
ACCEPT(For)

ACCEPT(Func)
ACCEPT(Var)
ACCEPT(Let)
ACCEPT(TypeAlias)
ACCEPT(Init)
ACCEPT(Struct)
ACCEPT(Enum)

ACCEPT(StringLiteral)
ACCEPT(CharLiteral)
ACCEPT(NumberLiteral)
ACCEPT(BoolLiteral)
ACCEPT(ArrayLiteral)
ACCEPT(MapLiteral)
ACCEPT(Lambda)

/* LCOV_EXCL_END */

#undef ACCEPT
