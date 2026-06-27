#include "socketFuncs.hpp"
#include "fileFuncs.hpp" // file handling
#include "UI.hpp" // To set global variables
#include "appErrors.hpp"
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
  if (error < 0)
    return error; // Error with file
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
    if (err < 0)
      return err;
  }
  message = "";


  // Find peer with this IP
  auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [&IP](ConnectionSock* s) {return IP == s->getPeerIP(); });
  if (it == connectedSockets.end()) {
      pushError("Peer is not connected", LCE_PEER_OFFLINE);
      return LCE_ALREADY_REPORTED; // Peer is not connected
    }

  int err = (*it)->sendMsg(msg);
  if (err == LCE_SEND) {
    pushError("Error sending message", LCE_SEND);
    return LCE_ALREADY_REPORTED;
  }
  if (err == LCE_EPOLL_CTL) {
    pushError("Error with epoll_ctl: sendMessage", LCE_EPOLL_CTL);
    return LCE_ALREADY_REPORTED;
  }

  addMsg(IP, msg, 0);
  return 0;
}

int recieve(ConnectionSock* socket, Msg& msg) {
  int n = socket->recieve(msg);
  if (n < 0)
    return n;

  if (msg.filename == "")  // Plain message
   return 0;
  
  int err = savePeerFile(".LAN-chat_files", msg);

  if (err == LCE_FILE_OP) {
    pushError("Problem writing a file; recieve", err);
    return LCE_ALREADY_REPORTED;
  }

  return 0; // File was written succesfully
}


std::vector<ConnectionSock*> connectedSockets{};
// from https://medium.com/@hajorda/non-blocking-sockets-and-i-o-multiplexing-with-epoll-in-c-bd3d8e54c20a
int setNonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
      pushError("fcntl: F_GETFL; setNonblocking", LCE_FCNTL);
      return LCE_ALREADY_REPORTED;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
      pushError("fcntl: F_SETFL; setNonblocking", LCE_FCNTL);
      return LCE_ALREADY_REPORTED;
    }
    return 0;
}

/* Handles both connection requests and recieving requests (user recieves data from other peers) */
//TODO make the return statement exit the app
void handlePeerRequests(int portNum) {
  epollFd = epoll_create1(0);
  if (epollFd == -1) {
    pushError("epoll_create1", LCE_EPOLL_CREATE);
    return;
  }

  MonitorSock monSock = MonitorSock(epollFd);

  if (monSock.bind(portNum) < 0) {
      pushError("Error binding monitoring socket", LCE_BIND);
      return;
  }
  
  int err = setNonblocking(monSock.serverfd);
  if (err < 0)
    return;

  monSock.listen();

  
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = monSock.serverfd;
  
  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, monSock.serverfd, &event) == -1) {
    pushError("epoll_ctl: serverfd; handlePeerRequests", LCE_EPOLL_CTL);
    return;
  }

  constexpr int maxEvents = 10;
  struct epoll_event events[maxEvents];
  
  while (handleRequests) {
    int newEvents = epoll_wait(epollFd, events, maxEvents, -1);
    if (newEvents == -1) {
      pushError("epoll_wait", LCE_EPOLL_WAIT);
      return;
    }
    
    for (int i = 0; i < newEvents; ++i) {
      // New connection request occured
      if (events[i].data.fd == monSock.serverfd) {
        while (1) {
          ConnectionSock* sock = monSock.accept(); 
          if (!sock->exists()) {
            // All incoming connections have been processed
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            else {
              pushError("accept", LCE_ACCEPT);
              return;
            }
          }
          int err = setNonblocking(sock->clientfd);
          if (err < 0) {
            return;
          }

          event.events = EPOLLIN | EPOLLET;
          event.data.fd = sock->clientfd;
          // Adding new peer for monitoring by epoll
          if (epoll_ctl(epollFd, EPOLL_CTL_ADD, sock->clientfd, &event) == -1) {
            pushError("epoll_ctl: clientFd; handlePeerRequests", LCE_EPOLL_CTL);
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
      } else { // There is a read or write available on a socket
        uint32_t curEvents = events[i].events;
        if (curEvents & EPOLLIN) {
          int clientFd = events[i].data.fd;
          auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [clientFd](ConnectionSock* sock) {return sock->clientfd == clientFd; });
          if (it == connectedSockets.end()) {
            pushError("Recieve request from non connected socket recieved", LCE_IMPOSSIBLE);
            continue;
          }
          Msg msg;
          // TODO add check for closing file (returned size by recv(fd, ...) == 0)
          int err;
          do {
            err = recieve(*it, msg);
          }
          while (err == LCE_NOT_FULL_MSG);
          // Other peer closed file descriptor
          if (err == LCE_FD_CLOSED) { 
            connectedSockets.erase(it);
            // TODO make the removing peer code 
            delete *it;
          }
          // TODO not sure to show user this message or not
          if (err == LCE_RECIEVE) {
            continue;
          }

          addMsg((*it)->getPeerIP(), msg, 1);

        } else if (curEvents & EPOLLOUT) {
          int clientFd = events[i].data.fd;
          auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [clientFd](ConnectionSock* sock) {return sock->clientfd == clientFd; });
          if (it == connectedSockets.end()) {
            pushError("Sent request on non connected socket", LCE_SEND);
            continue;
          }
          int err = (*it)->send();
          if (err == LCE_SEND) {
            pushError("Failed to send message", err);
            continue;
          }
          if (err == LCE_EPOLL_CTL) {
            pushError("epoll_ctl; handlePeerRequests from ConnectionSock::send", err);
            return;
          }
        }
      }
    }
  }
  close(epollFd);
}
void establishConnection(std::string& IPOrHost, int portNum) {
  // finding the peer to which user wants to connect in available peers
  auto it = std::find_if(currentPeers.begin(), currentPeers.end(), [&IPOrHost](Peer p) {return IPOrHost == p.IP; });
  if (it == currentPeers.end()) {
    pushError("Selected peer is not online", LCE_PEER_OFFLINE);
    return;
  }

  Peer* p = &(*it);
  connectedPeers.push_back(p);

  ConnectionSock* sock = new ConnectionSock(IPOrHost, std::to_string(portNum), epollFd);
  if (!sock->exists()) {
    pushError("Error establishing connection: ConnectionSock does not exist", LCE_CONNECTION_SOCK);
    return;
  }
  int err = sock->connect();
  if (err < 0) {
    pushError("Could not connect; establishConnection", err);
    return;
  }
  
  struct epoll_event event;
  err = setNonblocking(sock->clientfd);
  if (err < 0)
    return;

  event.events = EPOLLIN | EPOLLET;
  event.data.fd = sock->clientfd;


  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, sock->clientfd, &event) == -1) {
    pushError("Error connecting to socket: epoll_ctl, clientfd; establishConnection", LCE_EPOLL_CTL);
    sock->close();
    return;
  }

  connectedSockets.push_back(sock);
}
