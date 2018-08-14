//
//  scope lookup.cpp
//  STELA
//
//  Created by Indi Kernick on 30/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "scope lookup.hpp"

#include <cassert>
#include "scope find.hpp"
#include "compare params args.hpp"

using namespace stela;

namespace {

template <typename Derived, typename Base>
Derived *assertDownCast(Base *const base) {
  assert(base);
  Derived *const derived = dynamic_cast<Derived *>(base);
  assert(derived);
  return derived;
}

sym::Func *selectOverload(
  Log &log,
  const std::vector<sym::Symbol *> &symbols,
  const sym::FunKey &key,
  const Loc loc
) {
  assert(!symbols.empty());
  for (sym::Symbol *symbol : symbols) {
    auto *func = dynamic_cast<sym::Func *>(symbol);
    if (func == nullptr) {
      log.error(loc) << "Calling \"" << key.name
        << "\" but it is not a function. " << symbols.front()->loc << fatal;
    }
    if (compatParams(func->params, key.params)) {
      return func;
    }
  }
  log.error(loc) << "No matching call to function \"" << key.name << '"' << fatal;
}

template <typename Symbol>
Symbol *referTo(Symbol *const symbol) {
  symbol->referenced = true;
  return symbol;
}

std::string_view scopeName(const sym::MemScope scope) {
  return scope == sym::MemScope::instance ? "instance" : "static";
}

class TypeVisitor final : public ast::Visitor {
public:
  explicit TypeVisitor(sym::Scope *const scope, Log &log)
    : lkp{scope, log}, log{log} {}

  void visit(ast::ArrayType &) override {
    assert(false);
  }
  void visit(ast::MapType &) override {
    assert(false);
  }
  void visit(ast::FuncType &) override {
    assert(false);
  }
  
  void visit(ast::NamedType &named) override {
    named.definition = lkp.lookupIdent(sym::Name(named.name), named.loc);
    type = named.definition;
  }
  
  void visit(ast::NestedType &nested) override {
    lkp.member(sym::Name(nested.name));
    nested.parent->accept(*this);
    nested.definition = lkp.lookupMember(nested.loc);
    type = nested.definition;
  }
  
  sym::Symbol *type;
  
  void enter() {
    lkp.enterSubExpr();
  }
  void leave(const Loc loc) {
    const sym::ExprType etype = lkp.leaveSubExpr();
    if (!etype.typeExpr) {
      log.error(loc) << "Expected type" << fatal;
    }
  }

private:
  ExprLookup lkp;
  Log &log;
};

template <typename ScopeType>
ScopeType *findNearest(sym::Scope *const scope) {
  if (scope == nullptr) {
    return nullptr;
  } else if (auto *dynamic = dynamic_cast<ScopeType *>(scope)) {
    return dynamic;
  } else {
    return findNearest<ScopeType>(scope->parent);
  }
}

struct ParentScope {
  sym::Scope *parent;
  sym::MemScope memScope = {};
  bool member = false;
};

sym::MemScope convertScope(const sym::FuncScope::Ctx ctx) {
  assert(ctx != sym::FuncScope::Ctx::free);
  if (ctx == sym::FuncScope::Ctx::stat_mem) {
    return sym::MemScope::static_;
  } else {
    return sym::MemScope::instance;
  }
}

sym::StructScope *memFunParent(sym::Scope *const parent) {
  return assertDownCast<sym::StructScope>(parent);
}

ParentScope parentScope(sym::Scope *const scope) {
  sym::FuncScope *func = dynamic_cast<sym::FuncScope *>(scope);
  if (func) {
    if (func->ctx == sym::FuncScope::Ctx::free) {
      return {findNearest<sym::NSScope>(func->parent)};
    } else {
      return {func->parent, convertScope(func->ctx), true};
    }
  } else {
    return {scope->parent};
  }
}

sym::MemAccess accessLevel(sym::Scope *const scope, sym::StructType *const strut) {
  assert(strut);
  assert(strut->scope);
  if (scope == strut->scope) {
    return sym::MemAccess::private_;
  }
  if (findNearest<sym::StructScope>(scope) == strut->scope) {
    return sym::MemAccess::private_;
  }
  return sym::MemAccess::public_;
}

}

