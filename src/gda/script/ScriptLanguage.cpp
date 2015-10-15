#include "gda/script/ScriptLanguage.hpp"
#include "gda/Utility.hpp"
#include "gda/script/Expression.hpp"
#include <sstream>
#include <ctype.h>
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
VariableType nameToType(const string& name) throw(Exception)
{
   if (name == kVariableInteger)
      return VariableType::TInteger;
   if (name == kVariableFloat)
      return VariableType::TFloat;
   if (name == kVariableBool)
      return VariableType::TBool;
   if (name == kVariableString)
      return VariableType::TString;
   throw Exception{"invalid type name: " + name};
}
//---------------------------------------------------------------------------
const string typeToName(VariableType type) throw()
{
   switch (type) {
      case VariableType::TInteger:
         return kVariableInteger;
      case VariableType::TFloat:
         return kVariableFloat;
      case VariableType::TBool:
         return kVariableBool;
      case VariableType::TString:
         return kVariableString;
   }
   throw Exception{"unreachable"};
}
//---------------------------------------------------------------------------
unique_ptr<Value> createDefaultValue(VariableType type) throw()
{
   switch (type) {
      case VariableType::TInteger:
         return gda::make_unique<IntegerValue>(0);
      case VariableType::TFloat:
         return gda::make_unique<FloatValue>(.0f);
      case VariableType::TBool:
         return gda::make_unique<BoolValue>(true);
      case VariableType::TString:
         return gda::make_unique<StringValue>("");
   }
   throw Exception{"unreachable"};
}
//---------------------------------------------------------------------------
const string parseIdentifier(istream& is) throw(Exception)
{
   if (!isalpha(is.peek()))
      throw Exception{"identifier can not start with: '" + string(is.peek(), 1) + "'"};

   ostringstream result;
   char buffer;
   while (isalnum(is.peek()) && is.good()) {
      is >> buffer;
      result << buffer;
   }

   return result.str();
}
//---------------------------------------------------------------------------
void skipWhiteSpace(istream& is) throw()
{
   while (isspace(is.peek()) && is.good())
      is.get();
}
//---------------------------------------------------------------------------
const string readOnlyAlpha(istream& is) throw()
{
   string result;
   while (isalpha(is.peek()) && is.good())
      result.push_back(is.get());
   return result;
}
//---------------------------------------------------------------------------
const string readParenthesisExpression(istream& is) throw(Exception)
{
   if (is.peek() != '(')
      throw Exception{"parenthesis expression has to start with parentesis"};
   int32_t parenthesisCount = 0;
   string result;

   do {
      // read
      char a;
      is >> a;
      if (a == '(') parenthesisCount++;
      if (a == ')') parenthesisCount--;
      if (parenthesisCount < 0) throw Exception{"expected '(' before ')'"};

      // check if finished
      result.push_back(a);
      if (parenthesisCount == 0)
         return result;
   } while (is.good());

   throw Exception{"parenthesis expression has to start with parentesis"};
}
//---------------------------------------------------------------------------
bool isImplicitCastPossible(VariableType from, VariableType to) throw()
{
   bool implicitCast[4][4] = {
   /* to\from      int     float   bool   string */
   /* int    */ {true, true, false, false},
   /* float  */ {true, true, false, false},
   /* bool   */ {false, false, false, false},
   /* string */ {false, false, false, false}
   };

   return implicitCast[static_cast<uint32_t>(to)][static_cast<uint32_t>(from)];
}
//---------------------------------------------------------------------------
} // end of namespace gda
//---------------------------------------------------------------------------
}
