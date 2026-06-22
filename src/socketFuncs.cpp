#include "socketFuncs.hpp"
#include "fileFuncs.hpp" // file handling
#include "UI.hpp" // To set global variables
#include <future> // std::future
#include <thread> // std::this_thread::sleep_for
#include <filesystem>
#include <algorithm> // std::find_if
#include <sys/epoll.h> // epoll
#include <fcntl.h>
#include <unistd.h>


namespace fs = std::filesystem;

bool acceptConnection = true;
bool doConnection = true;
bool doListening = true;
bool isConnected = false;
bool doPeerDiscovery = true;
// TODO make a function which would display perror messages in UI

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
  // TODO make a function for this in fileFuncs.cpp
  // Create a file in LAN-chat directory
  std::string dirName = ".LAN-chat_files";
  fs::path dirPath = fs::current_path() / dirName;
  if (!fs::is_directory(dirPath))
   fs::create_directory(dirPath);
  
  dirName.append("/");
  dirName.append(msg.filename);
  std::fstream file(dirName, std::fstream::out | std::fstream::binary);
  if (!file.is_open())
    return 3; // Error with file
  
  file.write(msg.data.data(), msg.data.size());
  file.close();
  message = msg.filename;
  return 4; // File was written succesfully
}

/* Should always run on separate thread */
//void monitor(int portNum) {
//  MonitorSock monSock = MonitorSock();
//  ConnectionSock conSock;
//  std::future<void> _connectionRequest; // Added here to supress warning
//  
//  //TODO handle this better
//  if (monSock.bind(portNum))
//      appError("Error while monitoring socket");
//
//  monSock.listen();
//  while (doListening) { 
//    if (acceptConnection) {
//      acceptConnection = false;
//      conSock.close();
//      conSock = monSock.accept();
//      // Establishing connection with socket
//      _connectionRequest = std::async(std::launch::async, [conSock]() { establishConnection(conSock); });
//    }
//
//    std::this_thread::sleep_for(std::chrono::milliseconds(100));
//  }
//  monSock.close();
//}

std::vector<ConnectionSock*> connectedSockets{};
// from https://medium.com/@hajorda/non-blocking-sockets-and-i-o-multiplexing-with-epoll-in-c-bd3d8e54c20a
int setNonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        displayError("fcntl(F_GETFL)");
        return -1;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        displayError("fcntl(F_SETFL)");
        return -1;
    }
    return 0;
}

/* Handles both connection requests and recieving requests (user recieves data from other peers) */
void handlePeerRequests(int portNum) {
  MonitorSock monSock = MonitorSock();

  if (monSock.bind(portNum))
      displayError("Error binding monitoring socket");
  
  setNonblocking(monSock.serverfd);

  monSock.listen();

  int epollFd = epoll_create1(0);
  if (epollFd == -1) {
    displayError("epoll_create1");
    return;
  }
  
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = monSock.serverfd;
  
  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, monSock.serverfd, &event) == -1) {
    displayError("epoll_ctl: serverfd");
    return;
  }

  constexpr int maxEvents = 10;
  struct epoll_event events[maxEvents];
  
  // TODO add some terminating condition here
  while (1) {
    int newEvents = epoll_wait(epollFd, events, maxEvents, -1);

    if (newEvents == -1) {
      displayError("epoll_wait");
      return;
    }
    
    for (int i = 0; i < newEvents; ++i) {
      // New connection request occured
      if (events[i].data.fd == monSock.serverfd) {
        while (1) {
          ConnectionSock* sock = monSock.accept(); 
          if (sock->clientfd == -1) {
            // All incoming connections have been processed
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            else {
              displayError("accept");
            }
          }
          setNonblocking(sock->clientfd); 
          event.events = EPOLLIN | EPOLLET;
          event.data.fd = sock->clientfd;
          
          // Adding new peer for monitoring by epoll
          if (epoll_ctl(epollFd, EPOLL_CTL_ADD, sock->clientfd, &event) == -1) {
            displayError("epoll_ctl: clientFd");
            sock->close();
            continue;
          }

          connectedSockets.push_back(sock);
        }
        

      } else {
        int clientFd = events[i].data.fd;
        auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [clientFd](ConnectionSock* sock) {return sock->clientfd == clientFd; });
        if (it == connectedSockets.end()) {
          displayError("Recieve request from non connected socket recieved");
          continue;
        }
        Msg msg{};
        // TODO add check for closing file (returned size by recv(fd, ...) == 0)
        if ((*it)->recieve(msg) != 0) {
          displayError("Error while recieving message");
          continue;
        }
        addMsg((*it)->getPeerIP(), msg, 1);
      }
    }
  }
}

