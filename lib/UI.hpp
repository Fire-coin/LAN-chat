#ifndef UI_HPP
#define UI_HPP

#include "socket.hpp" // For Msg struct
#include "peer.hpp" // For Peer struct
#include <vector> // std::vector
#include <utility> // std::pair
#include <string> // std::string
#include <curses.h> // for WINDOW
#include <atomic> // for std::atomic
#include <unordered_map>

enum HOME_OPTIONS {NEW_CHAT, CHATS, CHANGE_NICKNAME, EXIT, NO_OPTION};

extern int ROWS, COLS;

extern std::string inputBuffer;
extern bool updateScreen;
extern bool displayChat;
extern bool isInputReady;
// A map that maps IP of peer to the chat history with that peer
using IPToHistoryMap = std::unordered_map<std::string, std::vector<std::pair<int, Msg>>>;
extern IPToHistoryMap chatHistory;

extern std::vector<std::string> homeScreenOptions;

void addMsg(std::string IP, Msg msg, int creator);

void beginUI();
void endUI();

void displayChatLog(WINDOW* win, std::string IP);
void displayChatScreen(std::string IP);
int selector(std::vector<std::string>& options, WINDOW* win);
HOME_OPTIONS displayHomeScreen();
std::string displayNewChatScreen();
std::string displayChangeNicknameScreen(std::string curNick);
std::string displayChatsScreen(std::vector<Peer>& connectedPeers);




#endif

#pragma once
