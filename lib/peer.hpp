#ifndef PEER_HPP
#define PEER_HPP

#include <string>
#include <chrono>
#include <vector>
#include <mutex> // std::mutex
#include <atomic> // std::atomic
#include <memory> // std::shared_ptr
                  
// Stores all needed information about peers
struct Peer {
  std::string IP;
  std::string nickname;
  std::chrono::steady_clock::time_point lastSeen;
  uint16_t portNum;

  Peer(std::string IP, std::string nick) : IP(IP), nickname(nick) {};
  Peer() : IP(""), nickname("") {};
};
// Processes write new peers here and remove ones which did not respond for last 3 seconds
extern std::vector<Peer> discoveredPeers;
extern std::mutex discoveredPeersMutex;

extern std::atomic<bool> changeNickname;
extern std::string globalNickname;
extern std::mutex nickMutex;

extern std::vector<std::shared_ptr<Peer>> connectedPeers;
extern std::mutex connectedPeersMutex;

void discoverPeers(uint16_t portNum, uint16_t discoveryPortNum);
extern std::atomic<bool> doPeerDiscovery;
void showPeers();
std::vector<std::string> getMachineIPs();

#endif
