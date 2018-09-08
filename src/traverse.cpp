//
//  traverse.cpp
//  STELA
//
//  Created by Indi Kernick on 10/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "traverse.hpp"

#include "infer type.hpp"
#include "scope insert.hpp"
#include "scope lookup.hpp"
#include "compare types.hpp"
#include "scope manager.hpp"
#include "scope traverse.hpp"

using namespace stela;

namespace {

class Visitor final : public ast::Visitor {
public:
  Visitor(sym::Scopes &scopes, Log &log, const BuiltinTypes &bnt)
    : man{scopes}, log{log}, ins{man.global(), log}, tlk{man.cur(), log}, bnt{bnt} {}
  
  void visitExpr(const ast::ExprPtr &expr) {
    if (expr) {
      getExprType(man, log, bnt, expr.get());
    }
  }
  void visitCond(const ast::ExprPtr &expr) {
    assert(expr);
    const sym::ExprType etype = getExprType(man, log, bnt, expr.get());
    if (!compareTypes(tlk, etype.type, bnt.Bool)) {
      log.error(expr->loc) << "Condition expression must be of type Bool" << fatal;
    }
  }
  
  void visit(ast::Block &block) override {
    ins.push(man.enterScope(sym::Scope::Type::block));
    for (const ast::StatPtr &stat : block.nodes) {
      stat->accept(*this);
    }
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::If &fi) override {
    ins.push(man.enterScope(sym::Scope::Type::block));
    visitCond(fi.cond);
    fi.body->accept(*this);
    if (fi.elseBody) {
      fi.elseBody->accept(*this);
    }
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::Switch &swich) override {
    const sym::ExprType etype = getExprType(man, log, bnt, swich.expr.get());
    bool foundDef = false;
    for (const ast::SwitchCase &cs : swich.cases) {
      if (cs.expr) {
        const sym::ExprType caseType = getExprType(man, log, bnt, cs.expr.get());
        if (!compareTypes(tlk, caseType.type, etype.type)) {
          log.error(cs.loc) << "Case expression type doesn't match type of switch expression" << fatal;
        }
      } else {
        if (foundDef) {
          log.error(cs.loc) << "Multiple default cases found in switch" << fatal;
        }
        foundDef = true;
      }
      ins.push(man.enterScope(sym::Scope::Type::flow));
      cs.body->accept(*this);
      ins.pop();
      man.leaveScope();
    }
  }
  void checkFlowKeyword(const std::string_view keyword, const Loc loc) {
    sym::Scope *const flow = findNearestNot(sym::Scope::Type::block, man.cur());
    if (flow->type != sym::Scope::Type::flow) {
      log.error(loc) << "Invalid usage of keyword \"" << keyword
        << "\" outside of loop or switch" << fatal;
    }
  }
  void visit(ast::Break &brake) override {
    checkFlowKeyword("break", brake.loc);
  }
  void visit(ast::Continue &continu) override {
    checkFlowKeyword("continue", continu.loc);
  }
  void visit(ast::Return &ret) override {
    visitExpr(ret.expr);
  }
  void visit(ast::While &wile) override {
    ins.push(man.enterScope(sym::Scope::Type::flow));
    visitCond(wile.cond);
    wile.body->accept(*this);
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::For &four) override {
    ins.push(man.enterScope(sym::Scope::Type::flow));
    four.init->accept(*this);
    visitCond(four.cond);
    four.incr->accept(*this);
    four.body->accept(*this);
    ins.pop();
    man.leaveScope();
  }

  void visit(ast::Func &func) override {
    sym::Func *const funcSym = ins.insert(func);
    funcSym->scope = man.enterScope(sym::Scope::Type::func);
    ins.enterFuncScope(funcSym, func);
    ins.push(funcSym->scope);
    for (const ast::StatPtr &stat : func.body.nodes) {
      stat->accept(*this);
    }
    ins.pop();
    man.leaveScope();
  }
  ast::Type *objectType(
    const ast::TypePtr &type,
    const ast::ExprPtr &expr,
    const Loc loc
  ) {
    const sym::ExprType exprType = expr ? getExprType(man, log, bnt, expr.get()) : sym::null_type;
    if (type != nullptr && exprType.type != nullptr && !compareTypes(tlk, type.get(), exprType.type)) {
      log.error(loc) << "Expression and declaration type do not match" << fatal;
    }
    if (type) {
      return type.get();
    } else {
      return exprType.type;
    }
  }
  void visit(ast::Var &var) override {
    sym::ExprType etype;
    etype.type = objectType(var.type, var.expr, var.loc);
    etype.mut = sym::ValueMut::var;
    etype.ref = sym::ValueRef::val;
    auto *varSym = ins.insert<sym::Object>(var);
    varSym->etype = etype;
  }
  void visit(ast::Let &let) override {
    sym::ExprType etype;
    etype.type = objectType(let.type, let.expr, let.loc);
    etype.mut = sym::ValueMut::let;
    etype.ref = sym::ValueRef::val;
    auto *letSym = ins.insert<sym::Object>(let);
    letSym->etype = etype;
  }
  void visit(ast::TypeAlias &alias) override {
    auto *aliasSym = ins.insert<sym::TypeAlias>(alias);
    aliasSym->node = &alias;
    tlk.lookupConcreteType(alias.type.get());
  }
  
  void visit(ast::CompAssign &as) override {
    const sym::ExprType left = getExprType(man, log, bnt, as.left.get());
    const sym::ExprType right = getExprType(man, log, bnt, as.right.get());
    //lkp.lookupFunc(sym::Name(opName(as.oper)), {left, right}, as.loc);
  }
  void visit(ast::IncrDecr &as) override {
    //lkp.lookupFunc(sym::Name(as.incr ? "a++" : "a--"), {left, right}, as.loc);
  }
  void visit(ast::Assign &as) override {
    const sym::ExprType left = getExprType(man, log, bnt, as.left.get());
    const sym::ExprType right = getExprType(man, log, bnt, as.right.get());
    if (!compareTypes(tlk, left.type, right.type)) {
      log.error(as.loc) << "Assignment types do not match" << fatal;
    }
  }
  void visit(ast::DeclAssign &as) override {
    sym::ExprType etype;
    etype.type = objectType(nullptr, as.expr, as.loc);
    etype.mut = sym::ValueMut::var;
    etype.ref = sym::ValueRef::val;
    auto *varSym = ins.insert<sym::Object>(as);
    varSym->etype = etype;
  }
  void visit(ast::CallAssign &as) override {
    const sym::ExprType etype = getExprType(man, log, bnt, &as.call);
    if (etype.type) {
      log.error(as.loc) << "Discarded return value" << fatal;
    }
  }

private:
  ScopeMan man;
  Log &log;
  InserterManager ins;
  NameLookup tlk;
  const BuiltinTypes &bnt;
};

}

void stela::traverse(sym::Scopes &scopes, const AST &ast, Log &log, const BuiltinTypes &types) {
  Visitor visitor{scopes, log, types};
  for (const ast::DeclPtr &decl : ast.global) {
    decl->accept(visitor);
  }
}
