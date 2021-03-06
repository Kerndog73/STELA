//
//  expr stack.hpp
//  STELA
//
//  Created by Indi Kernick on 26/11/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_expr_stack_hpp
#define stela_expr_stack_hpp

#include "symbols.hpp"

namespace stela {

enum class ExprKind {
  call,
  member,
  func,
  btn_func,
  expr,
  subexpr
};

class ExprStack {
public:
  void pushCall();
  void pushExpr(const sym::ExprType &);
  void pushMember(const sym::Name &);
  void pushMemberExpr(const ast::TypePtr &);
  void pushFunc(const sym::Name &);
  void pushBtnFunc(const sym::Name &);
  
  ExprKind top() const;
  /// top == kind && below top == member && below below top != call
  bool memVarExpr(ExprKind) const;
  /// top == kind && below top == member && below below top == call
  bool memFunExpr(ExprKind) const;
  /// top == kind && below top == call
  bool call(ExprKind) const;
  
  void popCall();
  sym::ExprType popExpr();
  sym::Name popMember();
  sym::Name popFunc();
  sym::Name popBtnFunc();
  
  void setExpr(sym::ExprType);
  void enterSubExpr();
  sym::ExprType leaveSubExpr();
  ast::TypePtr topType() const;
  
private:
  struct Expr {
    ExprKind kind;
    sym::Name name;
    
    explicit Expr(ExprKind);
    Expr(ExprKind, const sym::Name &);
  };
  
  std::vector<Expr> exprs;
  sym::ExprType exprType = sym::null_type;
  
  void pop(ExprKind);
  sym::Name popName(ExprKind);
};

}

#endif
