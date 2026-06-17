#ifndef FORMAT_STRING_HPP
#define FORMAT_STRING_HPP

#include <string>

std::string format(std::string& input, int outSize, char position);
std::string format(const char* input, int outSize, char position);

#endif
