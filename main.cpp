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

bool sendMessage(ConnectionSock& socket, std::string& msg);
bool sendFile(ConnectionSock& socket, std::string& filepath);
int recieve(ConnectionSock& socket, std::string& message);

int main() {
  std::cout << "Start of the Test\n";
  
  std::cout << "Select to be server(0) or client(1)\n";
  int choice;
  std::cin >> choice;
  if (choice != 0 && choice != 1)
    return 0;

  int portNum = 55555;
  int n; // Number of bytes written / read
  

  Msg recievedMsg;
  int useFile = 0;
  std::string filepath;
  std::string msg;
  if (choice == 0) {
    MonitorSock monSock = MonitorSock();
    
    if (monSock.bind(portNum))
      error("Error while monitoring socket");

    monSock.listen();

    ConnectionSock conSock = monSock.accept();
    
    if (!conSock.exists())
      error("Error while creating connection socket");

    std::getline(std::cin, msg); // Clear the buffer
    while (msg != "exit" && conSock.exists()) {
      std::cout << "Do you want to send file?(no=0)(yes=1): ";
      std::cin >> useFile ;
      std::getline(std::cin, filepath); // Clearing the buffer
      
      if (useFile == 1) {
        std::cout << "Enter filapath: ";
        std::getline(std::cin, filepath);
        
        if (!sendFile(conSock, filepath)) // Nothing transmitted, so other end cannot send anything either
          continue;

      } else if (useFile == 0) {
          std::cout << "Enter message to send to other device: ";
          std::getline(std::cin, msg);
          if (!sendMessage(conSock, msg))
            continue;
      }
       
      n = recieve(conSock, msg);
      if (n == 1 || n == 3) // Some error
        continue; // Handle them later
      if (n == 2) 
        std::cout << "Recieved message: " << msg << std::endl;
      if (n == 4)
        std::cout << "File recieved: " << msg << std::endl;
    }
    
    if (conSock.exists())
      conSock.close();
    monSock.close();

  } else {
    ConnectionSock conSock = ConnectionSock("localhost", "55555");

    if (!conSock.exists())
      error("Error connecting to Socket");
    
    std::getline(std::cin, msg);
    while (msg != "exit") {
      n = recieve(conSock, msg);
      if (n == 1 || n == 3) // Some error
        continue; // Handle them later
      if (n == 2) 
        std::cout << "Recieved message: " << msg << std::endl;
      if (n == 4)
        std::cout << "File recieved: " << msg << std::endl;
      
    
      std::cout << "Do you want to send file?(no=0)(yes=1): ";
      std::cin >> useFile ;
      std::getline(std::cin, filepath); // Clearing the buffer
      
      if (useFile == 1) {
        std::cout << "Enter filapath: ";
        std::getline(std::cin, filepath);
        
        if (!sendFile(conSock, filepath)) // Nothing transmitted, so other end cannot send anything either
          continue;

      } else if (useFile == 0) {
          std::cout << "Enter message to send to other device: ";
          std::getline(std::cin, msg);
          if (!sendMessage(conSock, msg))
            continue;
      }
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

/* Send message through socket. If error occures it closes the socket. */
bool sendMessage(ConnectionSock& socket, std::string& msg) {
  int n = 0;
  n = socket.send(msg);
  if (n < 0) {
    perror("Error sending message to socket");
    return false;
  }
  return true;
}

bool sendFile(ConnectionSock& socket, std::string& filepath) {
  int error = 0;
  int n = 0;
  std::string filename;
  std::fstream file;
  error = handleFile(filepath, file); // Opening the file and handling errors
  if (error > 0)
    return false;
  filename = getFilename(filepath);
  std::cout << "Filename of selected file: " << filename << std::endl;
  n = socket.sendFile(file, filename);
  if (n < 0) {
    perror("Error sending file to socket");
    return false;
  }
  return true;
}

int recieve(ConnectionSock& socket, std::string& message) {
  Msg msg;
  int n = socket.recieve(msg);
  if (n == 1)
    return 1; // Error while transmitting

  if (msg.filename == "") { // Plain message
   message = msg.data;
   return 2; // Text message was transmitted
  }
 
  // Create a file in LAN-chat directory
  std::string dirName = ".LAN-chat_files";
  fs::path dirPath = fs::current_path() / dirName;
  if (!fs::is_directory(dirPath))
   fs::create_directory(dirPath);
  
  dirName.append("/");
  dirName.append(msg.filename);
  std::cout << "Writing file to: " << dirName << std::endl;
  std::fstream file(dirName, std::fstream::out | std::fstream::binary);
  if (!file.is_open())
    return 3; // Error with file
  
  file.write(msg.data.data(), msg.data.size());
  file.close();
  message = msg.filename;
  return 4; // File was written succesfully
}
