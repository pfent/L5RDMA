//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - Like the bash environment, but for this script language.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
#include <vector>
// -------------------------------------------------------------------------------------------------
namespace gda {
//---------------------------------------------------------------------------
namespace harriet {
// -------------------------------------------------------------------------------------------------
class Value;
// -------------------------------------------------------------------------------------------------
class Environment {
public:
   Environment(Environment* parentEnvironment);
   ~Environment();

   void add(const std::string& identifier, std::unique_ptr<Value> value);
   void update(const std::string& identifier, std::unique_ptr<Value> value);
   const Value& read(const std::string& identifier) const;
   bool isInAnyScope(const std::string& identifier) const; // checks parents
   bool isInLocalScope(const std::string& identifier) const; // does not check parents

private:
   Environment* parent;
   std::vector<std::pair<std::string, std::unique_ptr<Value>>> data;
};
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
}
