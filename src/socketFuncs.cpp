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
#include <sys/eventfd.h>


std::atomic<int> connectionStatus = 0;
std::atomic<uint16_t> appPortNum = 55555;

std::atomic<bool> handleRequests = true;
int epollFd;

int endRequestHandling() {
  int efd = eventfd(0, EFD_NONBLOCK);
  if (efd == -1)
    return -1;
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = efd;

  epoll_ctl(epollFd, EPOLL_CTL_ADD, efd, &event);
  uint64_t u = 1;
  write(efd, &u, sizeof(u));
  return 0;
}

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
  int index = message.find("$file={");
  if (index == message.npos) { // Text message
    msg.data = message;
  } else { // File
    int endOfPathIndex = message.find("}");
    std::string filepath = std::string(message.begin() + index + 7, message.begin() + endOfPathIndex); // We add + 7 bytes to start from the filepath
    int err = processFile(filepath, msg);
    if (err < 0) {
      message.clear();
      return err;
    }
  }
  message.clear(); 


  // Find peer with this IP
  connectedSocketsMutex.lock();
  auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [&IP](ConnectionSock* s) {return IP == s->getPeerIP(); });
  if (it == connectedSockets.end()) {
      pushError("Peer is not connected", LCE_PEER_OFFLINE);
      connectedSocketsMutex.unlock();
      return LCE_ALREADY_REPORTED; // Peer is not connected
  }

  int err = (*it)->sendMsg(msg);
  connectedSocketsMutex.unlock();

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
std::string modifiedFile = "";

int recieve(ConnectionSock* socket, Msg& msg) {
  int n;
  try {
    n = socket->recieve(msg);
  } catch (const std::exception& e) {

    pushError(e.what(), -1);
  }
  
  if (n == LCE_NOT_FULL_PACKAGE || n == LCE_FULL_PACKAGE) {
    //TODO fix this case
    if (msg.filename.empty())
      return LCE_RECIEVE;
    
    if (modifiedFile != msg.filename) {
      int err = savePeerFile(".LAN-chat_files", msg, true);

      if (err == LCE_FILE_OP) {
        pushError("Problem writing a file; recieve", err);
        return LCE_ALREADY_REPORTED;
      }
      modifiedFile = msg.filename;
    } else {
      int err = savePeerFile(".LAN-chat_files", msg, false);

      if (err == LCE_FILE_OP) {
        pushError("Problem writing a file; recieve", err);
        return LCE_ALREADY_REPORTED;
      }
    }
    if (n == LCE_FULL_PACKAGE) {
      modifiedFile.clear();
      return 0;
    }
    return LCE_NOT_FULL_MSG;
  }

  if (n < 0)
    return n;

  //if (msg.filename == "")  // Plain message
  // return 0;
  //
  //int err = savePeerFile(".LAN-chat_files", msg);

  //if (err == LCE_FILE_OP) {
  //  pushError("Problem writing a file; recieve", err);
  //  return LCE_ALREADY_REPORTED;
  //}

  return 0; // File was written succesfully
}


std::vector<ConnectionSock*> connectedSockets{};
std::mutex connectedSocketsMutex;
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
void handlePeerRequestsWrapper() {
  handlePeerRequests();
  handleRequests = false;
  doPeerDiscovery = false;
  showUI = false;
}

int closePeerConnection(std::string IP) {
  connectedPeersMutex.lock();
  connectedSocketsMutex.lock();

  auto it = std::find_if(connectedPeers.begin(), connectedPeers.end(), [IP](std::shared_ptr<Peer> p) { return p->IP == IP; });
  if (it == connectedPeers.end()) {
    pushError("Specified peer is not connected; closePeer", LCE_PEER_OFFLINE);
    connectedPeersMutex.unlock();
    connectedSocketsMutex.unlock();
    return LCE_ALREADY_REPORTED;
  }
  /* Erasing the peer from connectedPeers */
  connectedPeers.erase(it);
  
  auto it2 = std::find_if(connectedSockets.begin(), connectedSockets.end(), [IP](ConnectionSock* s) { return s->getPeerIP() == IP; });
  if (it2 == connectedSockets.end()) {
    pushError("Specified socket is not connected; closePeer", LCE_PEER_OFFLINE);
    connectedPeersMutex.unlock();
    connectedSocketsMutex.unlock();
    return LCE_ALREADY_REPORTED;
  }
  /* Removing the socket file descriptor from epoll list */
  if (epoll_ctl(epollFd, EPOLL_CTL_DEL, (*it2)->clientfd, NULL) == -1) {
    pushError("epoll_ctl: EPOLL_CTL_DEL; closePeerConnection;", LCE_EPOLL_CTL);
    connectedPeersMutex.unlock();
    connectedSocketsMutex.unlock();
    return LCE_ALREADY_REPORTED;
  }
  /* Closing the socket file deescriptor */
  (*it2)->close();
  /* Free the memory */
  delete *it2;
  /* Erase the socket from connectedSockets */
  connectedSockets.erase(it2);

  connectedPeersMutex.unlock();
  connectedSocketsMutex.unlock();
  return 0;
}


