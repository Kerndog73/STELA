//
//  scope lookup.cpp
//  STELA
//
//  Created by Indi Kernick on 30/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "scope lookup.hpp"

#include <cassert>
#include "scope traverse.hpp"
#include "compare params args.hpp"
#include <Simpleton/Utils/algorithm.hpp>

using namespace stela;

namespace {

template <typename Symbol>
Symbol *referTo(Symbol *const symbol) {
  symbol->referenced = true;
  return symbol;
}

sym::Symbol *find(sym::Scope *scope, const sym::Name name) {
  const auto iter = scope->table.find(name);
  if (iter == scope->table.end()) {
    return nullptr;
  } else {
    return iter->second.get();
  }
}

sym::Scope *parentScope(sym::Scope *const scope) {
  if (scope->type == sym::Scope::Type::func) {
    return findNearest(sym::Scope::Type::ns, scope->parent);
  } else {
    return scope->parent;
  }
}

}

NameLookup::NameLookup(ScopeMan &man, Log &log)
  : man{man}, log{log} {}

ast::TypeAlias *NameLookup::lookupType(ast::NamedType &type) const {
  if (type.definition) {
    return type.definition;
  } else {
    return lookupType(man.cur(), type);
  }
}

ast::TypeAlias *NameLookup::lookupType(ast::NamespacedType &type) const {
  return nullptr;
}

ast::Type *NameLookup::lookupConcreteType(ast::Type *type) const {
  if (auto *named = dynamic_cast<ast::NamedType *>(type)) {
    ast::TypeAlias *alias = lookupType(*named);
    return lookupConcreteType(alias->type.get());
  } else if (auto *namespaced = dynamic_cast<ast::NamespacedType *>(type)) {
    ast::TypeAlias *alias = lookupType(*namespaced);
    return lookupConcreteType(alias->type.get());
  } else {
    return type;
  }
}

ast::Type *NameLookup::validateType(ast::Type *type) const {
  ast::Type *concrete = lookupConcreteType(type);
  if (auto *strut = dynamic_cast<ast::StructType *>(type)) {
    validateStruct(strut);
  }
  return concrete;
}

ast::TypeAlias *NameLookup::lookupType(sym::Scope *scope, ast::NamedType &type) const {
  if (sym::Symbol *symbol = find(scope, sym::Name(type.name))) {
    if (auto *alias = dynamic_cast<sym::TypeAlias *>(symbol)) {
      type.definition = alias->node;
      alias->referenced = true;
      return type.definition;
    }
    log.error(type.loc) << "The name \"" << type.name << "\" does not refer to a type" << fatal;
  }
  if (sym::Scope *parent = parentScope(scope)) {
    return lookupType(parent, type);
  } else {
    log.error(type.loc) << "Expected type name but found \"" << type.name << "\"" << fatal;
  }
}

void NameLookup::validateStruct(ast::StructType *strut) const {
  std::vector<std::pair<std::string_view, Loc>> names;
  names.reserve(strut->fields.size());
  for (const ast::Field &field : strut->fields) {
    names.push_back({field.name, field.loc});
  }
  Utils::sort(names, [] (auto a, auto b) {
    return a.first < b.first;
  });
  const auto dup = Utils::adjacent_find(names, [] (auto a, auto b) {
    return a.first == b.first;
  });
  if (dup != names.cend()) {
    log.error(dup->second) << "Duplicate field \"" << dup->first << "\" in struct" << fatal;
  }
}

ExprLookup::ExprLookup(ScopeMan &man, Log &log)
  : man{man}, log{log}, etype{sym::null_type} {}

void ExprLookup::call() {
  exprs.push_back({Expr::Type::call});
}

sym::Func *ExprLookup::lookupFun(
  sym::Scope *scope,
  const FunKey &key,
  const Loc loc
) {
  const auto [begin, end] = scope->table.equal_range(key.name);
  if (begin == end) {
    if (sym::Scope *parent = parentScope(scope)) {
      return lookupFun(parent, key, loc);
    } else {
      log.error(loc) << "Use of undefined symbol \"" << key.name << '"' << fatal;
    }
  } else {
    for (auto s = begin; s != end; ++s) {
      sym::Symbol *const symbol = s->second.get();
      auto *func = dynamic_cast<sym::Func *>(symbol);
      if (func == nullptr) {
        log.error(loc) << "Calling \"" << key.name
          << "\" but it is not a function. " << symbol->loc << fatal;
      }
      if (compatParams(NameLookup{man, log}, func->params, key.params)) {
        return referTo(func);
      }
    }
    log.error(loc) << "No matching call to function \"" << key.name << '"' << fatal;
  }
}

ExprLookup::FunKey ExprLookup::funKey(const sym::ExprType etype, const sym::FuncParams &params) {
  FunKey key;
  key.params.reserve(1 + params.size());
  key.params.push_back(etype);
  key.params.insert(++key.params.begin(), params.cbegin(), params.cend());
  key.name = popName();
  return key;
}

