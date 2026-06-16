#include "fileFuncs.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void appError(std::string msg) {
  std::cerr << "LAN-chat: " << msg << std::endl;
}

bool fileExists(const std::string& filePath) {
  return fs::exists(filePath);
}
/* Handles opening file and error management */
int handleFile(const std::string& path, std::fstream& file) {
  if (!fileExists(path)) { 
    appError("Specified file does not exist");
    return 1;
  }
  // Opening file for reading in binary mode
  file.open(path, std::fstream::in | std::fstream::binary);

  if (!file.is_open()) {
    appError("Could not open the file");
    return 1;
  }
  return 0;
}
 
std::string getFilename(const std::string& path) {
  if (!fileExists(path)) {
    appError("Specified file does not exist");
    return "";
  }
  size_t index = path.find_last_of('/');
  if (index == path.npos)
    return path;
  else
    return path.substr(index + 1);
}

std::string getCurDir() {
  return fs::current_path().string();
}

std::string changeDir(std::string path, std::string dir) {
  fs::path p(path);
  p /= dir;
  return p.string();
}

std::string getParentDir(std::string curPath) {
  fs::path p(curPath);
  return p.parent_path().string();
}

void getDirContents(std::vector<std::string>& files, std::string p) {
  files.clear();
  files.push_back("..");
  
  for (const auto& dirEntry : fs::directory_iterator{p}) {
    fs::path fileOrDir = dirEntry.path();
    if (is_directory(fileOrDir)) {
      // Directory will have a slash at the end of the filename
      files.push_back(fileOrDir.filename().string().append("/"));
    } else {
      files.push_back(fileOrDir.filename().string());
    }
  }
}
