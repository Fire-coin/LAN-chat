#include "formatString.hpp"
/* Adds a padding to given input string, so in total
 * its size if outSize. position tells, where to place the
 * string itself, < means to the left, > means to the right
 * and ^ means centered. */
std::string format(std::string& input, int outSize, char position) {
  std::string out;
  int length = input.size();
  int needed = outSize - length;
  int strMiddle;
  
  // Return only part of the string so it fits into outSize
  if (needed <= 0)
    return std::string(input.begin(), input.begin() + outSize);
  
  switch (position) {
    case '>':
      out += std::string(needed, ' ');
      out += input;
      return out;
    case '^':
      strMiddle = length / 2;
      out += std::string(needed / 2, ' ');
      out += input;
      out += std::string(needed - (needed / 2), ' ');
      return out;
    case '<': // Align to the left
    default: // Align to the left by default
      out += input;
      out += std::string(needed, ' ');
      return out;
  }
}

std::string format(const char* input, int outSize, char position) {
  std::string in(input);
  return format(in, outSize, position);
}
