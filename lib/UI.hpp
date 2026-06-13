#ifndef UI_HPP
#define UI_HPP

#include "socket.hpp" // For Msg struct
#include "peer.hpp" // For Peer struct
#include <vector> // std::vector
#include <utility> // std::pair
#include <string> // std::string
#include <curses.h> // for WINDOW
#include <atomic> // for std::atomic

enum HOME_OPTIONS {NEW_CHAT, CHATS, CHANGE_NICKNAME, EXIT};

extern int ROWS, COLS;

extern std::string inputBuffer;
extern bool updateScreen;
extern bool displayChat;
extern bool isInputReady;
extern std::vector<std::pair<int, Msg>> chatHistory;

extern std::vector<std::string> homeScreenOptions;
extern std::atomic<HOME_OPTIONS> selectedOption;


void addMsg(Msg msg, int creator);

void beginUI();
void endUI();

void displayChatLog(WINDOW* win);
void displayChatScreen();
int selector(std::vector<std::string>& options, WINDOW* win);
void displayHomeScreen();
std::string displayNewChatScreen();
std::string displayChangeNicknameScreen(std::string curNick);
std::string displayChatsScreen(std::vector<Peer>& connectedPeers);




#endif

#pragma once
