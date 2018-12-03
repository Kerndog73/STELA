//
//  symbols.hpp
//  STELA
//
//  Created by Indi Kernick on 9/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_symbols_hpp
#define stela_symbols_hpp

#include <string>
#include <memory>
#include "ast.hpp"
#include <unordered_map>

namespace stela::sym {

struct Symbol {
  virtual ~Symbol();

  Loc loc;
  // for detecting things like: unused variable, unused function, etc
  bool referenced = false;
};
using SymbolPtr = std::unique_ptr<Symbol>;

using Name = std::string;
using Table = std::unordered_multimap<Name, SymbolPtr>;

enum class ScopeType {
  // global namespace scope
  ns,
  block,
  func,
  // break and continue are valid within flow scopes (while, for, switch)
  flow,
  closure
};

struct Scope {
  Scope(Scope *const parent, const ast::Name module)
    : parent{parent}, type{ScopeType::ns}, symbol{}, module{module} {}
  Scope(Scope *const parent, const ScopeType type)
    : parent{parent}, type{type}, symbol{}, module{} {
    assert(type != ScopeType::func && type != ScopeType::closure);
  }
  Scope(Scope *const parent, const ScopeType type, sym::Symbol *const symbol)
    : parent{parent}, type{type}, symbol{symbol}, module{} {
    assert(type == ScopeType::func || type == ScopeType::closure);
  }

  Scope *const parent;
  const ScopeType type;
  sym::Symbol *const symbol;
  const ast::Name module;
  Table table;
};
using ScopePtr = std::unique_ptr<Scope>;
using Scopes = std::vector<ScopePtr>;

enum class ValueMut : uint8_t {
  let,
  var
};

enum class ValueRef : uint8_t  {
  val,
  ref
};

struct ExprType {
  ast::TypePtr type;
  ValueMut mut;
  ValueRef ref;
};

inline ValueMut common(const ValueMut a, const ValueMut b) {
  return a == ValueMut::let ? a : b;
}

inline ValueRef common(const ValueRef a, const ValueRef b) {
  return a == ValueRef::val ? a : b;
}

inline ExprType memberType(const ExprType &obj, const ast::TypePtr &mem) {
  return {mem, obj.mut, obj.ref};
}

inline ExprType makeLetVal(const ast::TypePtr &type) {
  return {type, ValueMut::let, ValueRef::val};
}

inline ExprType makeLetVal(ast::TypePtr &&type) {
  return {std::move(type), ValueMut::let, ValueRef::val};
}

inline ExprType makeVarVal(const ast::TypePtr &type) {
  return {type, ValueMut::var, ValueRef::val};
}

inline ExprType makeVarVal(ast::TypePtr &&type) {
  return {std::move(type), ValueMut::var, ValueRef::val};
}

inline bool callMut(const ValueMut param, const ValueMut arg) {
  return static_cast<uint8_t>(param) <= static_cast<uint8_t>(arg);
}

inline bool callMutRef(const ExprType &param, const ExprType &arg) {
  return param.ref == sym::ValueRef::val || callMut(param.mut, arg.mut);
}

const ExprType null_type {};

struct TypeAlias final : Symbol {
  ~TypeAlias();
  
  retain_ptr<ast::TypeAlias> node;
};

struct Object final : Symbol {
  ~Object();
  
  ExprType etype;
  retain_ptr<ast::Statement> node;
};

// the first parameter is the receiver
// if params.front().type == nullptr then there is no receiver
using FuncParams = std::vector<ExprType>;
struct Func final : Symbol {
  ~Func();
  
  ExprType ret;
  FuncParams params;
  Scope *scope;
  retain_ptr<ast::Func> node;
};
using FuncPtr = std::unique_ptr<Func>;

struct ClosureCap {
  ast::TypePtr type;
  ast::Statement *object = nullptr;
  uint32_t index = ~uint32_t{};
};
struct Lambda final : Symbol {
  ~Lambda();
  
  ExprType ret;
  FuncParams params;
  Scope *scope;
  retain_ptr<ast::Lambda> node;
  std::vector<ClosureCap> captures;
};

struct BtnFunc final : Symbol {
  ~BtnFunc();
  
  retain_ptr<ast::BtnFunc> node;
};

struct Builtins {
  // Builtin types
  ast::BtnTypePtr Void;
  ast::BtnTypePtr Bool;
  ast::BtnTypePtr Byte;
  ast::BtnTypePtr Char;
  ast::BtnTypePtr Real;
  ast::BtnTypePtr Sint;
  ast::BtnTypePtr Uint;
  
  // Alias of [char] used as the type of string literals
  ast::TypePtr string;
};

}

namespace stela {

struct Symbols {
  ast::Decls decls;
  sym::Scopes scopes;
  sym::Builtins builtins;
  sym::Scope *global;
};

}

#endif
