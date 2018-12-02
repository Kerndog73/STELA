//
//  expr lookup.cpp
//  STELA
//
//  Created by Indi Kernick on 26/11/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "expr lookup.hpp"

#include <cassert>
#include "unreachable.hpp"
#include "symbol desc.hpp"
#include "scope lookup.hpp"
#include "scope insert.hpp"
#include "compare types.hpp"
#include "scope traverse.hpp"
#include "builtin symbols.hpp"
#include "assert down cast.hpp"
#include "compare params args.hpp"

using namespace stela;

ExprLookup::ExprLookup(sym::Ctx ctx)
  : ctx{ctx} {}

void ExprLookup::call() {
  stack.pushCall();
}

sym::Func *ExprLookup::lookupFun(
  sym::Scope *scope,
  const FunKey &key,
  const Loc loc
) {
  const auto [begin, end] = scope->table.equal_range(key.name);
  if (begin == end) {
    if (sym::Scope *parent = scope->parent) {
      return lookupFun(parent, key, loc);
    } else {
      ctx.log.error(loc) << "Use of undefined symbol \"" << key.name << '"' << fatal;
    }
  } else {
    for (auto s = begin; s != end; ++s) {
      sym::Symbol *const symbol = s->second.get();
      auto *func = dynamic_cast<sym::Func *>(symbol);
      if (func == nullptr) {
        ctx.log.error(loc) << "Calling \"" << key.name
          << "\" declarared at " << moduleName(ctx.man.cur()) << ':'
          << symbol->loc << " but it is not a function" << fatal;
      }
      if (compatParams(ctx, func->params, key.args)) {
        func->referenced = true;
        return func;
      }
    }
    ctx.log.error(loc) << "No matching call to function \"" << key.name << '"' << fatal;
  }
}

namespace {

sym::FuncParams pushReceiver(const sym::ExprType &receiver, const sym::FuncParams &args) {
  sym::FuncParams recArgs;
  recArgs.reserve(1 + args.size());
  recArgs.push_back(receiver);
  recArgs.insert(recArgs.end(), args.cbegin(), args.cend());
  return recArgs;
}

}

ast::Declaration *ExprLookup::lookupFunc(const sym::FuncParams &args, const Loc loc) {
  if (stack.memFunExpr(ExprKind::expr)) {
    sym::ExprType receiver = stack.popExpr();
    sym::Name name = stack.popMember();
    const FunKey key {name, pushReceiver(receiver, args)};
    return popCallPushRet(lookupFun(ctx.man.cur(), key, loc));
  }
  if (stack.call(ExprKind::func)) {
    sym::Name name = stack.popFunc();
    const FunKey key {name, pushReceiver(sym::null_type, args)};
    return popCallPushRet(lookupFun(ctx.man.cur(), key, loc));
  }
  if (stack.call(ExprKind::btn_func)) {
    sym::Name name = stack.popBtnFunc();
    sym::Symbol *symbol = findScope(ctx.man.cur(), name).symbol;
    sym::BtnFunc *func = assertDownCast<sym::BtnFunc>(symbol);
    stack.popCall();
    assert(func->node);
    ast::TypePtr retType = callBtnFunc(ctx, func->node->value, args, loc);
    stack.pushExpr(sym::makeLetVal(std::move(retType)));
    return func->node.get();
  }
  if (stack.call(ExprKind::expr)) {
    sym::ExprType expr = stack.popExpr();
    stack.popCall();
    auto func = dynamic_pointer_cast<ast::FuncType>(std::move(expr.type));
    if (!func) {
      ctx.log.error(loc) << "Calling an expression but it is not a function object" << fatal;
    }
    sym::FuncParams params;
    for (const ast::ParamType &param : func->params) {
      params.push_back(convert(ctx, param.type, param.ref));
    }
    if (!compatParams(ctx, params, args)) {
      ctx.log.error(loc) << "No matching call to function object" << fatal;
    }
    stack.pushExpr(convertNullable(ctx, func->ret, ast::ParamRef::value));
    return nullptr;
  }
  UNREACHABLE();
}

void ExprLookup::member(const sym::Name &name) {
  stack.pushMember(name);
}

