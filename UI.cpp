#include "UI.hpp"
#include <curses.h>
#include <stdlib.h>
#include <cstring>

bool acceptInputToBuffer = true;
std::string inputBuffer = "";
bool updateScreen = false;
bool displayChat = true;
bool isInputReady = false;
std::vector<std::pair<int, Msg>> chatHistory;



// Inspired from https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html#INIT 
void displayChatLog(WINDOW* win) {
  int x, y;
  wclear(win);
  // TODO add color to distinguish between peers
  for (auto p : chatHistory) {
    getyx(win, y, x);
    if (p.first == 0) {
      if (p.second.filename == "") 
        mvwprintw(win, y + 1, 0, "You have sent message: %s\n", p.second.data.c_str());
      else
        mvwprintw(win, y + 1, 0, "You have sent a file: %s\n", p.second.filename.c_str());
    }
    else {
      if (p.second.filename == "")
        mvwprintw(win, y + 1, 0, "Message recieved: %s\n", p.second.data.c_str());
      else
        mvwprintw(win, y + 1, 0, "File recieved: %s\n", p.second.filename.c_str());
    }
  }
  wrefresh(win); // To display it into real terminal
}

// TODO move code from displayChatScreen here
std::string getUserInput() {
  return std::string("");
}

void displayChatScreen() {
  initscr();
  noecho();
  halfdelay(2); // Check for request every 200 milliseconds that user does not type
  WINDOW* chatWin;
  WINDOW* inputWin;
  int rows, cols, ch;

  getmaxyx(stdscr, rows, cols);

  chatWin = newwin(rows - 3, cols, 0, 0);
  inputWin = newwin(3, cols, rows - 3, 0);
  
  wclear(chatWin);
  wclear(inputWin);

  // Display a bar separating chat and input windows
  char* separator = new char[cols];
  memset(separator, '-', cols);
  mvwprintw(inputWin, 0, 0, "%s", separator);
  
  wrefresh(chatWin);
  wrefresh(inputWin);

  while (displayChat) {
    if (updateScreen) {
      displayChatLog(chatWin);
      updateScreen = false;
      //TODO REMOVE AFTER TEST
      inputBuffer = "";
      isInputReady = false;
    }
    
    ch = wgetch(inputWin);
    if (ch == ERR)
      continue;
    
    // 127 is delete
    // TODO handle delete separately
    if (ch >= ' ' && ch <= 127 && !isInputReady) {
      if (ch != 127) {
        inputBuffer.append(reinterpret_cast<char*>(&ch));
        waddch(inputWin, ch);
        wrefresh(inputWin);
      } else {
        int x, y;
        getyx(inputWin, y, x);
        if (x == 0)
          continue;
        inputBuffer.erase(inputBuffer.size() - 1, 1); // Erase last character from buffer
        mvwdelch(inputWin, y, x - 1);
        wrefresh(inputWin);
      }
    }
    
    // Enter has been pressed
    if (ch == 10) {
      isInputReady = true;
      // TODO make better way to clear the line
      wclear(inputWin);
      mvwprintw(inputWin, 0, 0, "%s", separator);
      wrefresh(inputWin);
      // TODO FOR TEST ONLY REMOVE LATER
      Msg msg;
      msg.filename = "";
      msg.data = "";
      if (inputBuffer.find("#file=") == inputBuffer.npos) { // Text message
        msg.data = inputBuffer;
      } else { // File
        int index = inputBuffer.find("#file=");
        std::string filename = std::string(inputBuffer.begin() + index + 7, inputBuffer.end()); // We add + 7 bytes to start from the filename
        msg.filename = filename;
      }
      std::pair<int, Msg> sendMsg = std::pair<int, Msg>(0, msg);
      chatHistory.push_back(sendMsg);
      updateScreen = true;
    }
  }
  
  delete[] separator;
  endwin();
}
