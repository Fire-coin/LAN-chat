#ifndef FILE_FUNCS_HPP
#define FILE_FUNCS_HPP

#include <string>
#include <fstream>

void appError(std::string msg);

bool fileExists(const std::string& filePath);

int handleFile(const std::string& path, std::fstream& file);
std::string getFilename(const std::string& path);

#endif