sym::Symbol *stela::lookupType(sym::Scope *scope, Log &log, const ast::TypePtr &type) {
  TypeVisitor visitor{scope, log};
  visitor.enter();
  type->accept(visitor);
  visitor.leave(type->loc);
  return referTo(visitor.type);
}

sym::ValueMut refToMut(const ast::ParamRef ref) {
  if (ref == ast::ParamRef::inout) {
    return sym::ValueMut::var;
  } else {
    return sym::ValueMut::let;
  }
}

sym::ValueRef refToRef(const ast::ParamRef ref) {
  if (ref == ast::ParamRef::inout) {
    return sym::ValueRef::ref;
  } else {
    return sym::ValueRef::val;
  }
}

sym::FuncParams stela::lookupParams(sym::Scope *scope, Log &log, const ast::FuncParams &params) {
  sym::FuncParams symParams;
  for (const ast::FuncParam &param : params) {
    symParams.push_back({
      lookupType(scope, log, param.type),
      refToMut(param.ref),
      refToRef(param.ref)
    });
  }
  return symParams;
}

TypeLookup::TypeLookup(ScopeMan &man, Log &log)
  : man{man}, log{log} {}

sym::Symbol *TypeLookup::lookupType(const ast::TypePtr &astType) {
  return stela::lookupType(man.cur(), log, astType);
}

sym::FuncParams TypeLookup::lookupParams(const ast::FuncParams &params) {
  return stela::lookupParams(man.cur(), log, params);
}

ExprLookup::ExprLookup(sym::Scope *const scope, Log &log)
  : scope{scope}, log{log}, etype{sym::null_type} {}

void ExprLookup::call() {
  exprs.push_back({Expr::Type::call});
}

sym::Func *ExprLookup::lookupFun(
  sym::Scope *scope,
  const sym::FunKey &key,
  const Loc loc
) {
  const std::vector<sym::Symbol *> symbols = findMany(scope, key.name);
  if (symbols.empty()) {
    ParentScope ps = parentScope(scope);
    if (ps.member) {
      sym::Func *func = lookupFun(memFunParent(ps.parent), {
        key.name, key.params, sym::MemAccess::private_, ps.memScope,
      }, loc, true);
      if (func) {
        return func;
      }
      if (ps.memScope == sym::MemScope::instance) {
        func = lookupFun(memFunParent(ps.parent), {
          key.name, key.params, sym::MemAccess::private_, sym::MemScope::static_,
        }, loc, true);
        if (func) {
          return func;
        }
      }
      if (!ps.parent->parent) {
        log.error(loc) << "Use of undefined symbol \"" << key.name << '"' << fatal;
      }
      return lookupFun(ps.parent->parent, key, loc);
    } else if (ps.parent) {
      return lookupFun(ps.parent, key, loc);
    } else {
      log.error(loc) << "Use of undefined symbol \"" << key.name << '"' << fatal;
    }
  } else {
    return referTo(selectOverload(log, symbols, key, loc));
  }
}

sym::Func *ExprLookup::lookupFun(
  sym::StructScope *scope,
  const sym::MemFunKey &key,
  const Loc loc,
  const bool retNull = false
) {
  for (const sym::StructTableRow &row : scope->table) {
    if (key.name != row.name || key.scope != row.scope) {
      continue;
    }
    auto *func = dynamic_cast<sym::Func *>(row.val.get());
    if (func == nullptr) {
      log.error(loc) << "Calling \"" << key.name
        << "\" but it is not a function. " << func->loc << fatal;
    }
    if (!compatParams(func->params, key.params)) {
      continue;
    }
    if (key.access == sym::MemAccess::public_ && row.access == sym::MemAccess::private_) {
      log.error(loc) << "Cannot access private member function \""
        << key.name << "\" of struct" << fatal;
    }
    return referTo(func);
  }
  if (retNull) {
    return nullptr;
  } else {
    log.error(loc) << "No " << scopeName(key.scope) << " member function \""
      << key.name << "\" found in struct" << fatal;
  }
}

