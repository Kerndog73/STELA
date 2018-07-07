//
//  parse tokens.cpp
//  STELA
//
//  Created by Indi Kernick on 24/6/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "parse tokens.hpp"

std::ostream &stela::operator<<(std::ostream &stream, const Token::Type type) {
  switch (type) {
    case Token::Type::keyword:
      return stream << "keyword";
    case Token::Type::identifier:
      return stream << "identifier";
    case Token::Type::number:
      return stream << "number";
    case Token::Type::string:
      return stream << "string";
    case Token::Type::character:
      return stream << "character";
    case Token::Type::oper:
      return stream << "operator";
  }
}

std::ostream &stela::operator<<(std::ostream &stream, const Token &tok) {
  return stream << tok.type << ' ' << tok.view;
}

stela::ParseTokens::ParseTokens(const Tokens &tokens, Log &log)
  : beg{tokens.data()}, end{tokens.data() + tokens.size()}, logger{log} {}

stela::Log &stela::ParseTokens::log() const {
  return logger;
}

bool stela::ParseTokens::empty() const {
  return beg == end;
}

size_t stela::ParseTokens::size() const {
  return end - beg;
}

const stela::Token &stela::ParseTokens::front() const {
  return *beg;
}

stela::Loc stela::ParseTokens::lastLoc() const {
  return beg[-1].loc;
}

const stela::Token *stela::ParseTokens::pos() const {
  return beg;
}

void stela::ParseTokens::pos(const Token *pos) {
  beg = pos;
}

stela::Context stela::ParseTokens::context(const std::string_view desc) {
  return ctxStack.context(desc);
}

const stela::ContextStack &stela::ParseTokens::contextStack() const {
  return ctxStack;
}

bool stela::ParseTokens::check(const Token::Type type, const std::string_view view) {
  if (empty() || beg->type != type || beg->view != view) {
    return false;
  } else {
    ++beg;
    return true;
  }
}

bool stela::ParseTokens::checkKeyword(const std::string_view view) {
  return check(Token::Type::keyword, view);
}

bool stela::ParseTokens::checkOp(const std::string_view view) {
  return check(Token::Type::oper, view);
}

std::string_view stela::ParseTokens::expect(const Token::Type type) {
  expectToken();
  if (beg->type != type) {
    logger.ferror(beg->loc) << "Expected " << type << " but found " << *beg
      << ctxStack << endlog;
  }
  const std::string_view view = beg->view;
  ++beg;
  return view;
}

void stela::ParseTokens::expect(const Token::Type type, const std::string_view view) {
  expectToken();
  if (beg->type != type || beg->view != view) {
    logger.ferror(beg->loc) << "Expected " << type << ' ' << view
      << " but found " << *beg << ctxStack << endlog;
  }
  ++beg;
}

std::string_view stela::ParseTokens::expectEither(
  const Token::Type type, const std::string_view a, const std::string_view b
) {
  expectToken();
  if (beg->type != type || (beg->view != a && beg->view != b)) {
    logger.ferror(beg->loc) << "Expected " << type << ' ' << a << " or " << b
      << " but found " << *beg << ctxStack << endlog;
  }
  const std::string_view view = beg->view;
  ++beg;
  return view;
}

std::string_view stela::ParseTokens::expectID() {
  return expect(Token::Type::identifier);
}

std::string_view stela::ParseTokens::expectOp() {
  return expect(Token::Type::oper);
}

void stela::ParseTokens::expectOp(const std::string_view view) {
  expect(Token::Type::oper, view);
}

std::string_view stela::ParseTokens::expectEitherOp(const std::string_view a, const std::string_view b) {
  return expectEither(Token::Type::oper, a, b);
}

void stela::ParseTokens::expectKeyword(const std::string_view view) {
  expect(Token::Type::keyword, view);
}

bool stela::ParseTokens::peekType(const Token::Type type) const {
  return !empty() && front().type == type;
}

bool stela::ParseTokens::peekOpType() const {
  return peekType(Token::Type::oper);
}

bool stela::ParseTokens::peekIdentType() const {
  return peekType(Token::Type::identifier);
}

void stela::ParseTokens::consume() {
  expectToken();
  ++beg;
}

void stela::ParseTokens::expectToken() {
  if (empty()) {
    logger.ferror() << "Unexpected end of input" << ctxStack << endlog;
  }
}
