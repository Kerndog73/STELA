//
//  syntax analysis.cpp
//  STELA
//
//  Created by Indi Kernick on 21/6/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "syntax analysis.hpp"

#include "log output.hpp"
#include "parse tokens.hpp"
#include "lexical analysis.hpp"

using namespace stela;

namespace {

//--------------------------------- Types --------------------------------------

ast::ParamRef parseRef(ParseTokens &tok) {
  if (tok.checkKeyword("inout")) {
    return ast::ParamRef::inout;
  } else {
    return ast::ParamRef::value;
  }
}

ast::TypePtr parseType(ParseTokens &);

ast::TypePtr parseFuncType(ParseTokens &tok) {
  auto type = std::make_unique<ast::FuncType>();
  tok.expectOp("(");
  if (!tok.checkOp(")")) {
    do {
      ast::ParamType &param = type->params.emplace_back();
      param.ref = parseRef(tok);
      param.type = parseType(tok);
    } while (tok.expectEitherOp(")", ",") == ",");
  }
  tok.expectOp("->");
  type->ret = parseType(tok);
  return type;
}

ast::TypePtr parseType(ParseTokens &tok) {
  if (tok.checkOp("(")) {
    tok.unget();
    return parseFuncType(tok);
  } else if (tok.checkOp("[")) {
    // could be an array or a dictionary
    ast::TypePtr elem = parseType(tok);
    if (tok.checkOp(":")) {
      // it's a dictionary and `elem` is the key
      ast::TypePtr val = parseType(tok);
      tok.expectOp("]");
      auto dictType = std::make_unique<ast::DictType>();
      dictType->key = std::move(elem);
      dictType->val = std::move(val);
      return dictType;
    } else {
      // it's an array and `elem` is the element type
      tok.expectOp("]");
      auto arrayType = std::make_unique<ast::ArrayType>();
      arrayType->elem = std::move(elem);
      return arrayType;
    }
  } else {
    // not a function,
    // not an array,
    // not a dictionary
    // so it must be a named type like `Int` or `MyClass`
    auto namedType = std::make_unique<ast::NamedType>();
    namedType->name = tok.expectID();
    return namedType;
  }
}

//------------------------------ Expressions -----------------------------------

ast::ExprPtr parseExpr(ParseTokens &tok) {
  if (tok.check(Token::Type::identifier, "expr")) {
    return std::make_unique<ast::BinaryExpr>();
  } else {
    return nullptr;
  }
}

//------------------------------ Declarations ----------------------------------

ast::FuncParams parseFuncParams(ParseTokens &tok) {
  tok.expectOp("(");
  if (tok.checkOp(")")) {
    return {};
  }
  ast::FuncParams params;
  do {
    ast::FuncParam &param = params.emplace_back();
    param.name = tok.expectID();
    tok.expectOp(":");
    param.ref = parseRef(tok);
    param.type = tok.expectNode(parseType, "type");
  } while (tok.expectEitherOp(")", ",") == ",");
  return params;
}

ast::TypePtr parseFuncRet(ParseTokens &tok) {
  if (tok.checkOp("->")) {
    return tok.expectNode(parseType, "type");
  } else {
    return nullptr;
  }
}

ast::StatPtr parseBlock(ParseTokens &);

ast::DeclPtr parseFunc(ParseTokens &tok) {
  if (!tok.checkKeyword("func")) {
    return nullptr;
  }
  
  auto funcNode = std::make_unique<ast::Func>();
  funcNode->name = tok.expectID();
  funcNode->params = parseFuncParams(tok);
  funcNode->ret = parseFuncRet(tok);
  funcNode->body = parseBlock(tok);
  
  return funcNode;
}

ast::DeclPtr parseVar(ParseTokens &tok) {
  if (!tok.checkKeyword("var")) {
    return nullptr;
  }
  auto var = std::make_unique<ast::Var>();
  var->name = tok.expectID();
  if (tok.checkOp(":")) {
    var->type = tok.expectNode(parseType, "type after :");
  }
  if (tok.checkOp("=")) {
    var->expr = tok.expectNode(parseExpr, "expression after =");
  }
  tok.expectOp(";");
  return var;
}

ast::DeclPtr parseLet(ParseTokens &tok) {
  if (!tok.checkKeyword("let")) {
    return nullptr;
  }
  auto let = std::make_unique<ast::Let>();
  let->name = tok.expectID();
  if (tok.checkOp(":")) {
    let->type = tok.expectNode(parseType, "type after :");
  }
  tok.expectOp("=");
  let->expr = tok.expectNode(parseExpr, "expression after =");
  tok.expectOp(";");
  return let;
}

ast::DeclPtr parseTypealias(ParseTokens &tok) {
  if (!tok.checkKeyword("typealias")) {
    return nullptr;
  }
  auto aliasNode = std::make_unique<ast::TypeAlias>();
  aliasNode->name = tok.expectID();
  tok.expectOp("=");
  aliasNode->type = tok.expectNode(parseType, "type");
  tok.expectOp(";");
  return aliasNode;
}

ast::MemAccess parseMemAccess(ParseTokens &tok) {
  if (tok.checkKeyword("private")) {
    return ast::MemAccess::private_;
  } else {
    tok.checkKeyword("public");
    return ast::MemAccess::public_;
  }
}

ast::MemScope parseMemScope(ParseTokens &tok) {
  if (tok.checkKeyword("static")) {
    return ast::MemScope::static_;
  } else {
    return ast::MemScope::member;
  }
}

ast::DeclPtr parseDecl(ParseTokens &);

ast::Member parseStructMember(ParseTokens &tok) {
  ast::Member member;
  member.access = parseMemAccess(tok);
  member.scope = parseMemScope(tok);
  member.node = parseDecl(tok);
  return member;
}

ast::DeclPtr parseStruct(ParseTokens &tok) {
  if (!tok.checkKeyword("struct")) {
    return nullptr;
  }
  auto structNode = std::make_unique<ast::Struct>();
  structNode->name = tok.expectID();
  tok.expectOp("{");
  while(!tok.checkOp("}")) {
    structNode->body.push_back(parseStructMember(tok));
  }
  return structNode;
}

ast::EnumCase parseEnumCase(ParseTokens &tok) {
  ast::EnumCase ecase;
  ecase.name = tok.expectID();
  if (tok.checkOp("=")) {
    ecase.value = tok.expectNode(parseExpr, "expression or ,");
  }
  return ecase;
}

ast::DeclPtr parseEnum(ParseTokens &tok) {
  if (!tok.checkKeyword("enum")) {
    return nullptr;
  }
  auto enumNode = std::make_unique<ast::Enum>();
  enumNode->name = tok.expectID();
  tok.expectOp("{");
  
  if (!tok.checkOp("}")) {
    do {
      enumNode->cases.push_back(parseEnumCase(tok));
    } while (tok.expectEitherOp("}", ",") == ",");
  }
  
  return enumNode;
}

ast::DeclPtr parseDecl(ParseTokens &tok) {
  if (ast::DeclPtr node = parseFunc(tok)) return node;
  if (ast::DeclPtr node = parseVar(tok)) return node;
  if (ast::DeclPtr node = parseLet(tok)) return node;
  if (ast::DeclPtr node = parseTypealias(tok)) return node;
  if (ast::DeclPtr node = parseStruct(tok)) return node;
  if (ast::DeclPtr node = parseEnum(tok)) return node;
  return nullptr;
}

//------------------------------- Statements -----------------------------------

// an expression followed by a ; is a statement
ast::StatPtr parseExprStatement(ParseTokens &tok) {
  if (ast::StatPtr node = parseExpr(tok)) {
    tok.expectOp(";");
    return node;
  } else {
    return nullptr;
  }
}

ast::StatPtr parseStatement(ParseTokens &);

ast::StatPtr parseIf(ParseTokens &tok) {
  if (!tok.checkKeyword("if")) {
    return nullptr;
  }
  auto ifNode = std::make_unique<ast::If>();
  tok.expectOp("(");
  ifNode->cond = tok.expectNode(parseExpr, "condition expression");
  tok.expectOp(")");
  ifNode->body = tok.expectNode(parseStatement, "statement or block");
  if (tok.checkKeyword("else")) {
    ifNode->elseBody = tok.expectNode(parseStatement, "statement or block");
  }
  return ifNode;
}

ast::StatPtr parseSwitch(ParseTokens &tok) {
  if (!tok.checkKeyword("switch")) {
    return nullptr;
  }
  auto switchNode = std::make_unique<ast::Switch>();
  tok.expectOp("(");
  switchNode->expr = tok.expectNode(parseExpr, "expression");
  tok.expectOp(")");
  tok.expectOp("{");
  
  while (!tok.checkOp("}")) {
    if (ast::StatPtr stat = parseStatement(tok)) {
      switchNode->body.nodes.emplace_back(std::move(stat));
    } else if (tok.checkKeyword("case")) {
      auto scase = std::make_unique<ast::SwitchCase>();
      scase->expr = tok.expectNode(parseExpr, "expression");
      tok.expectOp(":");
      switchNode->body.nodes.emplace_back(std::move(scase));
    } else if (tok.checkKeyword("default")) {
      tok.expectOp(":");
      switchNode->body.nodes.emplace_back(std::make_unique<ast::SwitchDefault>());
    } else {
      tok.log().ferror(tok.front().loc) << "Expected case label or statement but found "
        << tok.front() << endlog;
    }
  }
  
  return switchNode;
}

template <typename Type>
ast::StatPtr parseKeywordStatement(ParseTokens &tok, const std::string_view name) {
  if (tok.checkKeyword(name)) {
    tok.expectOp(";");
    return std::make_unique<Type>();
  } else {
    return nullptr;
  }
}

ast::StatPtr parseBreak(ParseTokens &tok) {
  return parseKeywordStatement<ast::Break>(tok, "break");
}

ast::StatPtr parseContinue(ParseTokens &tok) {
  return parseKeywordStatement<ast::Continue>(tok, "continue");
}

ast::StatPtr parseFallthrough(ParseTokens &tok) {
  return parseKeywordStatement<ast::Fallthrough>(tok, "fallthrough");
}

ast::StatPtr parseReturn(ParseTokens &tok) {
  if (tok.checkKeyword("return")) {
    if (tok.checkOp(";")) {
      return std::make_unique<ast::Return>();
    }
    auto ret = std::make_unique<ast::Return>();
    ret->expr = tok.expectNode(parseExpr, "expression or ;");
    tok.expectOp(";");
    return ret;
  } else {
    return nullptr;
  }
}

ast::StatPtr parseWhile(ParseTokens &tok) {
  if (!tok.checkKeyword("while")) {
    return nullptr;
  }
  tok.expectOp("(");
  auto whileNode = std::make_unique<ast::While>();
  whileNode->cond = tok.expectNode(parseExpr, "condition expression");
  tok.expectOp(")");
  whileNode->body = tok.expectNode(parseStatement, "statement or block");
  return whileNode;
}

ast::StatPtr parseRepeatWhile(ParseTokens &tok) {
  if (!tok.checkKeyword("repeat")) {
    return nullptr;
  }
  auto repeat = std::make_unique<ast::RepeatWhile>();
  repeat->body = tok.expectNode(parseStatement, "statement or block");
  tok.expectKeyword("while");
  tok.expectOp("(");
  repeat->cond = tok.expectNode(parseExpr, "condition expression");
  tok.expectOp(")");
  tok.expectOp(";");
  return repeat;
}

ast::StatPtr parseForInit(ParseTokens &tok) {
  if (ast::StatPtr node = parseVar(tok)) return node;
  if (ast::StatPtr node = parseLet(tok)) return node;
  if (ast::StatPtr node = parseExpr(tok)) return node;
  return nullptr;
}

ast::StatPtr parseFor(ParseTokens &tok) {
  if (!tok.checkKeyword("for")) {
    return nullptr;
  }
  tok.expectOp("(");
  auto forNode = std::make_unique<ast::For>();
  forNode->init = parseForInit(tok); // init statement is optional
  if (forNode->init == nullptr) {
    tok.expectOp(";");
  }
  forNode->cond = tok.expectNode(parseExpr, "condition expression");
  tok.expectOp(";");
  forNode->incr = parseExpr(tok); // incr expression is optional
  tok.expectOp(")");
  forNode->body = tok.expectNode(parseStatement, "statement or block");
  return forNode;
}

ast::StatPtr parseBlock(ParseTokens &tok);

ast::StatPtr parseStatement(ParseTokens &tok) {
  if (ast::StatPtr node = parseIf(tok)) return node;
  if (ast::StatPtr node = parseSwitch(tok)) return node;
  if (ast::StatPtr node = parseBreak(tok)) return node;
  if (ast::StatPtr node = parseContinue(tok)) return node;
  if (ast::StatPtr node = parseFallthrough(tok)) return node;
  if (ast::StatPtr node = parseReturn(tok)) return node;
  if (ast::StatPtr node = parseWhile(tok)) return node;
  if (ast::StatPtr node = parseRepeatWhile(tok)) return node;
  if (ast::StatPtr node = parseFor(tok)) return node;
  if (ast::StatPtr node = parseBlock(tok)) return node;
  if (ast::StatPtr node = parseDecl(tok)) return node;
  if (ast::StatPtr node = parseExprStatement(tok)) return node;
  if (tok.checkOp(";")) return std::make_unique<ast::EmptyStatement>();
  return nullptr;
}

ast::StatPtr parseBlock(ParseTokens &tok) {
  if (tok.checkOp("{")) {
    auto block = std::make_unique<ast::Block>();
    while (ast::StatPtr node = parseStatement(tok)) {
      block->nodes.emplace_back(std::move(node));
    }
    tok.expectOp("}");
    return block;
  } else {
    return nullptr;
  }
}

//---------------------------------- Base --------------------------------------

AST createASTimpl(const Tokens &tokens, Log &log) {
  log.verbose() << "Parsing " << tokens.size() << " tokens" << endlog;

  AST ast;
  ParseTokens tok(tokens, log);
  
  while (!tok.empty()) {
    if (tok.checkOp(";")) {
      log.warn(tok.lastLoc()) << "Unnecessary ;" << endlog;
      continue;
    }
  
    if (ast::DeclPtr node = parseDecl(tok)) {
      ast.global.emplace_back(std::move(node));
    } else {
      const Token &token = tok.front();
      log.info(token.loc) << "Only declarations are allowed at global scope" << endlog;
      log.ferror(token.loc) << "Unexpected token " << token << " in global scope" << endlog;
    };
  }
  
  log.verbose() << "Created AST with " << ast.global.size() << " global nodes" << endlog;
  
  return ast;
}

}

AST stela::createAST(const Tokens &tokens, LogBuf &buf) {
  Log log{buf, LogCat::syntax};
  try {
    return createASTimpl(tokens, log);
  } catch (FatalError &) {
    throw;
  } catch (std::exception &e) {
    log.error() << e.what() << endlog;
    throw;
  }
}

AST stela::createAST(const std::string_view source, LogBuf &buf) {
  return createAST(lex(source, buf), buf);
}