uint32_t ExprLookup::lookupMember(const Loc loc) {
  if (stack.memVarExpr(ExprKind::expr)) {
    ast::TypePtr type = stack.popExpr().type;
    ast::TypePtr concType = lookupConcreteType(ctx, type);
    const sym::Name name = stack.popMember();
    if (compareTypes(ctx, concType, ctx.btn.Void)) {
      ctx.log.error(loc) << "Cannot access field \"" << name << "\" of void expression" << fatal;
    }
    if (auto strut = dynamic_pointer_cast<ast::StructType>(concType)) {
      for (size_t f = 0; f != strut->fields.size(); ++f) {
        const ast::Field &field = strut->fields[f];
        if (field.name == name) {
          stack.pushMemberExpr(field.type);
          return static_cast<uint32_t>(f);
        }
      }
      ctx.log.error(loc) << "No field \"" << name << "\" found in " << typeDesc(type) << fatal;
    } else {
      ctx.log.error(loc) << "Cannot access field \"" << name << "\" of " << typeDesc(type) << fatal;
    }
  }
  return ~uint32_t{};
}

ast::Statement *ExprLookup::lookupIdent(const sym::Name &name, const Loc loc) {
  sym::Scope *currentScope = ctx.man.cur();
  const auto [symbol, scope] = findScope(currentScope, name);
  if (symbol == nullptr) {
    ctx.log.error(loc) << "Use of undefined symbol \"" << name << '"' << fatal;
  }
  if (auto *object = dynamic_cast<sym::Object *>(symbol)) {
    bool inClosure = false;
    // don't capture globals
    if (scope->type != sym::ScopeType::ns) {
      // make sure we're inside a closure
      if (sym::Scope *closureScope = findNearest(sym::ScopeType::closure, currentScope)) {
        auto *lamSym = assertDownCast<sym::Lambda>(closureScope->symbol);
        // make sure the variable hasn't been captured already
        bool alreadyCaptured = false;
        for (sym::Object *capture : lamSym->captures) {
          if (capture == object) {
            alreadyCaptured = true;
          }
        }
        if (!alreadyCaptured) {
          lamSym->captures.push_back(object);
        }
        inClosure = true;
      }
    }
    object->referenced = true;
    if (inClosure) {
      stack.pushExpr(sym::makeVarVal(object->etype.type));
    } else {
      stack.pushExpr(object->etype);
    }
    return object->node.get();
  }
  if (dynamic_cast<sym::Func *>(symbol)) {
    if (stack.top() == ExprKind::call) {
      stack.pushFunc(name);
      return nullptr;
    } else {
      return pushFunPtr(scope, name, loc);
    }
  }
  if (dynamic_cast<sym::BtnFunc *>(symbol)) {
    if (stack.top() != ExprKind::call) {
      ctx.log.error(loc) << "Reference to builtin function \"" << name << "\" must be called" << fatal;
    }
    stack.pushBtnFunc(name);
    return nullptr;
  }
  ctx.log.error(loc) << "Expected variable or function but found "
    << symbolDesc(symbol) << " \"" << name << "\"" << fatal;
}

void ExprLookup::setExpr(sym::ExprType type) {
  stack.setExpr(std::move(type));
}

void ExprLookup::enterSubExpr() {
  stack.enterSubExpr();
}

sym::ExprType ExprLookup::leaveSubExpr() {
  return stack.leaveSubExpr();
}

void ExprLookup::expected(const ast::TypePtr &type) {
  expType = type;
}

ast::Func *ExprLookup::popCallPushRet(sym::Func *const func) {
  stack.popCall();
  stack.pushExpr(func->ret);
  assert(func->node);
  return func->node.get();
}

ast::Func *ExprLookup::pushFunPtr(sym::Scope *scope, const sym::Name &name, const Loc loc) {
  const auto [begin, end] = scope->table.equal_range(name);
  assert(begin != end);
  if (std::next(begin) == end) {
    auto *funcSym = assertDownCast<sym::Func>(begin->second.get());
    auto funcType = getFuncType(ctx.log, *funcSym->node, loc);
    if (expType && !compareTypes(ctx, expType, funcType)) {
      ctx.log.error(loc) << "Function \"" << name << "\" does not match signature" << fatal;
    }
    stack.pushExpr(sym::makeLetVal(std::move(funcType)));
    return funcSym->node.get();
  } else {
    if (!expType) {
      ctx.log.error(loc) << "Ambiguous reference to overloaded function \"" << name << '"' << fatal;
    }
    for (auto f = begin; f != end; ++f) {
      auto *funcSym = assertDownCast<sym::Func>(f->second.get());
      auto funcType = getFuncType(ctx.log, *funcSym->node, loc);
      if (compareTypes(ctx, expType, funcType)) {
        stack.pushExpr(sym::makeLetVal(std::move(funcType)));
        return funcSym->node.get();
      }
    }
    ctx.log.error(loc) << "No overload of function \"" << name << "\" matches signature" << fatal;
  }
}
