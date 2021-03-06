//
//  expr lookup.cpp
//  STELA
//
//  Created by Indi Kernick on 26/11/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "expr lookup.hpp"

#include <cassert>
#include "symbol desc.hpp"
#include "scope lookup.hpp"
#include "scope insert.hpp"
#include "compare types.hpp"
#include "scope traverse.hpp"
#include "builtin symbols.hpp"
#include "Utils/unreachable.hpp"
#include "compare params args.hpp"
#include "Utils/assert down cast.hpp"

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
    sym::Func *funcSym = lookupFun(ctx.man.cur(), key, loc);
    assert(funcSym->ret.type);
    return popCallPushRet(funcSym);
  }
  if (stack.call(ExprKind::func)) {
    sym::Name name = stack.popFunc();
    const FunKey key {name, pushReceiver(sym::null_type, args)};
    sym::Func *funcSym = lookupFun(ctx.man.cur(), key, loc);
    if (!funcSym->ret.type) {
      ctx.log.error(loc) << "Return type of function \"" << name << "\" cannot be deduced" << fatal;
    }
    return popCallPushRet(funcSym);
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
    assert(func->ret);
    stack.pushExpr(convertNullable(ctx, func->ret, ast::ParamRef::val));
    return nullptr;
  }
  UNREACHABLE();
}

void ExprLookup::member(const sym::Name &name) {
  stack.pushMember(name);
}

template <typename FieldType>
void ExprLookup::lookupField(
  FieldType *strut,
  ast::TypePtr type,
  const sym::Name &name,
  ast::MemberIdent &mem
) {
  for (size_t f = 0; f != strut->fields.size(); ++f) {
    const auto &field = strut->fields[f];
    if (field.name == name) {
      stack.pushMemberExpr(field.type);
      mem.index = static_cast<uint32_t>(f);
      mem.exprType = field.type;
      return;
    }
  }
  ctx.log.error(mem.loc) << "No field \"" << name << "\" found in " << typeDesc(type) << fatal;
}

void ExprLookup::lookupMember(ast::MemberIdent &mem) {
  if (stack.memVarExpr(ExprKind::expr)) {
    ast::TypePtr type = stack.popExpr().type;
    ast::TypePtr concType = lookupConcreteType(ctx, type);
    const sym::Name name = stack.popMember();
    if (compareTypes(ctx, concType, ctx.btn.Void)) {
      ctx.log.error(mem.loc) << "Cannot access field \"" << name << "\" of void expression" << fatal;
    }
    if (auto strut = dynamic_pointer_cast<ast::StructType>(concType)) {
      lookupField(strut.get(), std::move(type), name, mem);
    } else if (auto user = dynamic_pointer_cast<ast::UserType>(concType)) {
      lookupField(user.get(), std::move(type), name, mem);
    } else {
      ctx.log.error(mem.loc) << "Cannot access field \"" << name << "\" of " << typeDesc(type) << fatal;
    }
  }
}

namespace {

sym::Scope *findParentClosure(sym::Scope *closureScope) {
  return findNearest(sym::ScopeType::closure, closureScope->parent);
}

uint32_t pushCapture(sym::Lambda *lamSym, sym::ClosureCap cap) {
  const uint32_t captureIndex = static_cast<uint32_t>(lamSym->captures.size());
  lamSym->captures.push_back(std::move(cap));
  return captureIndex;
}

bool lambdaCapture(ast::Identifier &ident, sym::Object *object, sym::Scope *scope, sym::Scope *currentScope) {
  // @TODO this function is huge!
  if (scope->type == sym::ScopeType::ns) {
    // don't capture globals
    return false;
  }
  sym::Scope *closureScope = findNearest(sym::ScopeType::closure, currentScope);
  if (!closureScope) {
    // we aren't in a lambda
    return false;
  }
  if (findNearest(sym::ScopeType::closure, scope) == closureScope) {
    // if we can reach the closure scope from the object scope, object is local
    return false;
  }
  auto *lamSym = assertDownCast<sym::Lambda>(closureScope->symbol);
  for (size_t c = 0; c != lamSym->captures.size(); ++c) {
    if (lamSym->captures[c].object == object->node.get()) {
      // already been captured
      ident.captureIndex = static_cast<uint32_t>(c);
      return true;
    }
  }
  // capture new variable
  sym::Scope *parentClosure = findParentClosure(closureScope);
  if (parentClosure) {
    // nested closure
    if (lambdaCapture(ident, object, scope, closureScope->parent)) {
      // the parent closure has to capture the variable
      sym::Lambda *parentLambda = assertDownCast<sym::Lambda>(parentClosure->symbol);
      for (size_t c = 0; c != parentLambda->captures.size(); ++c) {
        if (parentLambda->captures[c].object == object->node.get()) {
          // this closure captures the captured variable
          sym::ClosureCap cap;
          cap.object = ident.definition;
          // the capture index refers to the captured object in the parent
          cap.index = static_cast<uint32_t>(c);
          cap.type = object->etype.type;
          // the identifier refers to the captured object in this closure
          ident.captureIndex = pushCapture(lamSym, std::move(cap));
          return true;
        }
      }
      // lambdaCapture returns true when an object is captured
      // the parent must have captured the object
      // so we must find the object in the parent
      UNREACHABLE();
    }
  }
  // this closure captures an identifier
  sym::ClosureCap cap;
  cap.object = ident.definition;
  // capture does not refer to a captured object
  cap.index = ~uint32_t{};
  cap.type = object->etype.type;
  // identifier refers to the captured object in this closure
  ident.captureIndex = pushCapture(lamSym, std::move(cap));
  return true;
}

}

