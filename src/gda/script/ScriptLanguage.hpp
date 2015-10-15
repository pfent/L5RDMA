#pragma once
//---------------------------------------------------------------------------
#include <array>
#include <string>
#include <stdint.h>
#include <memory>
//---------------------------------------------------------------------------
//
//
//---------------------------------------------------------------------------
namespace gda {
//---------------------------------------------------------------------------
namespace harriet {
//---------------------------------------------------------------------------
class Value;
//---------------------------------------------------------------------------

/// variable types
enum struct VariableType : uint8_t { TInteger, TFloat, TBool, TString };

/// boolean values
const std::string kTrue = "true";
const std::string kFalse = "false";

const std::string kCastName = "cast";

const std::string kVariableInteger = "int";
const std::string kVariableFloat = "float";
const std::string kVariableBool = "bool";
const std::string kVariableString = "string";

/// exceptions
struct Exception : public std::exception {
   Exception(const std::string& message)
   : message(message) { }
   ~Exception() throw() { }
   const std::string message;
   virtual const char* what() const throw() { return message.c_str(); }
};

/// helper functions
VariableType nameToType(const std::string& name) throw(Exception);
const std::string typeToName(VariableType type) throw();

std::unique_ptr<Value> createDefaultValue(VariableType type) throw();

const std::string parseIdentifier(std::istream& is) throw(Exception);
void skipWhiteSpace(std::istream& is) throw();
const std::string readOnlyAlpha(std::istream& is) throw();
const std::string readParenthesisExpression(std::istream& is) throw(Exception);

bool isImplicitCastPossible(VariableType from, VariableType to) throw();
}
//---------------------------------------------------------------------------
}
