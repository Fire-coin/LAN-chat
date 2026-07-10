#include "peer.hpp"
#include "socket.hpp"
#include "fileFuncs.hpp"
#include "socketFuncs.hpp"
#include "appErrors.hpp"
#include <future> // std::future
#include <thread> // std::async
#include <ifaddrs.h> // getifaddr, getnameinfo
#include <netdb.h> // for constants
#include <algorithm> // find_if


std::vector<Peer> discoveredPeers; // Stores discovered peers, which are currently online
std::mutex discoveredPeersMutex; // Used when manipulating with discoveredPeers

std::atomic<bool> changeNickname = false;
std::string globalNickname = "";
std::mutex nickMutex;
std::vector<std::shared_ptr<Peer>> connectedPeers;
std::mutex connectedPeersMutex;


std::atomic<bool> doPeerDiscovery = true;
// TODO get rid of async and use epoll for these ones also (maybe)
// TODO make the sockets to broadcast on all broadcast addresses that are connected to the device, e.g wlan and eth. Because they can have different subnet masks XXX this is optional for version 1
void discoverPeers(uint16_t portNum, uint16_t discoveryPortNum) {
  UDPDiscoverySock uSock = UDPDiscoverySock();
  setNonblocking(uSock.sockFd);

  if (uSock.bind(discoveryPortNum, portNum) < 0) {
    pushError("Discovery socket binding failure", LCE_BIND);
    return;
  }
  std::vector<std::string> IPs = getMachineIPs();
  
  bool isSendingPacket = false;
  bool isRecievingPacket = false;
  std::future<int> packetSendError, packetRecvError;

  const int sendDelay = 1000; // In milliseconds
  const int recvDelay = 500; // In milliseconds

  std::string nickname, IP; 
  uint16_t peerPortNum;

  while (doPeerDiscovery) {
    if (changeNickname) {
      nickMutex.lock();
      uSock.changeNickname(globalNickname);
      nickMutex.unlock();
      changeNickname = false;
    }
    
    /* If there is no sending task running, begin one */
    if (!isSendingPacket) {
      isSendingPacket = true;
      packetSendError = std::async(std::launch::async, [sendDelay, &uSock]() { return uSock.sendPresence(sendDelay); });
    }
    /* Check if sending task finished running */
    auto sendStatus = packetSendError.wait_for(std::chrono::milliseconds(0));
    if (sendStatus == std::future_status::ready) {
      isSendingPacket = false;
      int SendError = packetSendError.get();
      if (SendError < 0 && errno != EWOULDBLOCK) {
        pushError("Error while sending over UDP socket", LCE_SEND);
        continue;
      }
    }

    if (!isRecievingPacket) {
      isRecievingPacket = true;
      packetRecvError = std::async(std::launch::async, [recvDelay, &uSock, &IP, &nickname, &peerPortNum]() { return uSock.recievePacket(IP, nickname, peerPortNum, recvDelay); });
    }

    auto recvStatus = packetRecvError.wait_for(std::chrono::milliseconds(0));

    if (recvStatus == std::future_status::ready) {
      int recvError = packetRecvError.get();
      if (recvError == LCE_SEND) {
        pushError("Error while recieving UDP packet", recvError);
        continue;
      }
      if (recvError == LCE_BAD_PACKET) {
        continue;
      }
      if (recvError < 0 && errno == EWOULDBLOCK)
        continue;

      isRecievingPacket = false;
      discoveredPeersMutex.lock();
      auto it = find_if(discoveredPeers.begin(), discoveredPeers.end(), [IP](Peer& peer) { return peer.IP == IP; });
      if (it == discoveredPeers.end()) {
        // Ignore if IP is one of the machine ones
        if (find_if(IPs.begin(), IPs.end(), [IP](std::string& ip) {return ip == IP; }) != IPs.end()) {
          discoveredPeersMutex.unlock();
          continue;
        }

        Peer p;
        p.IP = IP;
        p.nickname = nickname;
        p.lastSeen = std::chrono::steady_clock::now();
        p.portNum = peerPortNum;
        
        discoveredPeers.push_back(p);
      } else {
        it->lastSeen = std::chrono::steady_clock::now();
        it->nickname = nickname;
        it->portNum = peerPortNum;
      }
     

      discoveredPeersMutex.unlock();
    }
    discoveredPeersMutex.lock();
    // Scan if some of the peers are offline
    for (auto it = discoveredPeers.begin(); it != discoveredPeers.end();) {
      // If last packet recieved from this IP was more than 3 seconds ago
      if (std::chrono::steady_clock::now() - it->lastSeen> std::chrono::milliseconds(3000)) {
        it = discoveredPeers.erase(it);
      }
      else
        ++it;
    }
    discoveredPeersMutex.unlock();
  }
}

/* Gets all IP addresses of current machine with ifaddrs */
std::vector<std::string> getMachineIPs() {
  std::vector<std::string> IPs;
  struct ifaddrs *ifaddr;
  int family, s;
  char host[NI_MAXHOST];

  if (getifaddrs(&ifaddr) == -1)
    pushError("getifaddrs", LCE_SYS_CALL);
  
  for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;

    family = ifa->ifa_addr->sa_family;

    if (family == AF_INET) {
      s = getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

      if (s != 0) {
        std::string msg = "getnameinfo failed: ";
        msg.append(gai_strerror(s));
        pushError(msg, LCE_SYS_CALL);
      }
      if (std::string(host) != "127.0.0.1")
        IPs.push_back(std::string(host));
    }
  }
  freeifaddrs(ifaddr);
  return IPs;
}