sym::Func *ExprLookup::lookupFunc(const sym::FuncParams &params, const Loc loc) {
  if (memFunExpr(Expr::Type::expr)) {
    auto *const strut = dynamic_cast<sym::StructType *>(popType());
    if (strut == nullptr) {
      log.error(loc) << "Can only call member functions on struct objects" << fatal;
    }
    return popCallPushRet(lookupFun(strut->scope, iMemFunKey(strut, params), loc));
  }
  if (memFunExpr(Expr::Type::static_type)) {
    auto *const strut = dynamic_cast<sym::StructType *>(popType());
    if (strut == nullptr) {
      log.error(loc) << "Can only call static member functions on struct types" << fatal;
    }
    return popCallPushRet(lookupFun(strut->scope, sMemFunKey(strut, params), loc));
  }
  if (freeFun()) {
    return popCallPushRet(lookupFun(scope, {popName(), params}, loc));
  }
  if (call(Expr::Type::expr)) {
    log.error(loc) << "Overloaded function call operator has not been implemented" << fatal;
  }
  log.error(loc) << "Function call operator applied to invalid subject" << fatal;
}

sym::Func *ExprLookup::lookupFunc(
  const sym::Name &name,
  const sym::FuncParams &params,
  const Loc loc
) {
  sym::Func *const func = lookupFun(scope, {name, params}, loc);
  pushExpr(func->ret);
  return func;
}

void ExprLookup::member(const sym::Name &name) {
  exprs.push_back({Expr::Type::member, name});
}

sym::Symbol *ExprLookup::lookupMem(
  sym::StructScope *scope,
  const sym::MemKey &key,
  const Loc loc,
  const bool retNull = false
) {
  for (const sym::StructTableRow &row : scope->table) {
    if (key.name != row.name || key.scope != row.scope) {
      continue;
    }
    if (key.access == sym::MemAccess::public_ && row.access == sym::MemAccess::private_) {
      log.error(loc) << "Cannot access private member \"" << key.name << "\" of struct" << fatal;
    }
    if (sym::Func *func = dynamic_cast<sym::Func *>(row.val.get())) {
      log.error(loc) << "Reference to " << scopeName(key.scope) << " member function \""
        << key.name << "\" must be called" << fatal;
    }
    return referTo(row.val.get());
  }
  if (retNull) {
    return nullptr;
  } else {
    log.error(loc) << "No " << scopeName(key.scope) << " member \""
      << key.name << "\" found in struct" << fatal;
  }
}

sym::Object *ExprLookup::lookupMem(
  sym::EnumScope *scope,
  const sym::Name &name,
  const Loc loc
) {
  for (const sym::EnumTableRow &row : scope->table) {
    if (row.key == name) {
      return referTo(assertDownCast<sym::Object>(row.val.get()));
    }
  }
  log.error(loc) << "No case \"" << name << "\" found in enum" << fatal;
}

sym::Symbol *ExprLookup::lookupMember(const Loc loc) {
  if (memVarExpr(Expr::Type::expr)) {
    auto *strut = dynamic_cast<sym::StructType *>(popType());
    if (strut == nullptr) {
      log.error(loc) << "Can only use . operator on struct objects" << fatal;
    }
    sym::Symbol *const member = lookupMem(strut->scope, iMemKey(strut), loc);
    sym::Object *const object = assertDownCast<sym::Object>(member);
    pushExpr(memberType(etype, object->etype));
    return object;
  }
  if (memVarExpr(Expr::Type::static_type)) {
    sym::Symbol *const type = popType();
    if (auto *strut = dynamic_cast<sym::StructType *>(type)) {
      sym::Symbol *const member = lookupMem(strut->scope, sMemKey(strut), loc);
      if (auto *object = dynamic_cast<sym::Object *>(member)) {
        return pushObj(object);
      } else {
        return pushStatic(member);
      }
    }
    if (auto *enm = dynamic_cast<sym::EnumType *>(type)) {
      return pushObj(lookupMem(enm->scope, popName(), loc));
    }
    log.error(loc) << "Can only access static members of struct and enum types" << fatal;
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
    return referTo(symbol);
  }
  ParentScope ps = parentScope(scope);
  if (ps.member) {
    sym::Symbol *symbol = lookupMem(memFunParent(ps.parent), {
      name, sym::MemAccess::private_, ps.memScope,
    }, loc, true);
    if (symbol) {
      return symbol;
    }
    if (ps.memScope == sym::MemScope::instance) {
      symbol = lookupMem(memFunParent(ps.parent), {
        name, sym::MemAccess::private_, sym::MemScope::static_,
      }, loc, true);
      if (symbol) {
        return symbol;
      }
    }
    if (!ps.parent->parent) {
      log.error(loc) << "Use of undefined symbol \"" << name << '"' << fatal;
    }
    return lookupIdent(ps.parent->parent, name, loc);
  } else if (ps.parent) {
    return lookupIdent(ps.parent, name, loc);
  } else {
    log.error(loc) << "Use of undefined symbol \"" << name << '"' << fatal;
  }
}

