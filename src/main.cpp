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


int main() {
  
  int choice;
  int portNum = 55555;
  int discoveryPortNum = 5000;

  // Currently doing test for UDP discovery
  std::thread discoveryThread(discoverPeers, discoveryPortNum);
  std::thread peerRequestsThread(handlePeerRequests, portNum);

  std::string selectedIP;
  std::string nick;
  HOME_OPTIONS selectedOption = NO_OPTION;
  bool showUI = true;
  beginUI();
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
        handleRequests = false;
        doPeerDiscovery = false;
        showUI = false;
        endRequestHandling();
        break;
    }
  }
  endUI();

  discoveryThread.join();
  peerRequestsThread.join();
  
  return 0;
}
