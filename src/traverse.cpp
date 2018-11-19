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
#include "operator name.hpp"
#include "compare types.hpp"
#include "scope traverse.hpp"
#include "builtin symbols.hpp"

using namespace stela;

namespace {

class Visitor final : public ast::Visitor {
public:
  explicit Visitor(sym::Ctx ctx)
    : ctx{ctx} {}
  
  void visitCond(const ast::ExprPtr &expr) {
    assert(expr);
    const sym::ExprType etype = getExprType(ctx, expr, ctx.btn.Bool);
    if (!compareTypes(ctx, etype.type, ctx.btn.Bool)) {
      ctx.log.error(expr->loc) << "Condition expression must be of type Bool" << fatal;
    }
  }
  
  void visit(ast::Block &block) override {
    ctx.man.enterScope(sym::Scope::Type::block);
    for (const ast::StatPtr &stat : block.nodes) {
      stat->accept(*this);
    }
    ctx.man.leaveScope();
  }
  void visit(ast::If &fi) override {
    ctx.man.enterScope(sym::Scope::Type::block);
    visitCond(fi.cond);
    fi.body->accept(*this);
    if (fi.elseBody) {
      fi.elseBody->accept(*this);
    }
    ctx.man.leaveScope();
  }
  void visit(ast::Switch &swich) override {
    const sym::ExprType etype = getExprType(ctx, swich.expr, nullptr);
    bool foundDef = false;
    for (const ast::SwitchCase &cs : swich.cases) {
      if (cs.expr) {
        const sym::ExprType caseType = getExprType(ctx, cs.expr, etype.type);
        if (!compareTypes(ctx, caseType.type, etype.type)) {
          ctx.log.error(cs.loc) << "Case expression type doesn't match type of switch expression" << fatal;
        }
      } else {
        if (foundDef) {
          ctx.log.error(cs.loc) << "Multiple default cases found in switch" << fatal;
        }
        foundDef = true;
      }
      ctx.man.enterScope(sym::Scope::Type::flow);
      cs.body->accept(*this);
      ctx.man.leaveScope();
    }
  }
  void checkFlowKeyword(const std::string_view keyword, const Loc loc) {
    sym::Scope *const flow = findNearestNot(sym::Scope::Type::block, ctx.man.cur());
    if (flow->type != sym::Scope::Type::flow) {
      ctx.log.error(loc) << "Invalid usage of keyword \"" << keyword
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
    if (ret.expr) {
      getExprType(ctx, ret.expr, nullptr);
    }
  }
  void visit(ast::While &wile) override {
    ctx.man.enterScope(sym::Scope::Type::flow);
    visitCond(wile.cond);
    wile.body->accept(*this);
    ctx.man.leaveScope();
  }
  void visit(ast::For &four) override {
    ctx.man.enterScope(sym::Scope::Type::flow);
    four.init->accept(*this);
    visitCond(four.cond);
    four.incr->accept(*this);
    four.body->accept(*this);
    ctx.man.leaveScope();
  }

  void visit(ast::Func &func) override {
    sym::Func *const funcSym = insert(ctx, func);
    funcSym->scope = ctx.man.enterScope(sym::Scope::Type::func);
    enterFuncScope(funcSym, func);
    for (const ast::StatPtr &stat : func.body.nodes) {
      stat->accept(*this);
    }
    ctx.man.leaveScope();
  }
  ast::TypePtr objectType(
    const ast::TypePtr &type,
    const ast::ExprPtr &expr,
    const Loc loc
  ) {
    if (type) {
      validateType(ctx, type);
    }
    sym::ExprType exprType = expr ? getExprType(ctx, expr, type) : sym::null_type;
    if (exprType.type == sym::void_type.type) {
      ctx.log.error(loc) << "Cannot initialize variable with void expression" << fatal;
    }
    if (type && expr && !compareTypes(ctx, type, exprType.type)) {
      ctx.log.error(loc) << "Expression and declaration type do not match" << fatal;
    }
    if (type) {
      return type;
    } else {
      return exprType.type;
    }
  }
  void visit(ast::Var &var) override {
    sym::ExprType etype = sym::makeVarVal(objectType(var.type, var.expr, var.loc));
    auto *varSym = insert<sym::Object>(ctx, var);
    varSym->etype = std::move(etype);
  }
  void visit(ast::Let &let) override {
    sym::ExprType etype = sym::makeLetVal(objectType(let.type, let.expr, let.loc));
    auto *letSym = insert<sym::Object>(ctx, let);
    letSym->etype = std::move(etype);
  }
  void visit(ast::TypeAlias &alias) override {
    auto *aliasSym = insert<sym::TypeAlias>(ctx, alias);
    aliasSym->node = {retain, &alias};
    validateType(ctx, alias.type);
  }
  
  void visit(ast::CompAssign &as) override {
    const sym::ExprType left = getExprType(ctx, as.left, nullptr);
    const sym::ExprType right = getExprType(ctx, as.right, left.type);
    if (auto builtinLeft = lookupConcrete<ast::BtnType>(ctx, left.type)) {
      if (auto builtinRight = lookupConcrete<ast::BtnType>(ctx, right.type)) {
        if (validOp(as.oper, builtinLeft, builtinRight)) {
          if (left.mut == sym::ValueMut::var) {
            return;
          } else {
            ctx.log.error(as.loc) << "Left side of compound assignment must be mutable" << fatal;
          }
        }
      }
    }
    ctx.log.error(as.loc) << "Invalid operands to compound assignment operator " << opName(as.oper) << fatal;
  }
  void visit(ast::IncrDecr &as) override {
    const sym::ExprType etype = getExprType(ctx, as.expr, nullptr);
    if (auto builtin = lookupConcrete<ast::BtnType>(ctx, etype.type)) {
      if (validIncr(builtin)) {
        return;
      }
    }
    ctx.log.error(as.loc) << "Invalid operand to unary operator " << (as.incr ? "++" : "--") << fatal;
  }
  void visit(ast::Assign &as) override {
    const sym::ExprType left = getExprType(ctx, as.left, nullptr);
    const sym::ExprType right = getExprType(ctx, as.right, left.type);
    if (!compareTypes(ctx, left.type, right.type)) {
      ctx.log.error(as.loc) << "Assignment types do not match" << fatal;
    }
    if (left.mut == sym::ValueMut::let) {
      ctx.log.error(as.loc) << "Left side of assignment must be mutable" << fatal;
    }
  }
  void visit(ast::DeclAssign &as) override {
    sym::ExprType etype = sym::makeVarVal(objectType(nullptr, as.expr, as.loc));
    auto *varSym = insert<sym::Object>(ctx, as);
    varSym->etype = etype;
  }
  void visit(ast::CallAssign &as) override {
    const sym::ExprType etype = getExprType(ctx, {retain, &as.call}, nullptr);
    if (etype.type != sym::void_type.type) {
      ctx.log.error(as.loc) << "Discarded return value" << fatal;
    }
  }

private:
  sym::Ctx ctx;
};

}

void stela::traverse(sym::Ctx ctx, sym::Module &module) {
  Visitor visitor{ctx};
  for (const ast::DeclPtr &decl : module.decls) {
    decl->accept(visitor);
  }
}
