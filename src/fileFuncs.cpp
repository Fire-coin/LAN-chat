#include "fileFuncs.hpp"
#include "appErrors.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

/* Saves file contents from msg, into provided directory dirName */
int savePeerFile(std::string dirName, Msg& msg, bool overwrite) {
  /* Checks if LAN-chat directory for files exists. If not it creates it */
  fs::path dirPath = fs::current_path() / dirName;
  if (!fs::is_directory(dirPath))
   fs::create_directory(dirPath);

  /* Creating a path for the recieved file, and creting it in the directory*/
  fs::path filepath = dirPath / msg.filename;

  std::fstream file;
  if (overwrite)
    file.open(filepath, std::fstream::out | std::fstream::binary);
  else
    file.open(filepath, std::fstream::app | std::fstream::binary);
    
  if (!file.is_open())
    return LCE_FILE_OP; // Error with file

  /* Writing contents of msg into file */
  file.write(msg.data.data(), msg.data.size());
  file.close();
  return 0;
}

bool fileExists(const std::string& filePath) {
  return fs::exists(filePath);
}
/* Handles opening file and error management */
int handleFile(const std::string& path, std::fstream& file) {
  if (!fileExists(path)) { 
    pushError("Specified file does not exist", LCE_FILE_OP);
    return LCE_ALREADY_REPORTED;
  }
  // Opening file for reading in binary mode
  file.open(path, std::fstream::in | std::fstream::binary);

  if (!file.is_open()) {
    pushError("Could not open the file", LCE_FILE_OP);
    return LCE_ALREADY_REPORTED;
  }
  return 0;
}
 
std::string getFilename(const std::string& path) {
  if (!fileExists(path)) {
    pushError("Specified file does not exist", LCE_FILE_OP);
    return "";
  }

  fs::path p(path);
  return p.filename();
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
