#include "socket.hpp"
#include "UI.hpp"
#include "socketFuncs.hpp"
#include "peer.hpp"
#include <iostream> // std::cout, std::cin
#include <vector> // std::vector
#include <string> // std::string
#include <thread> // std::thread


int main() {
  
  int choice;
  //int portNum = 55555;
  int discoveryPortNum = 5000;
  
  std::thread peerRequestsThread(handlePeerRequestsWrapper);
  while (connectionStatus == 0) {}
  if (connectionStatus == -1) {
    return -1;
  }
  std::thread discoveryThread(discoverPeers, appPortNum.load(), discoveryPortNum);

  std::string selectedIP;
  std::string nick;
  HOME_OPTIONS selectedOption = NO_OPTION;
  beginUI();
  while (showUI) {
    selectedOption = displayHomeScreen();
    switch (selectedOption) {
      case NEW_CHAT:
        selectedIP = displayNewChatScreen();
        if (selectedIP == "")
          break;
        establishConnection(selectedIP);
        displayChatScreen(selectedIP);
        break;
      case CHATS:
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
      case EXIT:
        handleRequests = false;
        doPeerDiscovery = false;
        showUI = false;
        endRequestHandling();
        break;
      default:
        break;
    }
  }
  endUI();

  discoveryThread.join();
  peerRequestsThread.join();
  
  return 0;
}
