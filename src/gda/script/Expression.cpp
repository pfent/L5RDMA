#include "gda/script/Expression.hpp"
#include "gda/Utility.hpp"
#include "gda/Conversion.hpp"
#include "gda/String.hpp"
#include "gda/script/Environment.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <stack>
#include <cassert>
#include <cmath>
//---------------------------------------------------------------------------
//
//
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace gda {
//---------------------------------------------------------------------------
namespace harriet {
//---------------------------------------------------------------------------
struct OpeningPharentesis : public Expression {
   virtual void print() const { cout << " ( "; }
   virtual uint8_t priority() const { return 0; }
   virtual SimpleType getSimpleType() const { return SimpleType::TOpeningPharentesis; }
   virtual Associativity getAssociativity() const { throw; }
   virtual unique_ptr<Value> evaluate(Environment& /*environment*/) const { throw; }
};
//---------------------------------------------------------------------------
struct ClosingPharentesis : public Expression {
   virtual void print() const { cout << " ) "; }
   virtual uint8_t priority() const { throw; }
   virtual SimpleType getSimpleType() const { return SimpleType::TClosingPharentesis; }
   virtual Associativity getAssociativity() const { throw; }
   virtual unique_ptr<Value> evaluate(Environment& /*environment*/) const { throw; }
};
//---------------------------------------------------------------------------
bool Expression::isUnaryContext(SimpleType last)
{
   return last == SimpleType::TBinaryOperator || last == SimpleType::TUnaryOperator || last == SimpleType::TOpeningPharentesis;
}
//---------------------------------------------------------------------------
unique_ptr<Expression> Expression::parseSingleExpression(istream& input, SimpleType lastExpression)
{
   // check for one signed letters
   char a = input.get();
   if (!input.good())
      return nullptr;
   if (a == '(') return gda::make_unique<OpeningPharentesis>();
   if (a == ')') return gda::make_unique<ClosingPharentesis>();
   if (a == '+') return gda::make_unique<PlusOperator>();
   if (a == '-') { if (isUnaryContext(lastExpression)) return gda::make_unique<UnaryMinusOperator>(); else return gda::make_unique<MinusOperator>(); }
   if (a == '*') return gda::make_unique<MultiplicationOperator>();
   if (a == '/') return gda::make_unique<DivisionOperator>();
   if (a == '%') return gda::make_unique<ModuloOperator>();
   if (a == '^') return gda::make_unique<ExponentiationOperator>();
   if (a == '&') return gda::make_unique<AndOperator>();
   if (a == '|') return gda::make_unique<OrOperator>();
   if (a == '>' && input.peek() != '=') return gda::make_unique<GreaterOperator>();
   if (a == '<' && input.peek() != '=') return gda::make_unique<LessOperator>();
   if (a == '!' && input.peek() != '=') return gda::make_unique<NotOperator>();
   if (a == '=' && input.peek() != '=') return gda::make_unique<AssignmentOperator>();

   // check for string
   char b = input.get();
   if (a == '"') {
      string result;
      while (b != '"' && a != '\\') {
         if (!input.good())
            throw Exception{"unterminated string expression"};
         result.push_back(b);
         a = b;
         b = input.get();
      }
      return gda::make_unique<StringValue>(result);
   }

   // check for two signed letters
   if (input.good()) {
      if (a == '=' && b == '=') return gda::make_unique<EqualOperator>();
      if (a == '>' && b == '=') return gda::make_unique<GreaterEqualOperator>();
      if (a == '<' && b == '=') return gda::make_unique<LessEqualOperator>();
      if (a == '!' && b == '=') return gda::make_unique<NotEqualOperator>();
      input.unget();
   } else {
      input.clear();
   }
   input.unget();

   // check for a number
   if (isdigit(a)) {
      int32_t intNum;
      input >> intNum;
      if (input.peek() == '.' && input.good()) {
         float floatNum;
         input >> floatNum;
         return gda::make_unique<FloatValue>(floatNum + intNum);
      } else {
         return gda::make_unique<IntegerValue>(intNum);
      }
   }

   // read the name
   if (isalpha(a)) {
      string word = parseIdentifier(input);
      if (word == kTrue) return gda::make_unique<BoolValue>(true);
      if (word == kFalse) return gda::make_unique<BoolValue>(false);
      if (word == kCastName) {
         // extract '<', read type and extract '>'
         a = input.get();
         if (a != '<')
            throw Exception{string("invalid cast syntax, expected '<' got '") + a + "'. usage: cast<type> value"};
         VariableType type = nameToType(readOnlyAlpha(input));
         a = input.get();
         if (a != '>')
            throw Exception{string("invalid cast syntax, expected '>' got '") + a + "'. usage: cast<type> value"};

         // create cast operator
         switch (type) {
            case VariableType::TInteger:
               return gda::make_unique<IntegerCast>();
            case VariableType::TFloat:
               return gda::make_unique<FloatCast>();
            case VariableType::TBool:
               return gda::make_unique<BoolCast>();
            case VariableType::TString:
               return gda::make_unique<StringCast>();
         }
         return gda::make_unique<BoolValue>(false);
      }
      return gda::make_unique<Variable>(word);
   }

   throw Exception{"unable to parse expression"};
}
//---------------------------------------------------------------------------
void Expression::pushToOutput(stack<unique_ptr<Expression>>& workStack, unique_ptr<Expression> element)
{
   if (element->getSimpleType() == SimpleType::TValue || element->getSimpleType() == SimpleType::TVariable) {
      workStack.push(::move(element));
      return;
   }

   if (element->getSimpleType() == SimpleType::TUnaryOperator) {
      if (workStack.size() < 1)
         throw Exception{"to few arguments for unaray operator " + reinterpret_cast<UnaryOperator*>(element.get())->getSign()};
      auto operand = ::move(workStack.top());
      workStack.pop();
      reinterpret_cast<UnaryOperator*>(element.get())->addChild(::move(operand));
      workStack.push(::move(element));
      return;
   }

   if (element->getSimpleType() == SimpleType::TBinaryOperator) {
      if (workStack.size() < 2)
         throw Exception{"to few arguments for binary operator " + reinterpret_cast<BinaryOperator*>(element.get())->getSign()};
      auto rhs = ::move(workStack.top());
      workStack.pop();
      auto lhs = ::move(workStack.top());
      workStack.pop();
      reinterpret_cast<BinaryOperator*>(element.get())->addChildren(::move(lhs), ::move(rhs));
      workStack.push(::move(element));
      return;
   }
}
//---------------------------------------------------------------------------
unique_ptr<Expression> Expression::parse(const string& inputString)
{
   // set up data
   istringstream input(inputString);
   stack<unique_ptr<Expression>> outputStack;
   stack<unique_ptr<Expression>> operatorStack;
   skipWhiteSpace(input);
   SimpleType lastExpressionType = SimpleType::TOpeningPharentesis; // needed for not context free operators

   // parse input and build PRN on the fly
   while (input.good()) {
      // get next token
      auto token = parseSingleExpression(input, lastExpressionType);
      lastExpressionType = token->getSimpleType();
      if (token == nullptr)
         break;
      skipWhiteSpace(input);

      // shunting yard algorithm -- proces tokens
      switch (token->getSimpleType()) {
         case SimpleType::TVariable:
         case SimpleType::TValue:
            pushToOutput(outputStack, ::move(token));
            continue;
         case SimpleType::TBinaryOperator:
         case SimpleType::TUnaryOperator:
            while ((!operatorStack.empty() && operatorStack.top()->getSimpleType() != SimpleType::TOpeningPharentesis)
            && ((token->getAssociativity() == Associativity::TLeft && token->priority() >= operatorStack.top()->priority())
            || (token->getAssociativity() == Associativity::TRight && token->priority() > operatorStack.top()->priority()))) {
               auto stackToken = ::move(operatorStack.top());
               operatorStack.pop();
               pushToOutput(outputStack, ::move(stackToken));
            }
            operatorStack.push(::move(token));
            continue;
         case SimpleType::TOpeningPharentesis:
            operatorStack.push(::move(token));
            continue;
         case SimpleType::TClosingPharentesis:
            while (true) {
               if (operatorStack.empty())
                  throw Exception{"parenthesis missmatch: missing '('"};
               auto stackToken = ::move(operatorStack.top());
               operatorStack.pop();
               if (stackToken->getSimpleType() == SimpleType::TOpeningPharentesis)
                  break;
               else
                  pushToOutput(outputStack, ::move(stackToken));
            }
      }
   }

   // shunting yard algorithm -- clear stack
   while (!operatorStack.empty()) {
      auto stackToken = ::move(operatorStack.top());
      operatorStack.pop();
      if (stackToken->getSimpleType() == SimpleType::TOpeningPharentesis)
         throw Exception{"parenthesis missmatch: missing ')'"};
      pushToOutput(outputStack, ::move(stackToken));
   }

   // check type integrity
   assert(outputStack.size() == 1);
   return ::move(outputStack.top());
}
//---------------------------------------------------------------------------
unique_ptr<Expression> Expression::createCast(unique_ptr<Expression> expression, VariableType resultType)
{
   unique_ptr<CastOperator> result;
   switch (resultType) {
      case VariableType::TInteger:
         result = gda::make_unique<IntegerCast>();
         break;
      case VariableType::TFloat:
         result = gda::make_unique<FloatCast>();
         break;
      case VariableType::TBool:
         result = gda::make_unique<BoolCast>();
         break;
      case VariableType::TString:
         result = gda::make_unique<StringCast>();
         break;
   }
   result->addChild(::move(expression));
   return ::move(result);
}
//---------------------------------------------------------------------------
void Variable::print() const
{
   cout << identifier << " ";
}
//---------------------------------------------------------------------------
unique_ptr<Value> Variable::evaluate(Environment& environment) const
{
   const Value& result = environment.read(identifier);
   switch (result.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(reinterpret_cast<const IntegerValue&>(result).result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(reinterpret_cast<const FloatValue&>(result).result);
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(reinterpret_cast<const BoolValue&>(result).result);
      case VariableType::TString:
         return gda::make_unique<StringValue>(reinterpret_cast<const StringValue&>(result).result);
   }
   throw;
}
//---------------------------------------------------------------------------
void IntegerValue::print() const
{
   cout << result << "i ";
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::evaluate(Environment& /*environment*/) const
{
   return gda::make_unique<IntegerValue>(result);
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeAdd(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result + reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result + reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '+'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeSub(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result - reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result - reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '-'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeMul(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result * reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result * reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '*'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeDiv(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result / reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result / reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '/'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeMod(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result % reinterpret_cast<const IntegerValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '%'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeExp(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(static_cast<int32_t>(pow(this->result, reinterpret_cast<const IntegerValue*>(&rhs)->result)));
      case VariableType::TFloat:
         return gda::make_unique<IntegerValue>(static_cast<float>(pow(this->result, reinterpret_cast<const FloatValue*>(&rhs)->result)));
      default:
         throw Exception{"invalid input for binary operator '^'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeAnd(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result & reinterpret_cast<const IntegerValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '&'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeOr(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result | reinterpret_cast<const IntegerValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '|'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeGt(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result > reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result > reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '>'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeLt(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result < reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result < reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '<'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeGeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result >= reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result >= reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '>='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeLeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result >= reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result <= reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '<='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeEq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result == reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result == reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '=='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeNeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result != reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result != reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '!='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeInv(const Environment& /*env*/) const
{
   return gda::make_unique<IntegerValue>(-this->result);
}
//---------------------------------------------------------------------------
unique_ptr<Value> IntegerValue::computeCast(const Environment& /*env*/, VariableType resultType) const
{
   switch (resultType) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result);
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result != 0);
      case VariableType::TString:
         return gda::make_unique<StringValue>(to_string(this->result));
   }
   throw;
}
//---------------------------------------------------------------------------
void FloatValue::print() const
{
   cout << result << "f ";
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::evaluate(Environment& /*environment*/) const
{
   return gda::make_unique<FloatValue>(result);
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeAdd(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<FloatValue>(this->result + reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result + reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '+'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeSub(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<FloatValue>(this->result - reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result - reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '-'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeMul(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<FloatValue>(this->result * reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result * reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '*'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeDiv(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<FloatValue>(this->result / reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result / reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '/'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeMod(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<FloatValue>(static_cast<int32_t>(this->result) % reinterpret_cast<const IntegerValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '%'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeExp(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(static_cast<float>(pow(this->result, reinterpret_cast<const IntegerValue*>(&rhs)->result)));
      case VariableType::TFloat:
         return gda::make_unique<IntegerValue>(static_cast<float>(pow(this->result, reinterpret_cast<const FloatValue*>(&rhs)->result)));
      default:
         throw Exception{"invalid input for binary operator '^'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeGt(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result > reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result > reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '>'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeLt(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result < reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result < reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '<'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeGeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result >= reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result >= reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '>='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeLeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result >= reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result <= reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '<='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeEq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result == reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result == reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '=='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeNeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TInteger:
         return gda::make_unique<BoolValue>(this->result != reinterpret_cast<const IntegerValue*>(&rhs)->result);
      case VariableType::TFloat:
         return gda::make_unique<BoolValue>(this->result != reinterpret_cast<const FloatValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '!='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeInv(const Environment& /*env*/) const
{
   return gda::make_unique<FloatValue>(-this->result);
}
//---------------------------------------------------------------------------
unique_ptr<Value> FloatValue::computeCast(const Environment& /*env*/, VariableType resultType) const
{
   switch (resultType) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result);
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result != 0);
      case VariableType::TString:
         return gda::make_unique<StringValue>(to_string(this->result));
   }
   throw;
}
//---------------------------------------------------------------------------
void BoolValue::print() const
{
   cout << (result ? kTrue : kFalse) << " ";
}
//---------------------------------------------------------------------------
unique_ptr<Value> BoolValue::evaluate(Environment& /*environment*/) const
{
   return gda::make_unique<BoolValue>(result);
}
//---------------------------------------------------------------------------
unique_ptr<Value> BoolValue::computeAnd(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result & reinterpret_cast<const BoolValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '&'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> BoolValue::computeOr(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result | reinterpret_cast<const BoolValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '|'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> BoolValue::computeEq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result == reinterpret_cast<const BoolValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '=='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> BoolValue::computeNeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result != reinterpret_cast<const BoolValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '!='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> BoolValue::computeNot(const Environment& /*env*/) const
{
   return gda::make_unique<BoolValue>(!this->result);
}
//---------------------------------------------------------------------------
unique_ptr<Value> BoolValue::computeCast(const Environment& /*env*/, VariableType resultType) const
{
   switch (resultType) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(this->result);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(this->result);
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result);
      case VariableType::TString:
         return gda::make_unique<StringValue>(this->result ? kTrue : kFalse);
   }
   throw;
}
//---------------------------------------------------------------------------
void StringValue::print() const
{
   cout << "\"" << result << "\" ";
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::evaluate(Environment& /*environment*/) const
{
   return gda::make_unique<StringValue>(result);
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeAdd(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TString:
         return gda::make_unique<StringValue>(this->result + reinterpret_cast<const StringValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '+'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeGt(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TString:
         return gda::make_unique<BoolValue>(this->result > reinterpret_cast<const StringValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '>'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeLt(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TString:
         return gda::make_unique<BoolValue>(this->result < reinterpret_cast<const StringValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '<'"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeGeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TString:
         return gda::make_unique<BoolValue>(this->result >= reinterpret_cast<const StringValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '>='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeLeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TString:
         return gda::make_unique<BoolValue>(this->result <= reinterpret_cast<const StringValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '<='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeEq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TString:
         return gda::make_unique<BoolValue>(this->result == reinterpret_cast<const StringValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '=='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeNeq(const Value& rhs, const Environment& /*env*/) const
{
   switch (rhs.getResultType()) {
      case VariableType::TString:
         return gda::make_unique<BoolValue>(this->result != reinterpret_cast<const StringValue*>(&rhs)->result);
      default:
         throw Exception{"invalid input for binary operator '!='"};
   }
}
//---------------------------------------------------------------------------
unique_ptr<Value> StringValue::computeCast(const Environment& /*env*/, VariableType resultType) const
{
   switch (resultType) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(gda::to_number<int32_t>(this->result));
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(gda::to_number<float>(this->result));
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(this->result == kTrue || this->result == "0");
      case VariableType::TString:
         return gda::make_unique<StringValue>(this->result);
   }
   throw;
}
//---------------------------------------------------------------------------
void UnaryOperator::print() const
{
   cout << " ( ";
   cout << getSign();
   child->print();
   cout << " ) ";
}
//---------------------------------------------------------------------------
void UnaryOperator::addChild(unique_ptr<Expression> child)
{
   this->child = ::move(child);
}
//---------------------------------------------------------------------------
unique_ptr<Value> UnaryMinusOperator::evaluate(Environment& environment) const
{
   return child->evaluate(environment)->computeInv(environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> NotOperator::evaluate(Environment& environment) const
{
   return child->evaluate(environment)->computeNot(environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> CastOperator::evaluate(Environment& environment) const
{
   return child->evaluate(environment)->computeCast(environment, getCastType());
}
//---------------------------------------------------------------------------
void BinaryOperator::addChildren(unique_ptr<Expression> lhsChild, unique_ptr<Expression> rhsChild)
{
   assert(lhs == nullptr && rhs == nullptr);
   lhs = ::move(lhsChild);
   rhs = ::move(rhsChild);
}
//---------------------------------------------------------------------------
void BinaryOperator::print() const
{
   cout << " ( ";
   lhs->print();
   cout << getSign();
   rhs->print();
   cout << " ) ";
}
//---------------------------------------------------------------------------
unique_ptr<Value> AssignmentOperator::evaluate(Environment& environment) const
{
   if (lhs->getSimpleType() != SimpleType::TVariable)
      throw Exception("need variable as left hand side of assignment operator");

   environment.update(reinterpret_cast<Variable*>(lhs.get())->getIdentifier(), rhs->evaluate(environment));
   return lhs->evaluate(environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> PlusOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeAdd(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> MinusOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeSub(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> MultiplicationOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeMul(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> DivisionOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeDiv(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> ModuloOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeMod(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> ExponentiationOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeExp(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> AndOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeAnd(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> OrOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeOr(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> GreaterOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeGt(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> LessOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeLt(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> GreaterEqualOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeGeq(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> LessEqualOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeLeq(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> EqualOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeEq(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
unique_ptr<Value> NotEqualOperator::evaluate(Environment& environment) const
{
   return lhs->evaluate(environment)->computeNeq(*rhs->evaluate(environment), environment);
}
//---------------------------------------------------------------------------
} // End of namespace gda
//---------------------------------------------------------------------------
}
