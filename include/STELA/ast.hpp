//
//  ast.hpp
//  STELA
//
//  Created by Indi Kernick on 21/6/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_ast_hpp
#define stela_ast_hpp

#include <vector>
#include <memory>
#include <string_view>
#include "location.hpp"

namespace stela::ast {

//---------------------------------- Base --------------------------------------

struct Node {
  virtual ~Node() = default;
  
  Loc loc;
};
using NodePtr = std::unique_ptr<Node>;

struct Type : Node {};
using TypePtr = std::unique_ptr<Type>;

struct Statement : Node {};
using StatPtr = std::unique_ptr<Statement>;

struct Expression : Statement {};
using ExprPtr = std::unique_ptr<Expression>;

struct Literal : Expression {};
using LitrPtr = std::unique_ptr<Literal>;

struct Declaration : Statement {};
using DeclPtr = std::unique_ptr<Declaration>;

using Name = std::string_view;

//--------------------------------- Types --------------------------------------

struct ArrayType final : Type {
  TypePtr elem;
};

struct MapType final : Type {
  TypePtr key;
  TypePtr val;
};

/// A function parameter can be passed by value or by reference (inout)
enum class ParamRef {
  value,
  inout
};

struct ParamType {
  ParamRef ref;
  TypePtr type;
};

struct FuncType final : Type {
  std::vector<ParamType> params;
  TypePtr ret;
};

struct NamedType final : Type {
  Name name;
};

struct BuiltinType final : Type {
  enum Enum {
    Void,
    Int,
    Char,
    Bool,
    Float,
    Double,
    String,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64
  } value;
};

//------------------------------ Expressions -----------------------------------

// NOTE:
//
// Update the ops table in parseExpr if the order of the operator enums changes
//

/// Assignment operator
enum class AssignOp {
  beg_ = 0,

  assign = beg_,
  add, sub, mul, div, mod, pow,
  bit_or, bit_xor, bit_and, bit_shl, bit_shr,
  
  end_
};

/// Binary operator
enum class BinOp {
  beg_ = static_cast<int>(AssignOp::end_),

  bool_or = beg_, bool_and,
  bit_or, bit_xor, bit_and,
  eq, ne, lt, le, gt, ge,
  bit_shl, bit_shr,
  add, sub, mul, div, mod, pow,
  
  end_
};

/// Unary operator
enum class UnOp {
  beg_ = static_cast<int>(BinOp::end_),
  
  neg = beg_,
  bool_not, bit_not,
  pre_incr, pre_decr,
  post_incr, post_decr,
  
  end_
};

constexpr int next_op_enum_beg = static_cast<int>(UnOp::end_);

constexpr int operator+(const AssignOp op) {
  return static_cast<int>(op);
}
constexpr int operator+(const BinOp op) {
  return static_cast<int>(op);
}
constexpr int operator+(const UnOp op) {
  return static_cast<int>(op);
}

struct Assignment final : Expression {
  ExprPtr left;
  AssignOp oper;
  ExprPtr right;
};

struct BinaryExpr final : Expression {
  ExprPtr left;
  BinOp oper;
  ExprPtr right;
};

struct UnaryExpr final : Expression {
  UnOp oper;
  ExprPtr expr;
};

struct FuncArg {
  ExprPtr expr;
};
using FuncArgs = std::vector<FuncArg>;

struct FuncCall final : Expression {
  ExprPtr func;
  FuncArgs args;
};

struct MemberIdent final : Expression {
  ExprPtr object;
  Name name;
};

struct InitCall final : Expression {
  TypePtr type;
  FuncArgs args;
};

struct Subscript final : Expression {
  ExprPtr object;
  ExprPtr index;
};

struct Identifier final : Expression {
  Name name;
};

struct Ternary final : Expression {
  ExprPtr cond;
  ExprPtr tru;
  ExprPtr fals;
};

//------------------------------- Statements -----------------------------------

struct Block final : Statement {
  std::vector<StatPtr> nodes;
};

struct EmptyStatement final : Statement {};

struct If final : Statement {
  ExprPtr cond;
  StatPtr body;
  StatPtr elseBody;
};

struct SwitchCase final : Statement {
  ExprPtr expr;
};

struct SwitchDefault final : Statement {};

struct Switch final : Statement {
  ExprPtr expr;
  Block body;
};

struct Break final : Statement {};
struct Continue final : Statement {};
struct Fallthrough final : Statement {};

struct Return final : Statement {
  ExprPtr expr;
};

struct While final : Statement {
  ExprPtr cond;
  StatPtr body;
};

struct RepeatWhile final : Statement {
  StatPtr body;
  ExprPtr cond;
};

struct For final : Statement {
  StatPtr init;
  ExprPtr cond;
  ExprPtr incr;
  StatPtr body;
};

//------------------------------ Declarations ----------------------------------

struct FuncParam {
  Name name;
  ParamRef ref;
  TypePtr type;
};
using FuncParams = std::vector<FuncParam>;

struct Func final : Declaration {
  Name name;
  FuncParams params;
  TypePtr ret;
  Block body;
};

struct Var final : Declaration {
  Name name;
  TypePtr type;
  ExprPtr expr;
};

struct Let final : Declaration {
  Name name;
  TypePtr type;
  ExprPtr expr;
};

struct TypeAlias final : Declaration {
  Name name;
  TypePtr type;
};

struct Init final : Declaration {
  FuncParams params;
  Block body;
};

/// Access level of a member
enum class MemAccess {
  public_,
  private_
};

/// The scope of a member is either in the object or on the type
enum class MemScope {
  member,
  static_
};

struct Member {
  MemAccess access;
  MemScope scope;
  DeclPtr node;
};

struct Struct final : Declaration {
  Name name;
  std::vector<Member> body;
};

struct EnumCase {
  Name name;
  ExprPtr value;
};

struct Enum final : Declaration {
  Name name;
  std::vector<EnumCase> cases;
};

//-------------------------------- Literals ------------------------------------

struct StringLiteral final : Literal {
  std::string_view value;
};

struct CharLiteral final : Literal {
  std::string_view value;
};

struct NumberLiteral final : Literal {
  std::string_view value;
};

struct BoolLiteral final : Literal {
  bool value;
};

struct ArrayLiteral final : Literal {
  std::vector<ExprPtr> exprs;
};

struct MapPair {
  ExprPtr key;
  ExprPtr val;
};

struct MapLiteral final : Literal {
  std::vector<MapPair> pairs;
};

struct Lambda final : Literal {
  FuncParams params;
  TypePtr ret;
  Block body;
};

}

namespace stela {

struct AST {
  std::vector<ast::StatPtr> global;
};

}

#endif
