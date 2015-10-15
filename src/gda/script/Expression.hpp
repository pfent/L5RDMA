//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - Contains expressions of the script language.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include "ScriptLanguage.hpp"
#include <memory>
#include <string>
#include <iostream>
#include <stack>
// -------------------------------------------------------------------------------------------------
// TODO: eliminate massive memory allocations during evaluation .. ideas:
// 1. use a fixed allocator
// 2. use const return types and let values return *this
// -------------------------------------------------------------------------------------------------
namespace gda {
//---------------------------------------------------------------------------
namespace harriet {
// -------------------------------------------------------------------------------------------------
class Environment;
class Value;
// -------------------------------------------------------------------------------------------------
class Expression {
public:
   static std::unique_ptr<Expression> parse(const std::string& input);
   virtual void print() const = 0;

   static std::unique_ptr<Expression> createCast(std::unique_ptr<Expression> expression, VariableType resultType);

   virtual std::unique_ptr<Value> evaluate(Environment& environment) const = 0;

protected:
   /// for shunting yard -- pharentesis are ONLY used during parsing
   enum struct SimpleType
   : uint8_t {
      TValue, TVariable, TUnaryOperator, TBinaryOperator, TOpeningPharentesis, TClosingPharentesis
   };
   virtual SimpleType getSimpleType() const = 0;
   friend class AssignmentOperator;

   /// left *,+,-,/,% right *nothing* for shunting yard
   enum struct Associativity : uint8_t { TLeft, TRight };
   virtual Associativity getAssociativity() const = 0;

   /// assign a priority for shunting yard
   virtual uint8_t priority() const = 0;

   /// helper functions
   static bool isUnaryContext(SimpleType last);
   static std::unique_ptr<Expression> parseSingleExpression(std::istream& input, SimpleType lastExpression);
   static void pushToOutput(std::stack<std::unique_ptr<Expression>>& workStack, std::unique_ptr<Expression> element);
};
// -------------------------------------------------------------------------------------------------
class Variable : public Expression {
public:
   Variable(const std::string& identifier)
   : identifier(identifier) { }
   virtual void print() const;
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
   const std::string& getIdentifier() const { return identifier; }
protected:
   virtual uint8_t priority() const { throw; }
   virtual SimpleType getSimpleType() const { return SimpleType::TVariable; }
   virtual Associativity getAssociativity() const { throw; }
   std::string identifier;
};
// -------------------------------------------------------------------------------------------------
class Value : public Expression {
public:
   virtual VariableType getResultType() const = 0;