// TODO use epoll instead of bunch of threads
/* Makes connection with tcp socket.
 * Displays recieved messages / files and prompts to send messages / files*/
void establishConnection(ConnectionSock conSock) {
  if (!conSock.exists())
    appError("Error while creating connection socket");

  //TODO make this into thread later
  isConnected = true;
  doConnection = false;
  // IP of other peer
  std::string peerIP = conSock.getPeerIP();
  std::string nick = "~<error_name>~";
  auto it = std::find_if(currentPeers.begin(), currentPeers.end(), [&peerIP](Peer p) {return peerIP == p.IP; });
  if (it != currentPeers.end()) {
    nick = it->nickname;
  }
  // Adding peer to the vector of connected Peers
  // TODO add a function which will delete the peers when they disconnect from chat
  Peer* p = &(*it);
  connectedPeers.push_back(p);

  bool isRequestSending = false;
  bool isRequestRecieving = false;
  std::future<bool> sendResponce;
  std::future<int> recieveResponce;
  Msg sendMsg, recvMsg;
  std::string recievedData, sendData, filename;

  // TODO add condition which will be possible to terminate from other thread
  while (sendData != "exit" && conSock.exists() && recievedData != "exit") {

    if (isInputReady) {
      // Start with a new request to send when there is none currently
      if (!isRequestSending) {
        isRequestSending = true;
        sendData = inputBuffer;
        sendMsg.filename = "";
        sendMsg.data = "";
        if (sendData.find("$file=") == sendData.npos) { // Text message
          sendMsg.data = sendData;
          sendResponce = std::async(std::launch::async, [&conSock, sendData]() { return sendMessage(conSock, sendData); });
        } else { // File
          int index = sendData.find("$file=");
          // TODO make filename structure be $file={<filename>}
          std::string filename = std::string(sendData.begin() + index + 6, sendData.end()); // We add + 6 bytes to start from the filename
          sendMsg.filename = filename;
          sendResponce = std::async(std::launch::async, [&conSock, filename]() { return sendFile(conSock, filename); });
        }
        inputBuffer = "";
        isInputReady = false;
      }
    }

    if (sendResponce.valid()) {
      auto sendStatus = sendResponce.wait_for(std::chrono::milliseconds(0));
      if (sendStatus == std::future_status::ready) {
        bool sendSuccess = sendResponce.get();
        if (!sendSuccess) {
          // TODO handle error
          sendMsg.data = "There was error sending your message.";
          sendMsg.filename = "";
          addMsg(peerIP, sendMsg, 0); // TODO add another creator for system errors and messages
        } else {
          addMsg(peerIP, sendMsg, 0); // The message was sent by this machine thats why second param is 0
        }
        isRequestSending = false;
      }
    }

    // Start new recieving request only if none is currently pending 
    if (!isRequestRecieving) {
      isRequestRecieving = true;
      recvMsg.filename = "";
      recvMsg.data = "";
      recieveResponce = std::async(std::launch::async, [&conSock, &recievedData]() { return recieve(conSock, recievedData); });
    }
    
    if (recieveResponce.valid()) {
      auto recieveStatus = recieveResponce.wait_for(std::chrono::milliseconds(0));

      if (recieveStatus == std::future_status::ready) {
        int n = recieveResponce.get(); 

        if (n == 1 || n == 3) // Some error
          continue; // Handle them later
        if (n == 2 || n == 4) {
          // TODO make messages be possible to send both text and file
          if (n == 2)  {
            recvMsg.data = recievedData;
            addMsg(peerIP, recvMsg, 1); // Data recieved so creator = 1
          }
          if (n == 4) {
            recvMsg.filename = recievedData;
            addMsg(peerIP, recvMsg, 1); // Data recievec so creator = 1
          }
        }
        isRequestRecieving = false;
      }
    }
  }
  isConnected = false;
  doConnection = true;
  acceptConnection = true;
  conSock.close();
}

// TODO convert IPOrHost with inet_pton into binary value
void establishConnection(std::string IPOrHost, int portNum) {
  if (doConnection) {
    ConnectionSock conSock = ConnectionSock(IPOrHost, std::to_string(portNum));
    establishConnection(conSock);
  }
}
