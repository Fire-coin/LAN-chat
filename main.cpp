#include "socket.hpp"
#include "UI.hpp"
#include "socketFuncs.hpp"
#include "fileFuncs.hpp"
#include <iostream> // std::cout, std::cin
#include <vector> // std::vector
#include <future> // std::future
#include <chrono> // for sleep_for function
#include <string> // std::string
#include <mutex> // std::mutex
#include <algorithm> // find_if
#include <utility> // for std::pair
#include <ifaddrs.h> // getifaddr, getnameinfo
#include <netdb.h> // for constants


void error(const char* msg) {
  perror(msg);
  exit(1);
}



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


int main() {
  
  int choice;
  std::string buf; // Used to clear std::cin buffer
  int portNum = 55555;
  int discoveryPortNum = 5000;
  // Both added to supress compiler warnings
  std::future<void> _monitorRequest;
  std::future<void> _sendingRequest;

  // Currently doing test for UDP discovery
  std::future<void> _discoverRequest = std::async(std::launch::async, [discoveryPortNum]() { discoverPeers(discoveryPortNum); });
  
  //Testing curses
  //displayChatScreen();
  std::vector<std::string> options = {"New chat", "Chats", "Change nickname", "Exit"};
  //selector(options);
  


  std::cout << "Do you want to start monitoring(1 / 0): ";
  std::cin >> choice;
  
  if (choice == 1)
  _monitorRequest = std::async(std::launch::async, [portNum]() { monitor(portNum); });
 
  //TODO run this on a separate thread so it can be cancelled unlike async
  while (doListening && !isConnected) {
    std::cout << "Start connection(0 / 1): ";
    std::cin >> choice;
    
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
    
      if (choice == 1)
        exit(0);
    }
  }
  
  
  return 0;
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

// TODO remake this function for curses.h and combine with selector function
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


