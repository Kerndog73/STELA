//
//  generate expr.cpp
//  STELA
//
//  Created by Indi Kernick on 1/12/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "generate expr.hpp"

#include "symbols.hpp"
#include "unreachable.hpp"
#include "generate type.hpp"
#include "operator name.hpp"
#include "generate func.hpp"
#include "assert down cast.hpp"
#include "generate zero expr.hpp"

using namespace stela;

namespace {

class Visitor final : public ast::Visitor {
public:
  explicit Visitor(gen::Ctx ctx)
    : ctx{ctx} {}

  void visit(ast::BinaryExpr &expr) override {
    str += '(';
    expr.left->accept(*this);
    str += ") ";
    str += opName(expr.oper);
    str += " (";
    expr.right->accept(*this);
    str += ')';
  }
  void visit(ast::UnaryExpr &expr) override {
    str += opName(expr.oper);
    str += '(';
    expr.expr->accept(*this);
    str += ')';
  }
  
  void pushArgs(const ast::FuncArgs &args, const sym::FuncParams &) {
    for (size_t i = 0; i != args.size(); ++i) {
      // @TODO use address operator for ref parameters
      str += ", (";
      args[i]->accept(*this);
      str += ')';
    }
  }
  void pushBtnFunc(const ast::BtnFuncEnum e) {
    // @TODO X macros?
    switch (e) {
      case ast::BtnFuncEnum::capacity:
        str += "capacity"; return;
      case ast::BtnFuncEnum::size:
        str += "size"; return;
      case ast::BtnFuncEnum::push_back:
        str += "push_back"; return;
      case ast::BtnFuncEnum::append:
        str += "append"; return;
      case ast::BtnFuncEnum::pop_back:
        str += "pop_back"; return;
      case ast::BtnFuncEnum::resize:
        str += "resize"; return;
      case ast::BtnFuncEnum::reserve:
        str += "reserve"; return;
    }
    UNREACHABLE();
  }
  void visit(ast::FuncCall &call) override {
    if (call.definition == nullptr) {
      call.func->accept(*this);
      gen::String func = std::move(str);
      str += func;
      str += ".func(";
      str += func;
      str += ".data.get()";
      // @TODO get the parameter types of a function pointer
      pushArgs(call.args, {});
      str += ")";
    } else if (auto *func = dynamic_cast<ast::Func *>(call.definition)) {
      str += "f_";
      str += func->id;
      str += '(';
      if (func->receiver) {
        str += '(';
        assertDownCast<ast::MemberIdent>(call.func.get())->object->accept(*this);
        str += ')';
      } else {
        str += "nullptr";
      }
      pushArgs(call.args, func->symbol->params);
      str += ')';
    } else if (auto *btnFunc = dynamic_cast<ast::BtnFunc *>(call.definition)) {
      pushBtnFunc(btnFunc->value);
      str += '(';
      if (call.args.empty()) {
        str += ')';
        return;
      }
      str += '(';
      call.args[0]->accept(*this);
      str += ')';
      for (auto a = call.args.cbegin() + 1; a != call.args.cend(); ++a) {
        str += ", (";
        (*a)->accept(*this);
        str += ')';
      }
      str += ')';
    }
  }
  void visit(ast::MemberIdent &mem) override {
    mem.object->accept(*this);
    str += ".m_";
    str += mem.index;
  }
  void visit(ast::Subscript &sub) override {
    str += "index(";
    sub.object->accept(*this);
    str += ", ";
    sub.index->accept(*this);
    str += ')';
  }
  void writeID(ast::Statement *definition, ast::Type *exprType, ast::Type *expectedType) {
    // @TODO this function is too big. Chop, chop!
    assert(definition);
    gen::String name;
    if (auto *param = dynamic_cast<ast::FuncParam *>(definition)) {
      // @TODO uncomment when we move from references to pointers
      //str += "(";
      //if (param->ref == ast::ParamRef::ref) {
      //  str += "*";
      //}
      name += "p_";
      name += param->index;
      //str += ")";
    } else if (auto *decl = dynamic_cast<ast::DeclAssign *>(definition)) {
      name += "v_";
      name += decl->id;
    } else if (auto *var = dynamic_cast<ast::Var *>(definition)) {
      name += "v_";
      name += var->id;
    } else if (auto *let = dynamic_cast<ast::Let *>(definition)) {
      name += "v_";
      name += let->id;
    }
    if (!name.empty()) {
      auto *funcType = concreteType<ast::FuncType>(exprType);
      auto *btnType = concreteType<ast::BtnType>(expectedType);
      if (funcType && btnType && btnType->value == ast::BtnTypeEnum::Bool) {
        str += "(";
        str += name;
        str += ".func != &";
        str += generateNullFunc(ctx, *funcType);
        str += ")";
        return;
      }
      str += name;
    }
    if (auto *func = dynamic_cast<ast::Func *>(definition)) {
      auto *funcType = assertDownCast<ast::FuncType>(exprType);
      str += generateMakeFunc(ctx, *funcType);
      str += "(&f_";
      str += func->id;
      str += ")";
      return;
    }
    assert(!name.empty());
  }
  bool writeCapture(const uint32_t index) {
    if (index == ~uint32_t{}) {
      return false;
    } else {
      str += "capture.c_";
      str += index;
      return true;
    }
  }
  void visit(ast::Identifier &ident) override {
    if (!writeCapture(ident.captureIndex)) {
      writeID(ident.definition, ident.exprType.get(), ident.expectedType.get());
    }
  }
  void visit(ast::Ternary &tern) override {
    str += '(';
    tern.cond->accept(*this);
    str += ") ? (";
    tern.tru->accept(*this);
    str += ") : (";
    tern.fals->accept(*this);
    str += ')';
  }
  void visit(ast::Make &make) override {
    str += generateType(ctx, make.type.get());
    str += '(';
    make.expr->accept(*this);
    str += ')';
  }
  
