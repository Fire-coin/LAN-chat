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
#include <iostream>


namespace fs = std::filesystem;


std::atomic<bool> handleRequests = true;
int epollFd;


int processFile(std::string& filepath, Msg& msg) {
  int error = 0;
  std::string filename;
  std::fstream file;

  error = handleFile(filepath, file); // Opening the file and handling errors
  if (error > 0)
    return -1; // Error with file
  filename = getFilename(filepath);
  msg.filename = filename;

  // Get length of file
  file.seekg(0, file.end);
  int64_t fileLength = file.tellg();
  file.seekg(0, file.beg);
  
  // Get length of filename
  int16_t filenameLength = filename.size();
  // Reserve space for file data
  msg.data.resize(fileLength);
  file.read(msg.data.data(), fileLength);

  file.close();

  return 0;
}

int sendMessage(std::string& IP, std::string& message) {
  Msg msg{};
  int index = message.find("$file=");
  if (index == message.npos) { // Text message
    msg.data = message;
  } else { // File
    // TODO make filename structure be $file={<filepath>}
    std::string filepath = std::string(message.begin() + index + 6, message.end()); // We add + 6 bytes to start from the filepath
    int err = processFile(filepath, msg);
    if (err == -1)
      return 4;
  }
  message = "";


  // Find peer with this IP
  auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [&IP](ConnectionSock* s) {return IP == s->getPeerIP(); });
  if (it == connectedSockets.end())
    return 1; // Peer is not connected

  int err = (*it)->sendMsg(msg);
  if (err == -1)
    return -1; // epoll_ctl

  // TODO add historyMsg struct for history, and use Msg to communicate with socket classes instead of strings
  addMsg(IP, msg, 0);
  return 0;
}

int recieve(ConnectionSock* socket, Msg& msg) {
  int n = socket->recieve(msg);
  if (n != 0)
    return n;

  if (msg.filename == "")  // Plain message
   return 0;
  
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
    return 4; // Error with file
  
  file.write(msg.data.data(), msg.data.size());
  file.close();
  return 0; // File was written succesfully
}


std::vector<ConnectionSock*> connectedSockets{};
// from https://medium.com/@hajorda/non-blocking-sockets-and-i-o-multiplexing-with-epoll-in-c-bd3d8e54c20a
int setNonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return 1;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return 2;
    }
    return 0;
}

/* Handles both connection requests and recieving requests (user recieves data from other peers) */
void handlePeerRequests(int portNum) {
  epollFd = epoll_create1(0);
  if (epollFd == -1) {
    displayError("epoll_create1");
    return;
  }

  MonitorSock monSock = MonitorSock(epollFd);

  if (monSock.bind(portNum))
      displayError("Error binding monitoring socket");
  
  int err = setNonblocking(monSock.serverfd);
  if (err == 1) {
    displayError("fcntl(F_GETFL)");
    return;
  }
  if (err == 2) {
    displayError("fcntl(F_SETFL)");
    return;
  }

  monSock.listen();

  
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = monSock.serverfd;
  
  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, monSock.serverfd, &event) == -1) {
    displayError("epoll_ctl: serverfd");
    return;
  }

  constexpr int maxEvents = 10;
  struct epoll_event events[maxEvents];
  
  while (handleRequests) {
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
              return;
            }
          }
          int err = setNonblocking(sock->clientfd);
          if (err == 1) {
            displayError("fcntl(F_GETFL)");
            return;
          }
          if (err == 2) {
            displayError("fcntl(F_SETFL)");
            return;
          }

          event.events = EPOLLIN | EPOLLET;
          event.data.fd = sock->clientfd;
          // Adding new peer for monitoring by epoll
          if (epoll_ctl(epollFd, EPOLL_CTL_ADD, sock->clientfd, &event) == -1) {
            displayError("epoll_ctl: clientFd");
            sock->close();
            continue;
          }

          // Adding the peer to connected peers, so it can be detected by Chats screen
          std::string peerIP = sock->getPeerIP();
          auto it = std::find_if(currentPeers.begin(), currentPeers.end(), [&peerIP](Peer p) {return peerIP == p.IP; });

          Peer* p = &(*it);
          connectedPeers.push_back(p);

          connectedSockets.push_back(sock);
        }
      } else { // There is a read available on a socket
        // TODO check for events either EPOLLIN or EPOLLOUT
        int clientFd = events[i].data.fd;
        auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [clientFd](ConnectionSock* sock) {return sock->clientfd == clientFd; });
        if (it == connectedSockets.end()) {
          displayError("Recieve request from non connected socket recieved");
          continue;
        }
        Msg msg;
        // TODO add check for closing file (returned size by recv(fd, ...) == 0)
        int err;
        do {
          err = recieve(*it, msg);
          // Other peer closed file descriptor
          if (err == 2) { 
            connectedSockets.erase(it);
            // TODO make the removing peer code 
            delete *it;
          }
        }
        while (err == 1);
        addMsg((*it)->getPeerIP(), msg, 1);
      }
    }
  }
  close(epollFd);
}

void establishConnection(std::string& IPOrHost, int portNum) {
  // finding the peer to which user wants to connect in available peers
  auto it = std::find_if(currentPeers.begin(), currentPeers.end(), [&IPOrHost](Peer p) {return IPOrHost == p.IP; });
  if (it == currentPeers.end()) {
    displayError("Selected peer is not online");
    return;
  }

  Peer* p = &(*it);
  connectedPeers.push_back(p);

  ConnectionSock* sock = new ConnectionSock(IPOrHost, std::to_string(portNum), epollFd);
  if (!sock->exists()) {
    displayError("Error establishing connection: ConnectionSock does not exist");
    return;
  }
  
  struct epoll_event event;
  int err = setNonblocking(sock->clientfd);
  if (err == 1) {
    displayError("fcntl(F_GETFL)");
    return;
  }
  if (err == 2) {
    displayError("fcntl(F_SETFL)");
    return;
  }
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = sock->clientfd;


  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, sock->clientfd, &event) == -1) {
    displayError("Error connecting to socket: epoll_ctl, clientfd");
    sock->close();
    return;
  }

  connectedSockets.push_back(sock);
}
