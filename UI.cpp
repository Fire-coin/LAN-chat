#include "UI.hpp"
#include <curses.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>


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
    }
    
    ch = wgetch(inputWin);
    if (ch == ERR)
      continue;

    // 127 is delete
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
    }
  }
  
  delete[] separator;
  endwin();
}

/* Displays option menu and returns the index of selected option */
int selector(std::vector<std::string>& options) {
  initscr();
  int rows, cols;
  WINDOW* optionWin;
  
  getmaxyx(stdscr, rows, cols);

  optionWin = newwin(options.size(), cols, 0, 0);

  int current = 0;
  for (int i = 0; i < options.size(); ++i) {
    if (i == current) {
      wprintw(optionWin, "[*] %s", options[i].c_str());
    } else
      wprintw(optionWin, "[ ] %s", options[i].c_str());
  }
  int ch;
  while (1) {
    for (int i = 0; i < options.size(); ++i) {
      mvwdelch(optionWin, i, 1);
      if (i == current)
        mvwprintw(optionWin, i, 1, "*");
    }
    ch = wgetch(optionWin);
    if (ch == 10) {
      return current;
    }
  }
  endwin();
}

