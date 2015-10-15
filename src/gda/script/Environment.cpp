#include "gda/script/Environment.hpp"
#include "gda/script/Expression.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
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
Environment::Environment(Environment* parentEnvironment)
: parent(parentEnvironment)
{
}
//---------------------------------------------------------------------------
Environment::~Environment()
{
}
//---------------------------------------------------------------------------
void Environment::add(const string& identifier, unique_ptr<Value> value)
{
   assert(none_of(data.begin(), data.end(), [&identifier](const pair<string, unique_ptr<Value>>& iter) { return iter.first == identifier; }));
   data.push_back(make_pair(identifier, ::move(value)));
}
//---------------------------------------------------------------------------
void Environment::update(const string& identifier, unique_ptr<Value> value)
{
   assert(isInAnyScope(identifier));
   for (auto& iter : data)
      if (iter.first == identifier) {
         iter.second = ::move(value);
         return;
      }
   parent->update(identifier, ::move(value));
}
//---------------------------------------------------------------------------
const Value& Environment::read(const string& identifier) const
{
   assert(isInAnyScope(identifier));
   for (auto& iter : data)
      if (iter.first == identifier)
         return *iter.second;
   return parent->read(identifier);
}
//---------------------------------------------------------------------------
bool Environment::isInAnyScope(const string& identifier) const
{
   for (auto& iter : data)
      if (iter.first == identifier)
         return true;
   if (parent != nullptr)
      return parent->isInAnyScope(identifier);
   else
      return false;
}
//---------------------------------------------------------------------------
bool Environment::isInLocalScope(const std::string& identifier) const
{
   for (auto& iter : data)
      if (iter.first == identifier)
         return true;
   return false;
}
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
}