/* Handles both connection requests and recieving requests (user recieves data from other peers) */
void handlePeerRequests() {
  epollFd = epoll_create1(0);
  if (epollFd == -1) {
    pushError("epoll_create1", LCE_EPOLL_CREATE);
    return;
  }

  MonitorSock monSock = MonitorSock(epollFd);
   
  uint16_t i = 0;
  int err = -1;
  /* trying to bind monitoring socket to an adress, if it fails try next one
   * this is repeated up to 10 times (theoretically 10 apps can run on the same machine)*/
  do {
    err = monSock.bind(appPortNum + i);
    i++;
  } while (err < 0 && i < 10);

  if (err < 0) {
      connectionStatus = -1;
      pushError("Error binding monitoring socket", LCE_BIND);
      return;
  }

  connectionStatus = 1;
  
  err = setNonblocking(monSock.serverfd);
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
          discoveredPeersMutex.lock();
          auto it = std::find_if(discoveredPeers.begin(), discoveredPeers.end(), [&peerIP](Peer p) {return peerIP == p.IP; });

          std::shared_ptr<Peer> p = std::make_shared<Peer>(*it);
          discoveredPeersMutex.unlock();
          connectedPeersMutex.lock();
          connectedPeers.push_back(p);
          connectedPeersMutex.unlock();
          
          connectedSocketsMutex.lock();
          connectedSockets.push_back(sock);
          connectedSocketsMutex.unlock();
        }
      } else { // There is a read or write available on a socket
        uint32_t curEvents = events[i].events;
        if (curEvents & EPOLLIN) {
          int clientFd = events[i].data.fd;
          connectedSocketsMutex.lock();
          auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [clientFd](ConnectionSock* sock) {return sock->clientfd == clientFd; });
          if (it == connectedSockets.end()) {
            pushError("Recieve request from non connected socket recieved", LCE_IMPOSSIBLE);
            connectedSocketsMutex.unlock();
            continue;
          }
          Msg msg;
          int err;
          do {
            err = recieve(*it, msg);
          }
          while (err == LCE_NOT_FULL_MSG);
          

          // Other peer closed file descriptor
          if (err == LCE_FD_CLOSED) { 
            connectedSocketsMutex.unlock();
            closePeerConnection((*it)->getPeerIP());
            continue;
          }
          /* There was error recieving message from other peer */
          if (err == LCE_RECIEVE) {
            connectedSocketsMutex.unlock();
            continue;
          }

          addMsg((*it)->getPeerIP(), msg, 1);

          connectedSocketsMutex.unlock();

        } else if (curEvents & EPOLLOUT) {
          connectedSocketsMutex.lock();
          int clientFd = events[i].data.fd;
          auto it = std::find_if(connectedSockets.begin(), connectedSockets.end(), [clientFd](ConnectionSock* sock) {return sock->clientfd == clientFd; });
          if (it == connectedSockets.end()) {
            pushError("Sent request on non connected socket", LCE_SEND);
            connectedSocketsMutex.unlock();
            continue;
          }
          int err = (*it)->send();
          connectedSocketsMutex.unlock();

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
void establishConnection(std::string& IPOrHost) {
  // finding the peer to which user wants to connect in available peers
  discoveredPeersMutex.lock();
  auto it = std::find_if(discoveredPeers.begin(), discoveredPeers.end(), [&IPOrHost](Peer p) {return IPOrHost == p.IP; });
  if (it == discoveredPeers.end()) {
    pushError("Selected peer is not online", LCE_PEER_OFFLINE);
    discoveredPeersMutex.unlock();
    return;
  }

  std::shared_ptr<Peer> p = std::make_shared<Peer>(*it);
  discoveredPeersMutex.unlock();

  connectedPeersMutex.lock();
  connectedPeers.push_back(p);
  connectedPeersMutex.unlock();

  ConnectionSock* sock = new ConnectionSock(IPOrHost, std::to_string(p->portNum), epollFd);
  pushError("I am here " + std::to_string(sock->clientfd), -1);
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
  
  connectedSocketsMutex.lock();
  connectedSockets.push_back(sock);
  connectedSocketsMutex.unlock();
}
