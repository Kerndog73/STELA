//
//  builtin symbols.cpp
//  STELA
//
//  Created by Indi Kernick on 16/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "builtin symbols.hpp"

#include "symbol desc.hpp"
#include "scope lookup.hpp"
#include "operator name.hpp"
#include "compare types.hpp"
#include "Utils/unreachable.hpp"
#include "Utils/assert down cast.hpp"

using namespace stela;

namespace {

stela::ast::BtnTypePtr insertType(
  sym::Table &table,
  const ast::BtnTypeEnum e,
  const ast::Name name
) {
  auto type = stela::make_retain<ast::BtnType>(e);
  auto alias = make_retain<ast::TypeAlias>();
  alias->name = name;
  alias->strong = false;
  alias->type = type;
  auto symbol = std::make_unique<sym::TypeAlias>();
  alias->symbol = symbol.get();
  symbol->node = std::move(alias);
  table.insert({sym::Name{name}, std::move(symbol)});
  return type;
}

void insertTypes(sym::Table &table, sym::Builtins &btn) {
  btn.Void = stela::make_retain<ast::BtnType>(ast::BtnTypeEnum::Void);
  btn.Opaq = insertType(table, ast::BtnTypeEnum::Opaq, "opaq");
  btn.Bool = insertType(table, ast::BtnTypeEnum::Bool, "bool");
  btn.Byte = insertType(table, ast::BtnTypeEnum::Byte, "byte");
  btn.Char = insertType(table, ast::BtnTypeEnum::Char, "char");
  btn.Real = insertType(table, ast::BtnTypeEnum::Real, "real");
  btn.Sint = insertType(table, ast::BtnTypeEnum::Sint, "sint");
  btn.Uint = insertType(table, ast::BtnTypeEnum::Uint, "uint");
  
  // A string literal has the type [char]
  auto charName = make_retain<ast::NamedType>();
  charName->name = "char";
  auto symbolIter = table.find("char");
  assert(symbolIter != table.end());
  sym::Symbol *symbol = symbolIter->second.get();
  assert(symbol);
  sym::TypeAlias *alias = assertDownCast<sym::TypeAlias>(symbol);
  charName->definition = alias->node.get();
  auto string = make_retain<ast::ArrayType>();
  string->elem = std::move(charName);
  btn.string = std::move(string);
}

void insertFunc(sym::Table &table, const ast::BtnFuncEnum e, const ast::Name name) {
  auto symbol = std::make_unique<sym::BtnFunc>();
  symbol->node = make_retain<ast::BtnFunc>(e);
  table.insert({sym::Name{name}, std::move(symbol)});
}

void insertFuncs(sym::Table &table) {
  insertFunc(table, ast::BtnFuncEnum::capacity,  "capacity");
  insertFunc(table, ast::BtnFuncEnum::size,      "size");
  insertFunc(table, ast::BtnFuncEnum::data,      "data");
  insertFunc(table, ast::BtnFuncEnum::push_back, "push_back");
  insertFunc(table, ast::BtnFuncEnum::append,    "append");
  insertFunc(table, ast::BtnFuncEnum::pop_back,  "pop_back");
  insertFunc(table, ast::BtnFuncEnum::resize,    "resize");
  insertFunc(table, ast::BtnFuncEnum::reserve,   "reserve");
}

bool isBoolType(const ast::BtnTypeEnum type) {
  return type == ast::BtnTypeEnum::Bool;
}

bool isBitwiseType(const ast::BtnTypeEnum type) {
  return type == ast::BtnTypeEnum::Byte || type == ast::BtnTypeEnum::Uint;
}

bool isArithType(const ast::BtnTypeEnum type) {
  return ast::BtnTypeEnum::Char <= type && type <= ast::BtnTypeEnum::Uint;
}

template <typename Enum>
auto operator+(const Enum e) -> std::underlying_type_t<Enum> {
  return static_cast<std::underlying_type_t<Enum>>(e);
}

bool isBoolOp(const ast::BinOp op) {
  return op == ast::BinOp::bool_or || op == ast::BinOp::bool_and;
}

bool isBitwiseOp(const ast::BinOp op) {
  return +ast::BinOp::bit_or <= +op && +op <= +ast::BinOp::bit_shr;
}

bool isEqualOp(const ast::BinOp op) {
  return op == ast::BinOp::eq || op == ast::BinOp::ne;
}

bool isOrderOp(const ast::BinOp op) {
  return +ast::BinOp::lt <= +op && +op <= +ast::BinOp::ge;
}

bool isArithOp(const ast::BinOp op) {
  return +ast::BinOp::add <= +op && +op <= +ast::BinOp::mod;
}

bool isBitwiseOp(const ast::AssignOp op) {
  return +ast::AssignOp::bit_or <= +op && +op <= +ast::AssignOp::bit_shr;
}

bool isArithOp(const ast::AssignOp op) {
  return +ast::AssignOp::add <= +op && +op <= +ast::AssignOp::mod;
}

}

