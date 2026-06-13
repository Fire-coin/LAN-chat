#ifndef PEER_HPP
#define PEER_HPP

#include <string>
#include <chrono>
#include <vector>
#include <mutex> // std::mutex

// Stores all needed information about peers
struct Peer {
  std::string IP;
  std::string nickname;
  std::chrono::steady_clock::time_point lastSeen;
};
// Processes write new peers here and remove ones which did not respond for last 3 seconds
extern std::vector<Peer> currentPeers;

extern std::mutex discoverMutex;

void discoverPeers(int portNum);
void showPeers();
std::vector<std::string> getMachineIPs();

#endif
