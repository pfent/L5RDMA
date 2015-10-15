//
// This file is part of the gda cpp utility library.
// Copyright (c) 2011 Alexander van Renen. All rights reserved.
//
// Purpose - String utilities.
// -------------------------------------------------------------------------------------------------
#include <string>
#include <sstream>
#include <stdint.h>
#include <vector>
// -------------------------------------------------------------------------------------------------
namespace gda {
/// Counts number of appearances of c in std::string str
uint32_t countAppearances(const std::string& str, char c);

/// Calculates the size of the longest line
uint32_t sizeOfLongestLine(const std::string& str, char c = '\n');

/// Clears the std::string, afterwards it will just contain: ([a-z][A-Z][0-9])*
std::string filterSpecialChars(const std::string& str);

/// Splits the std::string at the splitter parameter and stores the pieces in result
void split(std::vector<std::string>& result, const std::string& str, char splitter);

/// The specified character is limited to only one appearance in a row
std::string removeAllDouble(const std::string& str, char remove);

/// Returns the remaining content of the stream in form of a string
std::string getRemainingContent(std::istream& in);

/// Checks if string ends with "ending"
bool endsWith(const std::string& str, const std::string& ending);

/// Produces a random string
// TODO: pass random generator
std::string randomString(uint32_t len);
std::string randomAlphaString(uint32_t len);
std::string randomNumericString(uint32_t len);
std::string randomAlphaNumericString(uint32_t len);
std::string randomString(uint32_t len, const std::string& charsToUse);

// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
