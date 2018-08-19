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
#include "scope manager.hpp"

using namespace stela;

namespace {

class Visitor final : public ast::Visitor {
public:
  Visitor(sym::Scopes &scopes, Log &log, const BuiltinTypes &bnt)
    : man{scopes}, log{log}, ins{man.global(), log, bnt}, tlk{man, log}, bnt{bnt} {}
  
  void visitStat(const ast::StatPtr &stat) {
    if (ast::Expression *expr = dynamic_cast<ast::Expression *>(stat.get())) {
      getExprType(man, log, bnt, expr);
    } else if (stat) {
      stat->accept(*this);
    }
  }
  void visitExpr(const ast::ExprPtr &expr) {
    if (expr) {
      getExprType(man, log, bnt, expr.get());
    }
  }
  void visitCond(const ast::ExprPtr &expr) {
    assert(expr);
    const sym::ExprType etype = getExprType(man, log, bnt, expr.get());
    if (etype.type != bnt.Bool) {
      log.error(expr->loc) << "Condition expression must be of type Bool" << fatal;
    }
  }
  
  void visit(ast::Block &block) override {
    ins.push<BlockInserter>(man.enterScope<sym::BlockScope>(), log);
    for (const ast::StatPtr &stat : block.nodes) {
      visitStat(stat);
    }
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::If &fi) override {
    ins.push<BlockInserter>(man.enterScope<sym::BlockScope>(), log);
    visitCond(fi.cond);
    visitStat(fi.body);
    visitStat(fi.elseBody);
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::Switch &swich) override {
    const sym::ExprType etype = getExprType(man, log, bnt, swich.expr.get());
    bool foundDef = false;
    for (const ast::SwitchCase &cs : swich.cases) {
      if (cs.expr) {
        const sym::ExprType caseType = getExprType(man, log, bnt, cs.expr.get());
        if (caseType.type != etype.type) {
          log.error(cs.loc) << "Case expression type doesn't match type of switch expression" << fatal;
        }
      } else {
        if (foundDef) {
          log.error(cs.loc) << "Multiple default cases found in switch" << fatal;
        }
        foundDef = true;
      }
      ins.push<BlockInserter>(man.enterScope<sym::BlockScope>(), log);
      visitStat(cs.body);
      ins.pop();
      man.leaveScope();
    }
  }
  /*void visit(ast::Break &brake) override {
    
  }
  void visit(ast::Continue &continu) override {
    
  }
  */
  void visit(ast::Return &ret) override {
    visitExpr(ret.expr);
  }
  void visit(ast::While &wile) override {
    ins.push<BlockInserter>(man.enterScope<sym::BlockScope>(), log);
    visitCond(wile.cond);
    visitStat(wile.body);
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::RepeatWhile &repWhile) override {
    ins.push<BlockInserter>(man.enterScope<sym::BlockScope>(), log);
    visitStat(repWhile.body);
    visitCond(repWhile.cond);
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::For &four) override {
    ins.push<BlockInserter>(man.enterScope<sym::BlockScope>(), log);
    visitStat(four.init);
    visitCond(four.cond);
    visitExpr(four.incr);
    visitStat(four.body);
    ins.pop();
    man.leaveScope();
  }

  void visit(ast::Func &func) override {
    sym::Func *const funcSym = ins.insert(func);
    funcSym->scope = man.enterScope<sym::FuncScope>();
    ins.enterFuncScope(funcSym, func);
    ins.push<FuncInserter>(funcSym->scope, log);
    for (const ast::StatPtr &stat : func.body.nodes) {
      visitStat(stat);
    }
    ins.pop();
    man.leaveScope();
  }
  sym::Symbol *objectType(
    const ast::TypePtr &type,
    const ast::ExprPtr &expr,
    const Loc loc
  ) {
    const sym::ExprType exprType = expr ? getExprType(man, log, bnt, expr.get()) : sym::ExprType{};
    sym::Symbol *const symType = type ? tlk.lookupType(type) : exprType.type;
    if (exprType.type != nullptr && exprType.type != symType) {
      log.error(loc) << "Expression and declaration type do not match" << fatal;
    }
    return symType;
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
    aliasSym->type = tlk.lookupType(alias.type);
  }
  
  void visit(ast::Init &init) override {
    auto *strutIns = dynamic_cast<StructInserter *>(ins.get());
    if (strutIns == nullptr) {
      log.error(init.loc) << "Init function must be member of a struct" << fatal;
    }
    sym::Func *const funcSym = strutIns->insert(init);
    funcSym->scope = man.enterScope<sym::FuncScope>();
    strutIns->enterFuncScope(funcSym, init);
    ins.push<FuncInserter>(funcSym->scope, log);
    for (const ast::StatPtr &stat : init.body.nodes) {
      visitStat(stat);
    }
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::Struct &strut) override {
    auto *structSym = ins.insert<sym::StructType>(strut);
    structSym->scope = man.enterScope<sym::StructScope>();
    StructInserter *inserter = ins.push<StructInserter>(structSym, log);
    for (const ast::Member &mem : strut.body) {
      inserter->accessScope(mem);
      mem.node->accept(*this);
    }
    ins.pop();
    man.leaveScope();
  }
  void visit(ast::Enum &num) override {
    auto *enumSym = ins.insert<sym::EnumType>(num);
    enumSym->scope = man.enterScope<sym::EnumScope>();
    EnumInserter inserter{enumSym, log};
    for (const ast::EnumCase &cs : num.cases) {
      if (cs.value) {
        const sym::ExprType type = getExprType(man, log, bnt, cs.value.get());
        if (type.type != bnt.Int64 && type.type != enumSym) {
          log.error(cs.loc) << "Enum case \"" << cs.name
            << "\" must have value of Int or another case" << fatal;
        }
      }
      inserter.insert(cs);
    }
    man.leaveScope();
  }

private:
  ScopeMan man;
  Log &log;
  InserterManager ins;
  TypeLookup tlk;
  const BuiltinTypes &bnt;
};

}

void stela::traverse(sym::Scopes &scopes, const AST &ast, Log &log, const BuiltinTypes &types) {
  Visitor visitor{scopes, log, types};
  for (const ast::DeclPtr &decl : ast.global) {
    decl->accept(visitor);
  }
}
