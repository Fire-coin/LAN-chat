#include "UI.hpp"
#include <curses.h>
#include <cstring> // memset


std::string inputBuffer = "";
bool updateScreen = false;
bool displayChat = true;
bool isInputReady = false;
std::vector<std::pair<int, Msg>> chatHistory;

std::vector<std::string> homeScreenOptions = {"New Chat", "Chats", "Change Nickname", "Exit"};
std::atomic<HOME_OPTIONS> selectedOption;


void addMsg(Msg msg, int creator) {
  std::pair<int, Msg> temp = std::pair<int, Msg>(creator, msg);
  chatHistory.push_back(temp); //TODO maybe use emplace_back
  updateScreen = true;
}

void beginUI() {
  initscr();
  noecho();
  halfdelay(1);
}

void endUI() {
  endwin();
}

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
int selector(std::vector<std::string>& options, WINDOW* win) {
  noecho();
  cbreak(); // Immediately get key press

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible

  int current = 0;
  for (int i = 0; i < options.size(); ++i) {
    if (i == current) {
      wprintw(win, "[*] %s\n", options[i].c_str());
    } else
      wprintw(win, "[ ] %s\n", options[i].c_str());
  }
  wrefresh(win);
  int ch;
  bool update = false;
  while (1) {
    ch = wgetch(win);
    if (ch == 10) {
      wclear(win);
      wrefresh(win);
      return current;
    }
    update = true;
    switch (ch) {
      case 'k':
      case KEY_UP:
        if (current == 0)
          current = options.size() - 1;
        else
          current--;
        break;
      case 'j':
      case KEY_DOWN:
        if (current == options.size() - 1)
          current = 0;
        else
          current++;
        break;
      default:
        update = false;
    }
    
    if (update) {
      for (int i = 0; i < options.size(); ++i) {
        mvwprintw(win, i, 1, " ");
        if (i == current)
          mvwprintw(win, i, 1, "*");
      }
      wrefresh(win);
    }
  }
}

void displayHomeScreen() {
  // TODO make and display logo
  clear();
  int x, y;
  int rows, cols;
  
  getmaxyx(stdscr, rows, cols);
  getyx(stdscr, y, x);
  // TODO use map of WINDOW* and std::string to store windows and just refresh them
  WINDOW* homeWin;
  WINDOW* optionWin;
  
  homeWin = newwin(rows, cols, 0, 0);
  wprintw(homeWin, "====LAN-chat====\n");

  optionWin = newwin(homeScreenOptions.size(), cols, y + 1, 0);
  
  refresh();
  //wrefresh(homeWin);
  //wrefresh(optionWin);
  int userChoice = selector(homeScreenOptions, optionWin);

  switch (userChoice) {
    case 0: // New chat
      selectedOption = NEW_CHAT;
      break;
    case 1: // Chats
      selectedOption = CHATS;
      break;
    case 2: // Change nickname
      selectedOption = CHANGE_NICKNAME;
      break;
    case 3: // Exit
      selectedOption = EXIT;
      break;
    default: // How tf u managed to corrupt memory so bad to get new index?
      break;
  }
}
