//
//  scope insert.cpp
//  STELA
//
//  Created by Indi Kernick on 30/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "scope insert.hpp"

#include <cassert>
#include "scope lookup.hpp"
#include "compare params args.hpp"

using namespace stela;

namespace {

sym::FuncPtr makeFunc(const Loc loc) {
  auto funcSym = std::make_unique<sym::Func>();
  funcSym->loc = loc;
  return funcSym;
}

auto makeParam(const sym::ExprType &etype, ast::FuncParam &param) {
  auto paramSym = std::make_unique<sym::Object>();
  paramSym->loc = param.loc;
  paramSym->etype = etype;
  paramSym->node = {retain, &param};
  return paramSym;
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

sym::ExprType convert(const NameLookup &tlk, const ast::TypePtr &type, const ast::ParamRef ref) {
  tlk.validateType(type);
  return {
    type,
    refToMut(ref),
    refToRef(ref)
  };
}

sym::ExprType convert(const NameLookup &tlk, const ast::FuncParam &param) {
  return convert(tlk, param.type, param.ref);
}

sym::FuncParams convertParams(
  const NameLookup &tlk,
  const ast::Receiver &receiver,
  const ast::FuncParams &params
) {
  sym::FuncParams symParams;
  if (receiver) {
    symParams.push_back(convert(tlk, *receiver));
  } else {
    symParams.push_back(sym::null_type);
  }
  for (const ast::FuncParam &param : params) {
    symParams.push_back(convert(tlk, param));
  }
  return symParams;
}

}

void InserterManager::insert(const sym::Name &name, sym::SymbolPtr symbol) {
  const auto iter = man.cur()->table.find(name);
  if (iter != man.cur()->table.end()) {
    log.error(symbol->loc) << "Redefinition of symbol \"" << name
      << "\" previously declared at " << iter->second->loc << fatal;
  } else {
    man.cur()->table.insert({name, std::move(symbol)});
  }
}

sym::Func *InserterManager::insert(ast::Func &func) {
  sym::FuncPtr funcSym = makeFunc(func.loc);
  if (func.receiver) {
    if (auto strut = tlk.lookupConcrete<ast::StructType>(func.receiver->type)) {
      for (const ast::Field &field : strut->fields) {
        if (field.name == func.name) {
          log.error(func.loc) << "Colliding function and field \"" << func.name << "\"" << fatal;
        }
      }
    }
  }
  funcSym->params = convertParams(tlk, func.receiver, func.params);
  funcSym->ret = convert(tlk, func.ret, ast::ParamRef::value);
  funcSym->node = {retain, &func};
  const auto [beg, end] = man.cur()->table.equal_range(sym::Name(func.name));
  for (auto s = beg; s != end; ++s) {
    sym::Symbol *const symbol = s->second.get();
    sym::Func *const dupFunc = dynamic_cast<sym::Func *>(symbol);
    if (dupFunc) {
      if (sameParams(tlk, dupFunc->params, funcSym->params)) {
        log.error(funcSym->loc) << "Redefinition of function \"" << func.name
          << "\" previously declared at " << symbol->loc << fatal;
      }
    } else {
      log.error(funcSym->loc) << "Redefinition of function \"" << func.name
        << "\" previously declared (as a different kind of symbol) at "
        << symbol->loc << fatal;
    }
  }
  sym::Func *const ret = funcSym.get();
  man.cur()->table.insert({sym::Name(func.name), std::move(funcSym)});
  return ret;
}

void InserterManager::enterFuncScope(sym::Func *funcSym, ast::Func &func) {
  for (size_t i = 0; i != func.params.size(); ++i) {
    funcSym->scope->table.insert({
      sym::Name(func.params[i].name),
      makeParam(funcSym->params[i + 1], func.params[i])
    });
  }
  if (func.receiver) {
    ast::FuncParam &param = *func.receiver;
    funcSym->scope->table.insert({
      sym::Name(param.name),
      makeParam(funcSym->params[0], param)
    });
  }
}

InserterManager::InserterManager(sym::Modules &modules, ScopeMan &man, Log &log)
  : log{log}, man{man}, tlk{modules, man, log} {}
