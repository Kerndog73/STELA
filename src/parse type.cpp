//
//  parse type.cpp
//  STELA
//
//  Created by Indi Kernick on 1/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "parse type.hpp"

using namespace stela;

namespace {

ast::TypePtr parseFuncType(ParseTokens &tok) {
  if (!tok.checkKeyword("func")) {
    return nullptr;
  }
  Context ctx = tok.context("in function type");
  tok.expectOp("(");
  auto type = std::make_unique<ast::FuncType>();
  type->loc = tok.lastLoc();
  if (!tok.checkOp(")")) {
    do {
      ast::ParamType &param = type->params.emplace_back();
      param.ref = parseRef(tok);
      param.type = tok.expectNode(parseType, "type");
    } while (tok.expectEitherOp(")", ",") == ",");
  }
  if (tok.checkOp("->")) {
    ctx.desc("after ->");
    type->ret = tok.expectNode(parseType, "type");
  }
  return type;
}

ast::TypePtr parseArrayType(ParseTokens &tok) {
  if (!tok.checkOp("[")) {
    return nullptr;
  }
  Context ctx = tok.context("in array type");
  auto arrayType = std::make_unique<ast::ArrayType>();
  arrayType->loc = tok.lastLoc();
  arrayType->elem = tok.expectNode(parseType, "element type");
  tok.expectOp("]");
  return arrayType;
}

ast::TypePtr parseNamedType(ParseTokens &tok) {
  if (!tok.peekIdentType()) {
    return nullptr;
  }
  auto namedType = std::make_unique<ast::NamedType>();
  namedType->loc = tok.loc();
  namedType->name = tok.expectID();
  return namedType;
}

ast::TypePtr parseNamespacedType(ParseTokens &tok) {
  ast::TypePtr left = parseNamedType(tok);
  if (left == nullptr) {
    return nullptr;
  }
  while (tok.checkOp(".")) {
    auto nest = std::make_unique<ast::NamespacedType>();
    nest->loc = tok.lastLoc();
    nest->parent = std::move(left);
    nest->name = tok.expectID();
    left = std::move(nest);
  }
  return left;
}

ast::Field parseStructMember(ParseTokens &tok) {
  ast::Field field;
  field.name = tok.expectID();
  tok.expectOp(":");
  field.type = tok.expectNode(parseType, "type after :");
  tok.expectOp(";");
  return field;
}

ast::TypePtr parseStructType(ParseTokens &tok) {
  if (!tok.checkKeyword("struct")) {
    return nullptr;
  }
  auto strut = std::make_unique<ast::StructType>();
  strut->loc = tok.lastLoc();
  Context ctx = tok.context("in struct type");
  tok.expectOp("{");
  while (!tok.checkOp("}")) {
    strut->fields.push_back(parseStructMember(tok));
  }
  return strut;
}

}

ast::ParamRef stela::parseRef(ParseTokens &tok) {
  if (tok.checkKeyword("inout")) {
    return ast::ParamRef::inout;
  } else {
    return ast::ParamRef::value;
  }
}

ast::TypePtr stela::parseType(ParseTokens &tok) {
  if (ast::TypePtr type = parseFuncType(tok)) return type;
  if (ast::TypePtr type = parseArrayType(tok)) return type;
  if (ast::TypePtr type = parseNamedType(tok)) return type;
  if (ast::TypePtr type = parseNamespacedType(tok)) return type;
  if (ast::TypePtr type = parseStructType(tok)) return type;
  return nullptr;
}
