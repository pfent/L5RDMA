//
// This file is part of the gda cpp utility library.
// Copyright (c) 2011 Alexander van Renen. All rights reserved.
//
// Purpose - Function for number to string and vise versa function.
// -------------------------------------------------------------------------------------------------
#include <string>
#include <sstream>
#include <stdint.h>
#include <vector>
#include <iostream>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------

/// string --> number
template<class Number>
Number to_number_no_throw(const std::string& str)
{
   Number num;
   std::istringstream stream(str);
   stream >> num;
   return num;
}

/// The exception which gets thrown when parsing a number fails
// TODO make this a real exception
struct NoNumber { };

template<class Number>
std::string to_string(const Number& num)
{
   std::ostringstream stream;
   stream << num;
   if (!stream.good())
      throw NoNumber();
   return stream.str();
}
/// string --> number
template<class Number>
Number to_number(const std::string& str)
{
   Number num;
   std::istringstream stream(str);
   stream >> num;
   if (!stream.good() && !stream.eof())
      throw NoNumber();
   return num;
}
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