   virtual std::unique_ptr<Value> computeAdd(const Value& rhs, const Environment& /*env*/) const
   {
      doError("+", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeSub(const Value& rhs, const Environment& /*env*/) const
   {
      doError("-", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeMul(const Value& rhs, const Environment& /*env*/) const
   {
      doError("*", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeDiv(const Value& rhs, const Environment& /*env*/) const
   {
      doError("/", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeMod(const Value& rhs, const Environment& /*env*/) const
   {
      doError("%", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeExp(const Value& rhs, const Environment& /*env*/) const
   {
      doError("^", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeAnd(const Value& rhs, const Environment& /*env*/) const
   {
      doError("&", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeOr(const Value& rhs, const Environment& /*env*/) const
   {
      doError("|", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeGt(const Value& rhs, const Environment& /*env*/) const
   {
      doError(">", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeLt(const Value& rhs, const Environment& /*env*/) const
   {
      doError("<", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeGeq(const Value& rhs, const Environment& /*env*/) const
   {
      doError(">=", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeLeq(const Value& rhs, const Environment& /*env*/) const
   {
      doError("<=", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeEq(const Value& rhs, const Environment& /*env*/) const
   {
      doError("==", *this, rhs);
      throw;
   }
   virtual std::unique_ptr<Value> computeNeq(const Value& rhs, const Environment& /*env*/) const
   {
      doError("!=", *this, rhs);
      throw;
   }

   virtual std::unique_ptr<Value> computeInv(const Environment& /*env*/) const
   {
      doError("-", *this);
      throw;
   }
   virtual std::unique_ptr<Value> computeNot(const Environment& /*env*/) const
   {
      doError("!", *this);
      throw;
   }

   virtual std::unique_ptr<Value> computeCast(const Environment& /*env*/, VariableType resultType) const { throw Exception{"unable to cast '" + typeToName(getResultType()) + "' to '" + typeToName(resultType) + "'"}; }

protected:
   virtual uint8_t priority() const { return 0; }
   virtual SimpleType getSimpleType() const { return SimpleType::TValue; }
   virtual Associativity getAssociativity() const { throw; }

   static void doError(const std::string& operatorSign, const Value& lhs, const Value& rhs) throw(Exception) { throw Exception{"binary operator '" + operatorSign + "' does not accept '" + typeToName(lhs.getResultType()) + "' and '" + typeToName(rhs.getResultType()) + "'"}; }
   static void doError(const std::string& operatorSign, const Value& lhs) throw(Exception) { throw Exception{"unary operator '" + operatorSign + "' does not accept '" + typeToName(lhs.getResultType()) + "'"}; }
};
// -------------------------------------------------------------------------------------------------
struct IntegerValue : public Value {
   virtual void print() const;
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
   int32_t result;
   IntegerValue(int32_t result)
   : result(result) { }
   virtual VariableType getResultType() const { return VariableType::TInteger; }

   virtual std::unique_ptr<Value> computeAdd(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeSub(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeMul(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeDiv(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeMod(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeExp(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeAnd(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeOr(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeGt(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeLt(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeGeq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeLeq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeEq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeNeq(const Value& rhs, const Environment& env) const;

   virtual std::unique_ptr<Value> computeInv(const Environment& env) const;

   virtual std::unique_ptr<Value> computeCast(const Environment& env, VariableType resultType) const;
};
// -------------------------------------------------------------------------------------------------
struct FloatValue : public Value {
   virtual void print() const;
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
   float result;
   FloatValue(float result)
   : result(result) { }
   virtual VariableType getResultType() const { return VariableType::TFloat; }

   virtual std::unique_ptr<Value> computeAdd(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeSub(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeMul(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeDiv(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeMod(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeExp(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeGt(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeLt(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeGeq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeLeq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeEq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeNeq(const Value& rhs, const Environment& env) const;

   virtual std::unique_ptr<Value> computeInv(const Environment& env) const;

   virtual std::unique_ptr<Value> computeCast(const Environment& env, VariableType resultType) const;
};
// -------------------------------------------------------------------------------------------------
struct BoolValue : public Value {
   virtual void print() const;
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
   bool result;
   BoolValue(bool result)
   : result(result) { }
   virtual VariableType getResultType() const { return VariableType::TBool; }

   virtual std::unique_ptr<Value> computeAnd(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeOr(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeEq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeNeq(const Value& rhs, const Environment& env) const;

   virtual std::unique_ptr<Value> computeNot(const Environment& env) const;

   virtual std::unique_ptr<Value> computeCast(const Environment& env, VariableType resultType) const;
};
// -------------------------------------------------------------------------------------------------
struct StringValue : public Value {
   virtual void print() const;
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
   std::string result;
   StringValue(const std::string& result)
   : result(result) { }
   virtual VariableType getResultType() const { return VariableType::TString; }

   virtual std::unique_ptr<Value> computeAdd(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeGt(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeLt(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeGeq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeLeq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeEq(const Value& rhs, const Environment& env) const;
   virtual std::unique_ptr<Value> computeNeq(const Value& rhs, const Environment& env) const;

   virtual std::unique_ptr<Value> computeCast(const Environment& env, VariableType resultType) const;
};
// -------------------------------------------------------------------------------------------------
class UnaryOperator : public Expression {
   virtual void print() const;
protected:
   virtual void addChild(std::unique_ptr<Expression> child);
   virtual SimpleType getSimpleType() const { return SimpleType::TUnaryOperator; }
   std::unique_ptr<Expression> child;
   virtual const std::string getSign() const = 0;
   friend class Expression;
};
// -------------------------------------------------------------------------------------------------
class UnaryMinusOperator : public UnaryOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TRight; }
   virtual uint8_t priority() const { return 3; }
   virtual const std::string getSign() const { return "-"; }
};
// -------------------------------------------------------------------------------------------------
class NotOperator : public UnaryOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TRight; }
   virtual uint8_t priority() const { return 3; }
   virtual const std::string getSign() const { return "!"; }
};
// -------------------------------------------------------------------------------------------------
class CastOperator : public UnaryOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const; // uses getCastType to determin the result type
protected:
   virtual VariableType getCastType() const = 0;
   virtual Associativity getAssociativity() const { return Associativity::TRight; }
   virtual uint8_t priority() const { return 3; }
};
// -------------------------------------------------------------------------------------------------
class IntegerCast : public CastOperator {
   virtual VariableType getCastType() const { return VariableType::TInteger; }
   virtual const std::string getSign() const { return "cast<int>"; }
};
// -------------------------------------------------------------------------------------------------
class FloatCast : public CastOperator {
   virtual VariableType getCastType() const { return VariableType::TFloat; }
   virtual const std::string getSign() const { return "cast<float>"; }
};
// -------------------------------------------------------------------------------------------------
class BoolCast : public CastOperator {
   virtual VariableType getCastType() const { return VariableType::TBool; }
   virtual const std::string getSign() const { return "cast<bool>"; }
};
// -------------------------------------------------------------------------------------------------
class StringCast : public CastOperator {
   virtual VariableType getCastType() const { return VariableType::TString; }
   virtual const std::string getSign() const { return "cast<string>"; }
};
// -------------------------------------------------------------------------------------------------
class BinaryOperator : public Expression {
   virtual void print() const;
protected:
   virtual void addChildren(std::unique_ptr<Expression> lhsChild, std::unique_ptr<Expression> rhsChild);
   virtual SimpleType getSimpleType() const { return SimpleType::TBinaryOperator; }
   std::unique_ptr<Expression> lhs;
   std::unique_ptr<Expression> rhs;
   virtual const std::string getSign() const = 0;
   friend class Expression;
};
// -------------------------------------------------------------------------------------------------
class AssignmentOperator : public BinaryOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TRight; }
   virtual uint8_t priority() const { return 16; }
   virtual const std::string getSign() const { return "="; }
};
// -------------------------------------------------------------------------------------------------
class ArithmeticOperator : public BinaryOperator {
};
// -------------------------------------------------------------------------------------------------
class PlusOperator : public ArithmeticOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 6; }
   virtual const std::string getSign() const { return "+"; }
};
// -------------------------------------------------------------------------------------------------
class MinusOperator : public ArithmeticOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 6; }
   virtual const std::string getSign() const { return "-"; }
};
// -------------------------------------------------------------------------------------------------
class MultiplicationOperator : public ArithmeticOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 5; }
   virtual const std::string getSign() const { return "*"; }
};
// -------------------------------------------------------------------------------------------------
class DivisionOperator : public ArithmeticOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 5; }
   virtual const std::string getSign() const { return "/"; }
};
// -------------------------------------------------------------------------------------------------
class ModuloOperator : public ArithmeticOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 5; }
   virtual const std::string getSign() const { return "%"; }
};
// -------------------------------------------------------------------------------------------------
class ExponentiationOperator : public ArithmeticOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TRight; }
   virtual uint8_t priority() const { return 3; }
   virtual const std::string getSign() const { return "%"; }
};
// -------------------------------------------------------------------------------------------------
class LogicOperator : public BinaryOperator {
};
// -------------------------------------------------------------------------------------------------
class AndOperator : public LogicOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 10; }
   virtual const std::string getSign() const { return "&"; }
};
// -------------------------------------------------------------------------------------------------
class OrOperator : public LogicOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 12; }
   virtual const std::string getSign() const { return "|"; }
};
// -------------------------------------------------------------------------------------------------
class ComparisonOperator : public BinaryOperator {
};
// -------------------------------------------------------------------------------------------------
class GreaterOperator : public ComparisonOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 8; }
   virtual const std::string getSign() const { return ">"; }
};
// -------------------------------------------------------------------------------------------------
class LessOperator : public ComparisonOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 8; }
   virtual const std::string getSign() const { return "<"; }
};
// -------------------------------------------------------------------------------------------------
class GreaterEqualOperator : public ComparisonOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 8; }
   virtual const std::string getSign() const { return ">="; }
};
// -------------------------------------------------------------------------------------------------
class LessEqualOperator : public ComparisonOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 8; }
   virtual const std::string getSign() const { return "<="; }
};
// -------------------------------------------------------------------------------------------------
class EqualOperator : public ComparisonOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 9; }
   virtual const std::string getSign() const { return "=="; }
};
// -------------------------------------------------------------------------------------------------
class NotEqualOperator : public ComparisonOperator {
   virtual std::unique_ptr<Value> evaluate(Environment& environment) const;
protected:
   virtual Associativity getAssociativity() const { return Associativity::TLeft; }
   virtual uint8_t priority() const { return 9; }
   virtual const std::string getSign() const { return "!="; }
};
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
}
