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
#include <mutex>
#include <algorithm>
#include <stdlib.h>
#include <curses.h> // TODO include in readme that this library has to be installed
#include <utility> // for std::pair
#include "UI.hpp"

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
// TODO set all global vaiables to false before exiting
// Global variables used to control async processes
bool acceptConnection = true;
bool doConnection = true;
bool doListening = true;
bool isConnected = false;
bool doPeerDiscovery = true;

// Stores all needed information about peers
struct Peer {
  std::string IP;
  std::string nickname;
  std::chrono::steady_clock::time_point lastSeen;
};
// Processes write new peers here and remove ones which did not respond for last 3 seconds
std::vector<Peer> currentPeers;

std::mutex discoverMutex;

void discoverPeers(int portNum);
void showPeers();

std::vector<std::string> getMachineIPs();


// Chat history using Msg structure from socket.hpp

// Stores the messages which were sent and by whom it was sent
// Currently 0 will be messages sent and 1 messages recieved

std::mutex messageMutex;
void addMsg(Msg msg, int creator);


int main() {
  //std::cout << "Start of the Test\n";
  
  int choice;
  std::string buf; // Used to clear std::cin buffer
  int portNum = 55555;
  int discoveryPortNum = 5000;
  // Both added to supress compiler warnings
  std::future<void> _monitorRequest;
  std::future<void> _sendingRequest;
  // std::vector<std::string> IPs = getMachineIPs();
  // for (auto ip : IPs) 
  //   std::cout << "IP found: " << ip << std::endl;

  // Currently doing test for UDP discovery
  std::future<void> _discoverRequest = std::async(std::launch::async, [discoveryPortNum]() { discoverPeers(discoveryPortNum); });
  // int i = 0;
  // while (true) {
  //   //system("clear");
  //   std::cout << "Iteration: " << i++ << std::endl;
  //   showPeers();
  //   std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  // }
  
  //Testing curses
  //testInputAndOutput();
  displayChatScreen();
  


  std::cout << "Do you want to start monitoring(1 / 0): ";
  std::cin >> choice;
  
  if (choice == 1)
  _monitorRequest = std::async(std::launch::async, [portNum]() { monitor(portNum); });
 
  //TODO run this on a separate thread so it can be cancelled unlike async
  while (doListening && !isConnected) {
    std::cout << "Start connection(0 / 1): ";
    std::cin >> choice;
    // std::getline(std::cin, buf);
    
    if (!isConnected && choice == 1) {
      std::cout << "Available devices: \n";
      showPeers();
      std::string ip;
      std::cout << "Enter IP to connect to: ";
      std::getline(std::cin, ip);
      std::getline(std::cin, ip);
      _sendingRequest = std::async(std::launch::async, [portNum, ip]() { establishConnection(ip, portNum); });
    }
    else if (choice == 0 && !isConnected) {
      std::cout << "Exit app (1 / 0)?: ";
      std::cin >> choice;
      // std::getline(std::cin, buf);
    
      if (choice == 1)
        exit(0);
    }
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
  //std::cout << "Writing file to: " << dirName << std::endl;
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
      appError("Error while monitoring socket");

  monSock.listen();
  while (doListening) { 
    if (acceptConnection) {
      acceptConnection = false;
      conSock.close();
      conSock = monSock.accept();
      // Establishing connection with socket
      _connectionRequest = std::async(std::launch::async, [&conSock]() { establishConnection(conSock); });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  monSock.close();
}
/* Makes connection with tcp socket.
 * Displays recieved messages / files and prompts to send messages / files*/
void establishConnection(ConnectionSock& conSock) {
  std::cout << "Establish connection 2\n";
  if (!conSock.exists())
    error("Error while creating connection socket");
  initscr();
  isConnected = true;
  doConnection = false;

  bool isRequestSending = false;
  bool isRequestRecieving = false;
  bool isTakingInput = false;
  std::future<bool> sendResponce;
  std::future<int> recieveResponce;
  std::future<std::string> inputResponce;
  int useFile = 0;
  Msg sendMsg, recvMsg;
  std::string recievedData, sendData;

  //std::getline(std::cin, sendMsg); // Clear the buffer
  // TODO add condition which will be possible to terminate from other thread
  while (sendData != "exit" && conSock.exists() && recievedData != "exit") {
    // Start taking input
    if (!isTakingInput) {
      isTakingInput = true;
      inputResponce = std::async(std::launch::async, []() { return getUserInput(); });
    }

    auto inputStatus = inputResponce.wait_for(std::chrono::milliseconds(0));
    if (inputStatus == std::future_status::ready) {
      printw("Success to get string");
      // Start with a new request to send when there is none currently
      if (!isRequestSending) {
        isRequestSending = true;
        isTakingInput = false;
        std::string sendData = inputResponce.get();
        std::string filename;
        sendMsg.filename = "";
        sendMsg.data = "";
        // Process input, check if user sent file or text message
        if (sendData.find_first_of("$file=") == sendData.npos) { // Text message
          sendMsg.data = sendData;
          sendResponce = std::async(std::launch::async, [&conSock, sendData]() { return sendMessage(conSock, sendData); });
        } else { // File
          int index = sendData.find_first_of("$file=");
          filename = std::string(sendData.begin() + index + 7, sendData.end()); // We add + 7 bytes to start from the filename
          sendMsg.filename = filename;
          sendResponce = std::async(std::launch::async, [&conSock, filename]() { return sendFile(conSock, filename); });
        }
        addMsg(sendMsg, 0); // Creator = 0 because data is being sent

        


        //std::cout << "Do you want to send file?(no=0)(yes=1): ";
        //std::cin >> useFile ;
        //std::getline(std::cin, filepath); // Clearing the buffer
        //
        //if (useFile == 1) {
        //  std::cout << "Enter filapath: ";
        //  std::getline(std::cin, filepath);
        //  sendMsg = "";
        //  sendResponce = std::async(std::launch::async, [&conSock, filepath]() { return sendFile(conSock, filepath); });
        //} else if (useFile == 0) {
        //  std::cout << "Enter message to send to other device: ";
        //  std::getline(std::cin, sendMsg);
        //  filepath = "";
        //  sendResponce = std::async(std::launch::async, [&conSock, sendMsg]() { return sendMessage(conSock, sendMsg); });
        //}
      }
    }
    // TODO add some way to separate streams when recieving and sending data
    auto sendStatus = sendResponce.wait_for(std::chrono::milliseconds(0));
    if (sendStatus == std::future_status::ready) {
      bool sendSuccess = sendResponce.get();
      if (!sendSuccess) {
        // TODO handle error
      }
      addMsg(sendMsg, 0); // The message was sent by this machine thats why second param is 0
      //displayChatLog(chatHistory);
      isRequestSending = false;
    }
    // Start new recieving request only if none is currently pending 
    if (!isRequestRecieving) {
      isRequestRecieving = true;
      recvMsg.filename = "";
      recvMsg.data = "";
      recieveResponce = std::async(std::launch::async, [&conSock, &recievedData]() { return recieve(conSock, recievedData); });
    }
    
    auto recieveStatus = recieveResponce.wait_for(std::chrono::milliseconds(0));

    if (recieveStatus == std::future_status::ready) {
      int n = recieveResponce.get(); 

      if (n == 1 || n == 3) // Some error
        continue; // Handle them later
      if (n == 2 || n == 4) {
        Msg msg;
        // TODO make messages be possible to send both text and file
        if (n == 2)  {
          msg.data = recievedData;
          addMsg(msg, 1); // Data recieved so creator = 1
          //std::cout << "Recieved message: " << recvMsg << std::endl;
        }
        if (n == 4) {
          msg.filename = recievedData;
          addMsg(msg, 1); // Data recievec so creator = 1
          //std::cout << "File recieved: " << recvMsg << std::endl;
        }
        //displayChatLog(chatHistory);

        isRequestRecieving = false;
      }
    }
  }
  isConnected = false;
  doConnection = true;
  acceptConnection = true;
  conSock.close();
  endwin();
}

// TODO convert IPOrHost with inet_pton into binary value
void establishConnection(std::string IPOrHost, int portNum) {
  if (doConnection) {
    std::cout << "Establish connection 1\n";
    ConnectionSock conSock = ConnectionSock(IPOrHost, std::to_string(portNum));
    establishConnection(conSock);
  }
}
// TODO abolish async and use threads because these do not return anything
void discoverPeers(int portNum) {
  UDPDiscoverySock uSock = UDPDiscoverySock();

  if (uSock.bind(portNum) < 0) {
    appError("Discovery socket binding failure");
    perror("");
  }

  std::vector<std::string> IPs = getMachineIPs();
  
  bool isSendingPacket = false;
  bool isRecievingPacket = false;
  std::future<int> packetSendError, packetRecvError;

  int sendDelay = 1000; // In milliseconds
  int recvDelay = 500; // In milliseconds

  std::string nickname, IP; 

  while (doPeerDiscovery) {
    if (!isSendingPacket) {
      isSendingPacket = true;
      packetSendError = std::async(std::launch::async, [sendDelay, &uSock]() { return uSock.sendPresence(sendDelay); });
    }
    
    auto sendStatus = packetSendError.wait_for(std::chrono::milliseconds(0));
    if (sendStatus == std::future_status::ready) {
      isSendingPacket = false;
      int SendError = packetSendError.get();
      if (SendError < 0) {
        appError("Error while sending over UDP socket");
        perror("");
      }
    }
    // TODO make this part more unnested, cause its pain to look at 
    if (!isRecievingPacket) {
      isRecievingPacket = true;
      packetRecvError = std::async(std::launch::async, [recvDelay, &uSock, &IP, &nickname]() { return uSock.recievePacket(IP, nickname, recvDelay); });
    }

    auto recvStatus = packetRecvError.wait_for(std::chrono::milliseconds(0));

    if (recvStatus == std::future_status::ready) {
      isRecievingPacket = false;
      int recvError = packetRecvError.get();
      discoverMutex.lock();
      if (recvError < 0) {
        if (recvError == -1) {
          appError("Error while recieving UDP packet");
          perror("");
        }
      } else {
        auto it = find_if(currentPeers.begin(), currentPeers.end(), [IP](Peer& peer) { return peer.IP == IP; });
        if (it == currentPeers.end()) {
          // Ignore if IP is one of the machine ones
          if (find_if(IPs.begin(), IPs.end(), [IP](std::string& ip) {return ip == IP; }) == IPs.end()) {
            Peer p;
            p.IP = IP;
            p.nickname = nickname;
            p.lastSeen = std::chrono::steady_clock::now();
            
            currentPeers.push_back(p);
          }
        } else {
          it->lastSeen = std::chrono::steady_clock::now();
        }
      }
      // Scan if some of the peers are offline
      for (auto it = currentPeers.begin(); it != currentPeers.end();) {
        // If last packet recieved from this IP was more than 3 seconds ago
        if (std::chrono::steady_clock::now() - it->lastSeen > std::chrono::milliseconds(3000))
          it = currentPeers.erase(it);
        else
          ++it;
      }

      discoverMutex.unlock();
    }
  }
}


void showPeers() {
  discoverMutex.lock();
  std::cout << "========== Available peers ==========\n";
  for (auto it = currentPeers.begin(); it != currentPeers.end(); ++it)
    std::cout << "IP: " << it->IP << "; nickname: " << it->nickname << std::endl;
  std::cout << "=====================================\n";
  discoverMutex.unlock();
}

std::vector<std::string> getMachineIPs() {
  std::vector<std::string> IPs;
  struct ifaddrs *ifaddr;
  int family, s;
  char host[NI_MAXHOST];

  if (getifaddrs(&ifaddr) == -1)
    perror("getifaddrs");
  
  for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;

    family = ifa->ifa_addr->sa_family;

    if (family == AF_INET) {
      s = getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

      if (s != 0) {
        std::string msg = "getnameinfo failed: ";
        msg.append(gai_strerror(s));
        appError(msg);
      }
      if (std::string(host) != "127.0.0.1")
        IPs.push_back(std::string(host));
    }
  }
  freeifaddrs(ifaddr);
  return IPs;
}


void addMsg(Msg msg, int creator) {
  messageMutex.lock();

  std::pair<int, Msg> temp = std::pair<int, Msg>(creator, msg);
  chatHistory.push_back(temp);

  messageMutex.unlock();
}
