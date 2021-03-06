//
//  parse tokens.hpp
//  STELA
//
//  Created by Indi Kernick on 24/6/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_parse_tokens_hpp
#define stela_parse_tokens_hpp

#include "token.hpp"
#include "context stack.hpp"
#include "Log/log output.hpp"

namespace stela {

std::ostream &operator<<(std::ostream &, Token::Type);
std::ostream &operator<<(std::ostream &, const Token &);

class ParseTokens {
public:
  ParseTokens(const Tokens &, Log &);
  
  Log &log() const;
  bool empty() const;
  const Token &front() const;
  Loc lastLoc() const;
  Loc loc() const;
  
  [[nodiscard]] Context context(std::string_view);
  const ContextStack &contextStack() const;
  
  template <typename ParseFunc>
  auto expectNode(ParseFunc &&parse, const std::string_view msg) {
    auto node = parse(*this);
    if (node == nullptr) {
      logger.error(beg->loc) << "Expected " << msg << ctxStack << fatal;
    }
    return node;
  }
  
  bool check(Token::Type, std::string_view);
  bool checkKeyword(std::string_view);
  bool checkOp(std::string_view);
  
  std::string_view expect(Token::Type);
  void expect(Token::Type, std::string_view);
  std::string_view expectEither(Token::Type, std::string_view, std::string_view);
  
  std::string_view expectID();
  void expectOp(std::string_view);
  std::string_view expectEitherOp(std::string_view, std::string_view);
  
  bool peekType(Token::Type) const;
  bool peekIdentType() const;
  
  void extraSemi();

private:
  const Token *beg;
  const Token *end;
  Log &logger;
  ContextStack ctxStack;
  
  void expectToken();
};

}

#endif