sym::Symbol *ExprLookup::lookupIdent(const sym::Name &name, const Loc loc) {
  sym::Symbol *symbol = lookupIdent(scope, name, loc);
  if (auto *func = dynamic_cast<sym::Func *>(symbol)) {
    if (exprs.back().type != Expr::Type::call) {
      log.error(loc) << "Reference to function \"" << name << "\" must be called" << fatal;
    }
    exprs.push_back({Expr::Type::ident, name});
    return nullptr;
  }
  if (auto *object = dynamic_cast<sym::Object *>(symbol)) {
    return pushObj(object);
  }
  if (auto *alias = dynamic_cast<sym::TypeAlias *>(symbol)) {
    symbol = alias->type;
  }
  // symbol must be a StructType or EnumType
  pushStatic(symbol);
  return symbol;
}

sym::Symbol *ExprLookup::lookupSelf(const Loc loc) {
  return lookupIdent("self", loc);
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
  assert(top[0].type == Expr::Type::expr || top[0].type == Expr::Type::static_type);
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
    type == Type::static_type ||
    type == Type::subexpr
  );
}

ExprLookup::Expr::Expr(const Type type, const sym::Name &name)
  : type{type}, name{name} {
  assert(type == Type::member || type == Type::ident);
}

void ExprLookup::pushExpr(const sym::ExprType type) {
  exprs.push_back({Expr::Type::expr});
  etype = type;
}

sym::Object *ExprLookup::pushObj(sym::Object *const obj) {
  pushExpr(obj->etype);
  return obj;
}

sym::Symbol *ExprLookup::pushStatic(sym::Symbol *const type) {
  exprs.push_back({Expr::Type::static_type});
  etype.type = type;
  etype.typeExpr = true;
  return type;
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

bool ExprLookup::freeFun() const {
  return call(Expr::Type::ident);
}

sym::Name ExprLookup::popName() {
  const Expr &expr = exprs.back();
  assert(expr.type == Expr::Type::member || expr.type == Expr::Type::ident);
  sym::Name name = std::move(expr.name);
  exprs.pop_back();
  return name;
}

sym::Symbol *ExprLookup::popType() {
  assert(!exprs.empty());
  const Expr::Type type = exprs.back().type;
  assert(type == Expr::Type::expr || type == Expr::Type::static_type);
  exprs.pop_back();
  return etype.type;
}

sym::Func *ExprLookup::popCallPushRet(sym::Func *const func) {
  assert(!exprs.empty());
  assert(exprs.back().type == Expr::Type::call);
  exprs.pop_back();
  pushExpr(func->ret);
  return func;
}

sym::MemFunKey ExprLookup::sMemFunKey(sym::StructType *strut, const sym::FuncParams &params) {
  return {
    popName(),
    params,
    accessLevel(scope, strut),
    sym::MemScope::static_
  };
}

sym::MemFunKey ExprLookup::iMemFunKey(sym::StructType *strut, const sym::FuncParams &params) {
  sym::MemFunKey key = sMemFunKey(strut, params);
  key.scope = sym::MemScope::instance;
  key.params.insert(key.params.begin(), etype);
  return key;
}

sym::MemKey ExprLookup::sMemKey(sym::StructType *const strut) {
  return {
    popName(),
    accessLevel(scope, strut),
    sym::MemScope::static_
  };
}

sym::MemKey ExprLookup::iMemKey(sym::StructType *const strut) {
  sym::MemKey key = sMemKey(strut);
  key.scope = sym::MemScope::instance;
  return key;
}