  void visit(ast::StringLiteral &string) override {
    if (string.value.empty()) {
      str += "make_null_string()";
    } else {
      str += "string_literal(\"";
      str += string.value;
      str += "\")";
    }
  }
  void visit(ast::CharLiteral &chr) override {
    str += '\'';
    str += chr.value;
    str += '\'';
  }
  void visit(ast::NumberLiteral &num) override {
    str += generateType(ctx, num.exprType.get());
    str += '(';
    if (isalpha(num.value.back())) {
      str += std::string_view{num.value.data(), num.value.size() - 1};
    } else {
      str += num.value;
    }
    str += ')';
  }
  void visit(ast::BoolLiteral &bol) override {
    if (bol.value) {
      str += "true";
    } else {
      str += "false";
    }
  }
  void pushExprs(const std::vector<ast::ExprPtr> &exprs) {
    if (exprs.empty()) {
      return;
    }
    str += '(';
    exprs[0]->accept(*this);
    str += ')';
    for (auto e = exprs.cbegin() + 1; e != exprs.cend(); ++e) {
      str += ", (";
      (*e)->accept(*this);
      str += ')';
    }
  }
  void visit(ast::ArrayLiteral &arr) override {
    ast::TypePtr elem = assertDownCast<ast::ArrayType>(arr.exprType.get())->elem;
    if (arr.exprs.empty()) {
      str += "make_null_array<";
      str += generateType(ctx, elem.get());
      str += ">()";
    } else {
      str += "array_literal<";
      str += generateType(ctx, elem.get());
      str += ">(";
      pushExprs(arr.exprs);
      str += ')';
    }
  }
  void visit(ast::InitList &list) override {
    if (list.exprs.empty()) {
      str += generateZeroExpr(ctx, list.exprType.get());
    } else {
      str += generateType(ctx, list.exprType.get());
      str += '{';
      pushExprs(list.exprs);
      str += '}';
    }
  }
  void writeCapture(const sym::ClosureCap &cap) {
    if (!writeCapture(cap.index)) {
      writeID(cap.object, cap.type.get(), nullptr);
    }
  }
  void visit(ast::Lambda &lambda) override {
    str += generateMakeLam(ctx, lambda);
    str += "({";
    sym::Lambda *symbol = lambda.symbol;
    if (symbol->captures.empty()) {
      str += "})";
      return;
    } else {
      writeCapture(symbol->captures[0]);
    }
    for (auto c = symbol->captures.cbegin() + 1; c != symbol->captures.cend(); ++c) {
      str += ", ";
      writeCapture(*c);
    }
    str += "})";
  }

  gen::String str;

private:
  gen::Ctx ctx;
};

}

gen::String stela::generateExpr(gen::Ctx ctx, ast::Expression *expr) {
  Visitor visitor{ctx};
  expr->accept(visitor);
  return std::move(visitor.str);
}