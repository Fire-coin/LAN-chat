#include <iostream>
#include "socket.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void error(const char* msg) {
  perror(msg);
  exit(1);
}

void appError(const char* msg) {
  std::cerr << "LAN-chat: " << msg << std::endl;
}
bool fileExists(const std::string& filePath);

int handleFile(const std::string& path, std::fstream& file);
std::string getFilename(const std::string& path);


int main() {
  std::cout << "Start of the Test\n";
  
  std::cout << "Select to be server(0) or client(1)\n";
  int choice;
  std::cin >> choice;
  if (choice != 0 && choice != 1)
    return 0;

  int portNum = 55555;
  int n; // Number of bytes written / read
  

  if (choice == 0) {
    MonitorSock monSock = MonitorSock();
    
    if (monSock.bind(portNum))
      error("Error while monitoring socket");

    monSock.listen();

    ConnectionSock conSock = monSock.accept();
    
    if (!conSock.exists())
      error("Error while creating connection socket");

    std::string msg;
    std::getline(std::cin, msg); // Clear the buffer
    int sendFile = 0;
    std::string filepath, filename;
    while (msg != "exit") {
      std::cout << "Do you want to send file?(no=0)(yes=1): ";
      std::cin >> sendFile;
      std::getline(std::cin, filepath); // Clearing the buffer
      if (sendFile != 0 && sendFile != 1) {}
      else {
        if (sendFile == 1) {
          int error = 0;
          std::cout << "Enter filapath: ";
          std::getline(std::cin, filepath);
          std::fstream file;
          error = handleFile(filepath, file); // Opening the file and handling errors
          if (error < 0)
            continue;
          filename = getFilename(filepath);
          std::cout << "Filename of selected file: " << filename << std::endl;
          n = conSock.sendFile(file, filename);
          if (n < 0)
            perror("Error sending file to socket");
        } else {
            std::cout << "Enter message to send to other device: ";
            std::getline(std::cin, msg);
            n = conSock.send(msg);
            if (n < 0)
              perror("Error sending message to socket");
        }
      }
        
      n = conSock.recieve(msg);
      if (n == 1)
        continue;
      std::cout << "Recieved message: " << msg << std::endl;
    }

    conSock.close();
    monSock.close();

  } else {
    ConnectionSock conSock = ConnectionSock("localhost", "55555");

    if (!conSock.exists())
      error("Error connecting to Socket");
    
    std::string msg;
    std::getline(std::cin, msg);

    while (msg != "exit") {
      n = conSock.recieve(msg);
      if (n < 0)
        std::cout << "No recieved message\n";
      else
        std::cout << "Recieved message: " << msg << std::endl;

      std::cout << "Enter message to transmit: ";
      std::getline(std::cin, msg);

      n = conSock.send(msg);
      if (n < 0)
        perror("Error sending to socket");
    }

    conSock.close();

  }
  return 0;
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