bool stela::boolOp(const ast::BinOp op) {
  return isBoolOp(op);
}

bool stela::boolOp(const ast::UnOp op) {
  return op == ast::UnOp::bool_not;
}

bool stela::compOp(const ast::BinOp op) {
  return isEqualOp(op) || isOrderOp(op);
}

bool stela::validIncr(const ast::BtnTypePtr &type) {
  return isArithType(type->value);
}

bool stela::validOp(const ast::UnOp op, const ast::BtnTypePtr &type) {
  switch (op) {
    case ast::UnOp::neg:
      return isArithType(type->value);
    case ast::UnOp::bool_not:
      return isBoolType(type->value);
    case ast::UnOp::bit_not:
      return isBitwiseType(type->value);
  }
  UNREACHABLE();
}

ast::BtnTypePtr checkType(bool(*category)(ast::BtnTypeEnum), const ast::BtnTypePtr &type) {
  return category(type->value) ? type : nullptr;
}

ast::BtnTypePtr stela::validOp(
  const ast::BinOp op,
  const ast::BtnTypePtr &lhs,
  const ast::BtnTypePtr &rhs
) {
  if (lhs->value != rhs->value) {
    return nullptr;
  }
  assert(!isEqualOp(op));
  assert(!isOrderOp(op));
  if (isBoolOp(op)) {
    return checkType(isBoolType, lhs);
  } else if (isBitwiseOp(op)) {
    return checkType(isBitwiseType, lhs);
  } else if (isArithOp(op)) {
    return checkType(isArithType, lhs);
  }
  UNREACHABLE();
}

bool stela::validOp(
  const ast::AssignOp op,
  const ast::BtnTypePtr &type
) {
  if (isArithOp(op)) {
    return isArithType(type->value);
  } else if (isBitwiseOp(op)) {
    return isBitwiseType(type->value);
  }
  UNREACHABLE();
}

bool stela::validSubscript(const ast::BtnTypePtr &index) {
  return index->value == ast::BtnTypeEnum::Sint || index->value == ast::BtnTypeEnum::Uint;
}

bool stela::validCast(const ast::BtnTypePtr &dst, const ast::BtnTypePtr &src) {
  return dst->value != ast::BtnTypeEnum::Void &&
         src->value != ast::BtnTypeEnum::Void &&
         dst->value != ast::BtnTypeEnum::Opaq &&
         src->value != ast::BtnTypeEnum::Opaq;
}

namespace {

void checkArgs(Log &log, const ast::Name name, const Loc loc, const bool correct) {
  if (!correct) {
    log.error(loc) << "No matching call to builtin function \"" << name << '"' << fatal;
  }
}

stela::retain_ptr<ast::ArrayType> checkArray(sym::Ctx ctx, const ast::Name name, const Loc loc, const ast::TypePtr &type) {
  auto array = lookupConcrete<ast::ArrayType>(ctx, type);
  if (!array) {
    ctx.log.error(loc) << "Expected [T] in call to builtin function \"" << name
      << "\" but got " << typeDesc(type) << fatal;
  }
  return array;
}

void checkUint(sym::Ctx ctx, const ast::Name name, const Loc loc, const ast::TypePtr &type) {
  if (!compareTypes(ctx, ctx.btn.Uint, type)) {
    ctx.log.error(loc) << "Expected uint in call to builtin function \"" << name
      << "\" but got " << typeDesc(type) << fatal;
  }
}

void checkMutRef(Log &log, const ast::Name name, const Loc loc, const sym::ExprType &etype) {
  const sym::ExprType varRef = {nullptr, sym::ValueMut::var, sym::ValueRef::ref};
  if (!sym::callMutRef(varRef, etype)) {
    log.error(loc) << "No matching call to builtin function \"" << name << '"' << fatal;
  }
}

// func capacity<T>(arr: [T]) -> uint;
ast::TypePtr capacityFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "capacity", loc, args.size() == 1);
  checkArray(ctx, "capacity", loc, args[0].type);
  return ctx.btn.Uint;
}

// func size<T>(arr: [T]) -> uint;
ast::TypePtr sizeFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "size", loc, args.size() == 1);
  checkArray(ctx, "size", loc, args[0].type);
  return ctx.btn.Uint;
}