ast::Func *ExprLookup::lookupFunc(const sym::FuncParams &params, const Loc loc) {
  if (memFunExpr(Expr::Type::expr)) {
    return popCallPushRet(lookupFun(man.cur(), funKey(popExpr(), params), loc));
  }
  if (call(Expr::Type::free_fun)) {
    return popCallPushRet(lookupFun(man.cur(), funKey(sym::null_type, params), loc));
  }
  if (call(Expr::Type::expr)) {
    log.error(loc) << "Calling an object. Might be a function pointer. Who knows!" << fatal;
  }
  log.error(loc) << "Function call operator applied to invalid subject" << fatal;
}

void ExprLookup::member(const sym::Name &name) {
  exprs.push_back({Expr::Type::member, name});
}

ast::Field *ExprLookup::lookupMember(const Loc loc) {
  if (memVarExpr(Expr::Type::expr)) {
    NameLookup lkp{man, log};
    ast::Type *type = lkp.lookupConcreteType(popExpr().type);
    const sym::Name name = popName();
    if (type == nullptr) {
      log.error(loc) << "Cannot access field \"" << name << "\" of void expression" << fatal;
    }
    if (auto *strut = dynamic_cast<ast::StructType *>(type)) {
      for (ast::Field &field : strut->fields) {
        if (field.name == name) {
          pushExpr(memberType(etype, field.type.get()));
          return &field;
        }
      }
      log.error(loc) << "No field \"" << name << "\" found in struct" << fatal;
    } else {
      log.error(loc) << "Cannot access field \"" << name << "\" of non-struct expression" << fatal;
    }
  }
  return nullptr;
}

sym::Symbol *ExprLookup::lookupIdent(
  sym::Scope *scope,
  const sym::Name &name,
  const Loc loc
) {
  sym::Symbol *const symbol = find(scope, name);
  if (symbol) {
    return symbol;
  }
  if (sym::Scope *parent = parentScope(scope)) {
    return lookupIdent(parent, name, loc);
  } else {
    log.error(loc) << "Use of undefined symbol \"" << name << '"' << fatal;
  }
}

ast::Statement *ExprLookup::lookupIdent(const sym::Name &name, const Loc loc) {
  sym::Symbol *symbol = lookupIdent(man.cur(), name, loc);
  if (auto *func = dynamic_cast<sym::Func *>(symbol)) {
    if (exprs.back().type != Expr::Type::call) {
      log.error(loc) << "Reference to function \"" << name << "\" must be called" << fatal;
    }
    exprs.push_back({Expr::Type::free_fun, name});
    return nullptr;
  }
  if (auto *object = dynamic_cast<sym::Object *>(symbol)) {
    return pushObj(referTo(object));
  }
  log.error(loc) << "Expected variable or function name but found \"" << name << "\"" << fatal;
}

void ExprLookup::setExpr(const sym::ExprType type) {
  assert(exprs.back().type != Expr::Type::expr);
  pushExpr(type);
}

void ExprLookup::enterSubExpr() {
  exprs.push_back({Expr::Type::subexpr});
}

sym::ExprType ExprLookup::leaveSubExpr() {
  assert(exprs.size() >= 2);
  const auto top = exprs.rbegin();
  assert(top[0].type == Expr::Type::expr);
  assert(top[1].type == Expr::Type::subexpr);
  exprs.pop_back();
  exprs.pop_back();
  return etype;
}

ExprLookup::Expr::Expr(const Type type)
  : type{type}, name{} {
  assert(
    type == Type::call ||
    type == Type::expr ||
    type == Type::subexpr
  );
}

ExprLookup::Expr::Expr(const Type type, const sym::Name &name)
  : type{type}, name{name} {
  assert(type == Type::member || type == Type::free_fun);
}

void ExprLookup::pushExpr(const sym::ExprType type) {
  exprs.push_back({Expr::Type::expr});
  etype = type;
}

ast::Statement *ExprLookup::pushObj(sym::Object *const obj) {
  pushExpr(obj->etype);
  return obj->node;
}

bool ExprLookup::memVarExpr(const Expr::Type type) const {
  const auto top = exprs.rbegin();
  return exprs.size() >= 2
    && top[0].type == type
    && top[1].type == Expr::Type::member
    && (exprs.size() == 2 || top[2].type != Expr::Type::call)
  ;
}

bool ExprLookup::memFunExpr(const Expr::Type type) const {
  const auto top = exprs.rbegin();
  return exprs.size() >= 3
    && top[0].type == type
    && top[1].type == Expr::Type::member
    && top[2].type == Expr::Type::call
  ;
}

bool ExprLookup::call(const Expr::Type type) const {
  const auto top = exprs.rbegin();
  return exprs.size() >= 2
    && top[0].type == type
    && top[1].type == Expr::Type::call
  ;
}

sym::Name ExprLookup::popName() {
  const Expr &expr = exprs.back();
  assert(expr.type == Expr::Type::member || expr.type == Expr::Type::free_fun);
  sym::Name name = std::move(expr.name);
  exprs.pop_back();
  return name;
}

sym::ExprType ExprLookup::popExpr() {
  assert(!exprs.empty());
  assert(exprs.back().type == Expr::Type::expr);
  exprs.pop_back();
  return etype;
}

ast::Func *ExprLookup::popCallPushRet(sym::Func *const func) {
  assert(!exprs.empty());
  assert(exprs.back().type == Expr::Type::call);
  exprs.pop_back();
  pushExpr(func->ret);
  return func->node;
}