void ExprLookup::lookupIdent(ast::Identifier &ident) {
  sym::Scope *currentScope = ctx.man.cur();
  const auto [symbol, scope] = findScope(currentScope, sym::Name{ident.name});
  if (symbol == nullptr) {
    ctx.log.error(ident.loc) << "Use of undefined symbol \"" << ident.name << '"' << fatal;
  }
  if (auto *object = dynamic_cast<sym::Object *>(symbol)) {
    object->referenced = true;
    ident.definition = object->node.get();
    if (lambdaCapture(ident, object, scope, currentScope)) {
      stack.pushExpr(sym::makeVarVal(object->etype.type));
    } else {
      stack.pushExpr(object->etype);
    }
    ident.exprType = object->etype.type;
    return;
  }
  if (dynamic_cast<sym::Func *>(symbol)) {
    if (stack.top() == ExprKind::call) {
      stack.pushFunc(sym::Name{ident.name});
    } else {
      ident.definition = pushFunPtr(scope, sym::Name{ident.name}, ident.loc);
      ident.exprType = stack.topType();
    }
    return;
  }
  if (dynamic_cast<sym::BtnFunc *>(symbol)) {
    if (stack.top() != ExprKind::call) {
      ctx.log.error(ident.loc) << "Reference to builtin function \"" << ident.name
        << "\" must be called" << fatal;
    }
    stack.pushBtnFunc(sym::Name{ident.name});
    return;
  }
  ctx.log.error(ident.loc) << "Expected variable or function but found "
    << symbolDesc(symbol) << " \"" << ident.name << "\"" << fatal;
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

ast::TypePtr ExprLookup::topType() const {
  return stack.topType();
}

void ExprLookup::expected(const ast::TypePtr &type) {
  expType = type;
}

ast::Declaration *ExprLookup::popCallPushRet(sym::Func *const func) {
  stack.popCall();
  stack.pushExpr(func->ret);
  assert(func->node);
  return func->node.get();
}

namespace {

ast::Func *addressOfFunc(sym::Func *symbol, Log &log, Loc loc) {
  ast::Declaration *node = symbol->node.get();
  if (dynamic_cast<ast::ExtFunc *>(node)) {
    log.error(loc) << "Cannot take the address of an external function" << fatal;
  } else if (ast::Func *func = dynamic_cast<ast::Func *>(node)) {
    return func;
  }
  UNREACHABLE();
}

}

ast::Func *ExprLookup::pushFunPtr(sym::Scope *scope, const sym::Name &name, const Loc loc) {
  const auto [begin, end] = scope->table.equal_range(name);
  assert(begin != end);
  if (std::next(begin) == end) {
    sym::Func *funcSym = assertDownCast<sym::Func>(begin->second.get());
    ast::Func *funcNode = addressOfFunc(funcSym, ctx.log, loc);
    ast::TypePtr funcType = getFuncType(ctx.log, *funcNode, loc);
    if (expType && !compareTypes(ctx, expType, funcType)) {
      ctx.log.error(loc) << "Function \"" << name << "\" does not match signature" << fatal;
    }
    funcSym->referenced = true;
    stack.pushExpr(sym::makeLetVal(std::move(funcType)));
    return funcNode;
  } else {
    if (!expType) {
      ctx.log.error(loc) << "Ambiguous reference to overloaded function \"" << name << '"' << fatal;
    }
    for (auto f = begin; f != end; ++f) {
      sym::Func *funcSym = assertDownCast<sym::Func>(f->second.get());
      ast::Func *funcNode = addressOfFunc(funcSym, ctx.log, loc);
      ast::TypePtr funcType = getFuncType(ctx.log, *funcNode, loc);
      if (compareTypes(ctx, expType, funcType)) {
        stack.pushExpr(sym::makeLetVal(std::move(funcType)));
        funcSym->referenced = true;
        return funcNode;
      }
    }
    ctx.log.error(loc) << "No overload of function \"" << name << "\" matches signature" << fatal;
  }
}