// func data<T>(arr: [T]) -> opaq;
ast::TypePtr dataFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "data", loc, args.size() == 1);
  checkArray(ctx, "data", loc, args[0].type);
  return ctx.btn.Opaq;
}

// func push_back<T>(arr: ref [T], elem: T);
ast::TypePtr pushBackFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "push_back", loc, args.size() == 2);
  auto array = checkArray(ctx, "push_back", loc, args[0].type);
  checkMutRef(ctx.log, "push_back", loc, args[0]);
  if (!compareTypes(ctx, args[1].type, array->elem)) {
    ctx.log.error(loc) << "Expected T for second argument to builtin function"
      << " \"push_back\" but got " << typeDesc(args[1].type) << fatal;
  }
  return ctx.btn.Void;
}

// func append<T>(arr: ref [T], other: [T]);
ast::TypePtr appendFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "append", loc, args.size() == 2);
  auto array = checkArray(ctx, "append", loc, args[0].type);
  checkMutRef(ctx.log, "append", loc, args[0]);
  if (!compareTypes(ctx, args[1].type, array)) {
    ctx.log.error(loc) << "Expected [T] for second argument to builtin function"
      << " \"append\" but got " << typeDesc(args[1].type) << fatal;
  }
  return ctx.btn.Void;
}

// func pop_back<T>(arr: ref [T]);
ast::TypePtr popBackFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "pop_back", loc, args.size() == 1);
  checkArray(ctx, "pop_back", loc, args[0].type);
  checkMutRef(ctx.log, "pop_back", loc, args[0]);
  return ctx.btn.Void;
}

// func resize<T>(arr: ref [T], size: uint);
ast::TypePtr resizeFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "resize", loc, args.size() == 2);
  checkArray(ctx, "resize", loc, args[0].type);
  checkMutRef(ctx.log, "resize", loc, args[0]);
  checkUint(ctx, "resize", loc, args[1].type);
  return ctx.btn.Void;
}

// func reserve<T>(arr: ref [T], cap: uint);
ast::TypePtr reserveFn(sym::Ctx ctx, const sym::FuncParams &args, const Loc loc) {
  checkArgs(ctx.log, "reserve", loc, args.size() == 2);
  checkArray(ctx, "reserve", loc, args[0].type);
  checkMutRef(ctx.log, "reserve", loc, args[0]);
  checkUint(ctx, "reserve", loc, args[1].type);
  return ctx.btn.Void;
}

}

void stela::validComp(
  sym::Ctx ctx,
  const ast::BinOp op,
  const ast::TypePtr &type,
  const Loc loc
) {
  ast::TypePtr concrete = lookupConcreteType(ctx, type);
  if (auto btn = dynamic_pointer_cast<ast::BtnType>(concrete)) {
    if (btn->value == ast::BtnTypeEnum::Void) {
      ctx.log.error(loc) << "Cannot compare void expressions" << fatal;
    }
    return;
  } else if (auto arr = dynamic_pointer_cast<ast::ArrayType>(concrete)) {
    return validComp(ctx, op, arr->elem, loc);
  } else if (auto fun = dynamic_pointer_cast<ast::FuncType>(concrete)) {
    return;
  } else if (auto srt = dynamic_pointer_cast<ast::StructType>(concrete)) {
    for (const ast::Field &field : srt->fields) {
      validComp(ctx, op, field.type, loc);
    }
  }
}

ast::TypePtr stela::callBtnFunc(
  sym::Ctx ctx,
  const ast::BtnFuncEnum e,
  const sym::FuncParams &args,
  const Loc loc
) {
  switch (e) {
    case ast::BtnFuncEnum::capacity:
      return capacityFn(ctx, args, loc);
    case ast::BtnFuncEnum::size:
      return sizeFn(ctx, args, loc);
    case ast::BtnFuncEnum::data:
      return dataFn(ctx, args, loc);
    case ast::BtnFuncEnum::push_back:
      return pushBackFn(ctx, args, loc);
    case ast::BtnFuncEnum::append:
      return appendFn(ctx, args, loc);
    case ast::BtnFuncEnum::pop_back:
      return popBackFn(ctx, args, loc);
    case ast::BtnFuncEnum::resize:
      return resizeFn(ctx, args, loc);
    case ast::BtnFuncEnum::reserve:
      return reserveFn(ctx, args, loc);
  }
  UNREACHABLE();
}

sym::ScopePtr stela::makeBuiltinModule(sym::Builtins &btn) {
  auto scope = std::make_unique<sym::Scope>(nullptr, "$builtin");
  insertTypes(scope->table, btn);
  insertFuncs(scope->table);
  return scope;
}
