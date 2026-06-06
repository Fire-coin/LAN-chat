#include <iostream>
#include "socket.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <future>
#include <chrono>
#include <string>

namespace fs = std::filesystem;

void error(const char* msg) {
  perror(msg);
  exit(1);
}

void appError(std::string msg) {
  std::cerr << "LAN-chat: " << msg << std::endl;
}
bool fileExists(const std::string& filePath);

int handleFile(const std::string& path, std::fstream& file);
std::string getFilename(const std::string& path);

bool sendMessage(ConnectionSock& socket, std::string msg);
bool sendFile(ConnectionSock& socket, std::string filepath);
int recieve(ConnectionSock& socket, std::string& message);


void monitor(int portNum);
void establishConnection(ConnectionSock& conSock);
void establishConnection(std::string IPOrHost, int portNum);

bool acceptConnection = true;
bool doConnection = false;
bool doListening = true;
bool isConnected = false;

int main() {
  std::cout << "Start of the Test\n";
  
  //std::cout << "Select to be server(0) or client(1)\n";
  int choice;
  //std::cin >> choice;
  //if (choice != 0 && choice != 1)
  //  return 0;

  int portNum = 55555;
  int n; // Number of bytes written / read
  

  Msg recievedMsg;
  int useFile = 0;
  std::string filepath;
  std::string msg;
  // Both added to supress compiler warnings
  std::future<void> _monitorRequest;
  std::future<void> _sendingRequest;
  
  _monitorRequest = std::async(std::launch::async, [portNum]() { monitor(portNum); });
 

  while (doListening) {
    while (!isConnected) {
      std::cout << "Start connection(0 / 1): ";
      std::cin >> choice;
      
      if (!isConnected && choice == 1) 
        _sendingRequest = std::async(std::launch::async, [portNum]() { establishConnection("localhost", portNum); });
      else if (choice == 0) {
        std::cout << "Exit app (1 / 0)?: ";
        std::cin >> choice;
        if (choice == 1)
          exit(0);
    }
  } 
  
  


  // if (choice == 0) {
  //   // Launching socket which will always listen
  //    

  //   std::getline(std::cin, msg); // Clear the buffer
  //   while (msg != "exit" && conSock.exists()) {
  //     std::cout << "Do you want to send file?(no=0)(yes=1): ";
  //     std::cin >> useFile ;
  //     std::getline(std::cin, filepath); // Clearing the buffer
  //     
  //     if (useFile == 1) {
  //       std::cout << "Enter filapath: ";
  //       std::getline(std::cin, filepath);
  //       
  //       if (!sendFile(conSock, filepath)) // Nothing transmitted, so other end cannot send anything either
  //         continue;

  //     } else if (useFile == 0) {
  //         std::cout << "Enter message to send to other device: ";
  //         std::getline(std::cin, msg);
  //         if (!sendMessage(conSock, msg))
  //           continue;
  //     }
  //      
  //     n = recieve(conSock, msg);
  //     if (n == 1 || n == 3) // Some error
  //       continue; // Handle them later
  //     if (n == 2) 
  //       std::cout << "Recieved message: " << msg << std::endl;
  //     if (n == 4)
  //       std::cout << "File recieved: " << msg << std::endl;
  //   }
  //   
  //   if (conSock.exists())
  //     conSock.close();
  //   monSock.close();

  // } else {
  //   ConnectionSock conSock = ConnectionSock("localhost", "55555");

  //   if (!conSock.exists())
  //     error("Error connecting to Socket");
  //   
  //   std::getline(std::cin, msg);
  //   while (msg != "exit") {
  //     n = recieve(conSock, msg);
  //     if (n == 1 || n == 3) // Some error
  //       continue; // Handle them later
  //     if (n == 2) 
  //       std::cout << "Recieved message: " << msg << std::endl;
  //     if (n == 4)
  //       std::cout << "File recieved: " << msg << std::endl;
  //     
  //   
  //     std::cout << "Do you want to send file?(no=0)(yes=1): ";
  //     std::cin >> useFile ;
  //     std::getline(std::cin, filepath); // Clearing the buffer
  //     
  //     if (useFile == 1) {
  //       std::cout << "Enter filapath: ";
  //       std::getline(std::cin, filepath);
  //       
  //       if (!sendFile(conSock, filepath)) // Nothing transmitted, so other end cannot send anything either
  //         continue;

  //     } else if (useFile == 0) {
  //         std::cout << "Enter message to send to other device: ";
  //         std::getline(std::cin, msg);
  //         if (!sendMessage(conSock, msg))
  //           continue;
  //     }
  //   }

  //   conSock.close();

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
bool sendMessage(ConnectionSock& socket, std::string msg) {
  int n = 0;
  n = socket.send(msg);
  if (n < 0) {
    perror("Error sending message to socket");
    return false;
  }
  return true;
}

bool sendFile(ConnectionSock& socket, std::string filepath) {
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

/* Should always run on separate thread */
void monitor(int portNum) {
  MonitorSock monSock = MonitorSock();
  ConnectionSock conSock;
  std::future<void> _connectionRequest; // Added here to supress warning
  
  //TODO handle this better
  if (monSock.bind(portNum))
      error("Error while monitoring socket");

  monSock.listen();
  while (doListening) { 
    if (acceptConnection) {
      conSock.close();
      conSock = monSock.accept();
      // Establishing connection with socket
      _connectionRequest = std::async(std::launch::async, [&conSock]() { establishConnection(conSock); });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  monSock.close();
}

void establishConnection(ConnectionSock& conSock) {
  if (!conSock.exists())
    error("Error while creating connection socket");
  
  isConnected = true;
  doConnection = false;

  bool isRequestSending = false;
  bool isRequestRecieving = false;
  std::future<bool> sendResponce;
  std::future<int> recieveResponce;
  std::string sendMsg, recvMsg, filepath;
  int useFile = 0;

  std::getline(std::cin, sendMsg); // Clear the buffer
  // TODO add condition which will be possible to terminate from other thread
  while (sendMsg != "exit" && conSock.exists() && recvMsg != "exit") {
    if (!isRequestSending) {
      isRequestSending = true;
      
      std::cout << "Do you want to send file?(no=0)(yes=1): ";
      std::cin >> useFile ;
      std::getline(std::cin, filepath); // Clearing the buffer
      
      if (useFile == 1) {
        std::cout << "Enter filapath: ";
        std::getline(std::cin, filepath);
        // TODO fix this error, it treats last argument in function as a reference even though it is not
        sendResponce = std::async(std::launch::async, [&conSock, filepath]() { return sendFile(conSock, filepath); });
      } else if (useFile == 0) {
        std::cout << "Enter message to send to other device: ";
        std::getline(std::cin, sendMsg);
        // TODO some more bs here
        sendResponce = std::async(std::launch::async, [&conSock, sendMsg]() { return sendMessage(conSock, sendMsg); });
      }
    }
    // TODO add some way to separate streams when recieving and sending data
    auto sendStatus = sendResponce.wait_for(std::chrono::milliseconds(0));
    if (sendStatus == std::future_status::ready) {
      bool sendSuccess = sendResponce.get();
      if (!sendSuccess) {
        // TODO handle error
      }
      isRequestSending = false;
    }
    
    if (!isRequestRecieving) {
      isRequestRecieving = true;
       
      recieveResponce = std::async(std::launch::async, [&conSock, &recvMsg]() { return recieve(conSock, recvMsg); });
    }
    
    auto recieveStatus = recieveResponce.wait_for(std::chrono::milliseconds(0));

    if (recieveStatus == std::future_status::ready) {
      int n = recieveResponce.get(); 

      if (n == 1 || n == 3) // Some error
        continue; // Handle them later
      if (n == 2) 
        std::cout << "Recieved message: " << recvMsg << std::endl;
      if (n == 4)
        std::cout << "File recieved: " << recvMsg << std::endl;
      isRequestRecieving = false;
    }
  }
  isConnected = false;
  doConnection = true;
  conSock.close();
}


void establishConnection(std::string IPOrHost, int portNum) {
  if (doConnection) {
    ConnectionSock conSock = ConnectionSock(IPOrHost, std::to_string(portNum));
    establishConnection(conSock);
  }
}


