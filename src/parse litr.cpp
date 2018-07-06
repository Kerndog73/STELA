//
//  parse litr.cpp
//  STELA
//
//  Created by Indi Kernick on 1/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "parse litr.hpp"

#include "parse expr.hpp"
#include "parse stat.hpp"
#include "parse func.hpp"

using namespace stela;

namespace {

template <typename Literal, bool Trim = false>
ast::LitrPtr parseLiteralToken(ParseTokens &tok, const Token::Type type) {
  if (tok.peekType(type)) {
    auto literal = std::make_unique<Literal>();
    literal->value = tok.expect(type);
    if constexpr (Trim) {
      literal->value.remove_prefix(1);
      literal->value.remove_suffix(1);
    }
    return literal;
  } else {
    return nullptr;
  }
}

ast::LitrPtr parseString(ParseTokens &tok) {
  return parseLiteralToken<ast::StringLiteral, true>(tok, Token::Type::string);
}

ast::LitrPtr parseChar(ParseTokens &tok) {
  return parseLiteralToken<ast::CharLiteral, true>(tok, Token::Type::character);
}

ast::LitrPtr parseNumber(ParseTokens &tok) {
  return parseLiteralToken<ast::NumberLiteral>(tok, Token::Type::number);
}

ast::LitrPtr parseBool(ParseTokens &tok) {
  bool value;
  if (tok.checkKeyword("true")) {
    value = true;
  } else if (tok.checkKeyword("false")) {
    value = false;
  } else {
    return nullptr;
  }
  auto literal = std::make_unique<ast::BoolLiteral>();
  literal->value = value;
  return literal;
}

ast::LitrPtr parseArray(ParseTokens &tok) {
  if (!tok.checkOp("[")) {
    return nullptr;
  }
  Context ctx = tok.context("in array literal");
  auto array = std::make_unique<ast::ArrayLiteral>();
  if (!tok.checkOp("]")) {
    do {
      array->exprs.push_back(tok.expectNode(parseExpr, "expression"));
    } while (tok.expectEitherOp(",", "]") == ",");
  }
  return array;
}

ast::LitrPtr parseMap(ParseTokens &tok) {
  if (!tok.checkOp("[{")) {
    return nullptr;
  }
  Context ctx = tok.context("in map literal");
  auto map = std::make_unique<ast::MapLiteral>();
  if (!tok.checkOp("}]")) {
    do {
      ast::MapPair &pair = map->pairs.emplace_back();
      pair.key = tok.expectNode(parseExpr, "key expression");
      tok.expectOp(":");
      pair.val = tok.expectNode(parseExpr, "value expression");
    } while (tok.expectEitherOp(",", "}]") == ",");
  }
  return map;
}

ast::Block parseLambdaBody(ParseTokens &tok) {
  ast::Block body;
  Context ctx = tok.context("in body");
  tok.expectKeyword("in");
  while (ast::StatPtr node = parseStat(tok)) {
    body.nodes.emplace_back(std::move(node));
  }
  tok.expectOp("}");
  return body;
}

ast::LitrPtr parseLambda(ParseTokens &tok) {
  if (!tok.checkOp("{")) {
    return nullptr;
  }
  Context ctx = tok.context("in lambda expression");
  auto lambda = std::make_unique<ast::Lambda>();
  lambda->params = parseFuncParams(tok);
  lambda->ret = parseFuncRet(tok);
  lambda->body = parseLambdaBody(tok);
  return lambda;
}

}

ast::LitrPtr stela::parseLitr(ParseTokens &tok) {
  if (ast::LitrPtr node = parseString(tok)) return node;
  if (ast::LitrPtr node = parseChar(tok)) return node;
  if (ast::LitrPtr node = parseNumber(tok)) return node;
  if (ast::LitrPtr node = parseBool(tok)) return node;
  if (ast::LitrPtr node = parseArray(tok)) return node;
  if (ast::LitrPtr node = parseMap(tok)) return node;
  if (ast::LitrPtr node = parseLambda(tok)) return node;
  return nullptr;
}