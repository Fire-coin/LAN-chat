#include "socket.hpp"
#include "UI.hpp"
#include "socketFuncs.hpp"
#include "fileFuncs.hpp"
#include "peer.hpp"
#include "fileSelector.hpp"
#include <iostream> // std::cout, std::cin
#include <vector> // std::vector
#include <future> // std::future
#include <chrono> // for sleep_for function
#include <string> // std::string
#include <mutex> // std::mutex
#include <utility> // for std::pair


void error(const char* msg) {
  perror(msg);
  exit(1);
}

int main() {
  
  int choice;
  int portNum = 55555;
  int discoveryPortNum = 5000;
  // Both added to supress compiler warnings
  std::future<void> _monitorRequest;
  std::future<void> _sendingRequest;

  // Currently doing test for UDP discovery
  // TODO run this rather on a thread to be able to cancel it
  std::thread discoveryThread(discoverPeers, discoveryPortNum);
  //std::future<void> _discoverRequest = std::async(std::launch::async, [discoveryPortNum]() { discoverPeers(discoveryPortNum); });
  //_monitorRequest = std::async(std::launch::async, [portNum]() { monitor(portNum); });
  std::thread peerRequestsThread(handlePeerRequests, portNum);

  std::string selectedIP;
  std::string nick;
  HOME_OPTIONS selectedOption = NO_OPTION;
  bool showUI = true;
  beginUI();
  // testing file selector
  //displayFileSelector();
  //testing error screen
  displayError("Your computer has a virus");
  while (showUI) {
    selectedOption = displayHomeScreen();
    switch (selectedOption) {
      case NEW_CHAT:
        selectedIP = displayNewChatScreen();
        if (selectedIP == "")
          break;
        establishConnection(selectedIP, portNum);
        displayChatScreen(selectedIP);
        break;
      case CHATS: // TODO add mutex for connectedPeers
        selectedIP = displayChatsScreen(connectedPeers);
          
        if (selectedIP == "")
          break;
        displayChatScreen(selectedIP);
        break;
      case CHANGE_NICKNAME:
        nick = displayChangeNicknameScreen(globalNickname);
        if (nick == "~|NO_change|~")
          break;
        nickMutex.lock();
        if (nick != globalNickname) {
          globalNickname = nick;
          changeNickname = true;
        }
        nickMutex.unlock();
        break;
      case EXIT: // TODO end all processes
        showUI = false;
        break;
    }
  }
  endUI();

  handleRequests = false;
  doPeerDiscovery = false;

  discoveryThread.join();
  peerRequestsThread.join();
  
  return 0;
}
