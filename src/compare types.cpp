//
//  compare types.cpp
//  STELA
//
//  Created by Indi Kernick on 5/9/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "compare types.hpp"

#include "scope lookup.hpp"
#include <Simpleton/Utils/algorithm.hpp>

using namespace stela;

namespace {

template <typename Left>
class RightVisitor final : public ast::Visitor {
public:
  RightVisitor(sym::Ctx ctx, Left &left)
    : ctx{ctx}, left{left} {}

  void visit(ast::BtnType &right) override {
    eq = compare(ctx, left, right);
  }
  void visit(ast::ArrayType &right) override {
    eq = compare(ctx, left, right);
  }
  void visit(ast::FuncType &right) override {
    eq = compare(ctx, left, right);
  }
  void visit(ast::NamedType &right) override {
    ast::TypeAlias *def = lookupTypeName(ctx, right);
    if (def->strong) {
      eq = compare(ctx, left, right); // a strong type alias is its own type
    } else {
      def->type->accept(*this);  // a weak type alias is an existing type
    }
  }
  void visit(ast::StructType &right) override {
    eq = compare(ctx, left, right);
  }

  bool eq = false;

private:
  sym::Ctx ctx;
  Left &left;

  static bool compare(const sym::Ctx &, ast::BtnType &left, ast::BtnType &right) {
    return left.value == right.value;
  }
  static bool compare(const sym::Ctx &ctx, ast::ArrayType &left, ast::ArrayType &right) {
    return compareTypes(ctx, left.elem, right.elem);
  }
  static bool compare(const sym::Ctx &ctx, ast::FuncType &left, ast::FuncType &right) {
    const auto compareParams = [&ctx] (const ast::ParamType &a, const ast::ParamType &b) {
      return a.ref == b.ref && compareTypes(ctx, a.type, b.type);
    };
    if (!compareTypes(ctx, left.ret, right.ret)) {
      return false;
    }
    return Utils::equal_size(left.params, right.params, compareParams);
  }
  static bool compare(const sym::Ctx &, ast::NamedType &left, ast::NamedType &right) {
    return left.name == right.name;
  }
  static bool compare(const sym::Ctx &ctx, ast::StructType &left, ast::StructType &right) {
    const auto compareFields = [&ctx] (const ast::Field &a, const ast::Field &b) {
      return a.name == b.name && compareTypes(ctx, a.type, b.type);
    };
    return Utils::equal_size(left.fields, right.fields, compareFields);
  }
  template <typename Right>
  static bool compare(const sym::Ctx &, Left &, Right &) {
    return false;
  }
};

class LeftVisitor final : public ast::Visitor {
public:
  LeftVisitor(sym::Ctx ctx, ast::Type *right)
    : ctx{ctx}, right{right} {}

  void visit(ast::BtnType &left) override {
    visitImpl(left);
  }
  void visit(ast::ArrayType &left) override {
    visitImpl(left);
  }
  void visit(ast::FuncType &left) override {
    visitImpl(left);
  }
  void visit(ast::NamedType &left) override {
    ast::TypeAlias *def = lookupTypeName(ctx, left);
    if (def->strong) {
      visitImpl(left);          // a strong type alias is its own type
    } else {
      def->type->accept(*this); // a weak type alias is an existing type
    }
  }
  void visit(ast::StructType &left) override {
    visitImpl(left);
  }
  
  bool eq = false;

private:
  template <typename Type>
  void visitImpl(Type &left) {
    RightVisitor rVis{ctx, left};
    right->accept(rVis);
    eq = rVis.eq;
  }
  
  sym::Ctx ctx;
  ast::Type *right;
};

}

bool stela::compareTypes(sym::Ctx ctx, const ast::TypePtr &a, const ast::TypePtr &b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  LeftVisitor left{ctx, b.get()};
  a->accept(left);
  return left.eq;
}
