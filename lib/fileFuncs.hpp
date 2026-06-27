#ifndef FILE_FUNCS_HPP
#define FILE_FUNCS_HPP

#include <string>
#include <fstream>
#include <vector>
#include "socket.hpp"

int savePeerFile(std::string dirName, Msg& msg);

void appError(std::string msg);

bool fileExists(const std::string& filePath);

int handleFile(const std::string& path, std::fstream& file);
std::string getFilename(const std::string& path);

std::string getCurDir();
void getDirContents(std::vector<std::string>& files, std::string p);
std::string changeDir(std::string path, std::string dir);
std::string getParentDir(std::string curPath);

#endif
