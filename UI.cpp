#include "UI.hpp"
#include "peer.hpp"
#include <curses.h>
#include <cstring> // memset
#include <assert.h>

int ROWS, COLS;

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
  getmaxyx(stdscr, ROWS, COLS);
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
  int ch;


  chatWin = newwin(ROWS - 3, COLS, 0, 0);
  inputWin = newwin(3, COLS, ROWS - 3, 0);
  
  wclear(chatWin);
  wclear(inputWin);

  // Display a bar separating chat and input windows
  char* separator = new char[COLS];
  memset(separator, '-', COLS);
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
  
  getyx(stdscr, y, x);
  // TODO use map of WINDOW* and std::string to store windows and just refresh them
  WINDOW* homeWin;
  WINDOW* optionWin;
  
  homeWin = newwin(ROWS, COLS, 0, 0);
  wprintw(homeWin, "====LAN-chat====\n");

  optionWin = newwin(homeScreenOptions.size(), COLS, y + 1, 0);
  
  refresh();
  wrefresh(homeWin);
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

std::string displayNewChatScreen() {
  clear();
  WINDOW* win;
  WINDOW* optionWin;
  win = newwin(ROWS, COLS, 0, 0);
  optionWin = newwin(ROWS - 2, COLS, 2, 0);

  // TODO make better header
  wprintw(win, "====SELECT PEER TO CONNECT====\n");
  wprintw(win, "        IP          |    nickname\n");
  
  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  halfdelay(1);

  discoverMutex.lock();
  int lastSize = currentPeers.size();


  int current = 0;
  for (int i = 0; i < currentPeers.size(); ++i) {
    if (i == current) {
      wprintw(optionWin, "[*] %s|%s\n", currentPeers[i].IP.c_str(), currentPeers[i].nickname.c_str());
    } else
      wprintw(optionWin, "[ ] %s|%s\n", currentPeers[i].IP.c_str(), currentPeers[i].nickname.c_str());
  }
  discoverMutex.unlock();
  wrefresh(win);
  int ch;
  bool update = false;
  // TODO add option for Esc key
  while (1) {
    ch = wgetch(optionWin);

    if (ch == 10) {
      discoverMutex.lock();
      assert(current < currentPeers.size());
      std::string out = currentPeers.size() > 0 ? currentPeers[current].IP : "";
      discoverMutex.unlock();

      wclear(optionWin);
      wrefresh(optionWin);
      return out;
    }
    update = true;
    switch (ch) {
      case 'k':
      case KEY_UP:
        if (current == 0)
          current = currentPeers.size() > 0 ? currentPeers.size() - 1 : 0;
        else
          current--;
        break;
      case 'j':
      case KEY_DOWN:
        if (currentPeers.size() < 1)
          current = 0;
        if (current == currentPeers.size() - 1)
          current = 0;
        else
          current++;
        break;
      default:
        update = false;
    }
    
    if (update) {
      for (int i = 0; i < currentPeers.size(); ++i) {
        mvwprintw(optionWin, i, 1, " ");
        if (i == current)
          mvwprintw(optionWin, i, 1, "*");
      }
      wrefresh(optionWin);
    }
    discoverMutex.lock();
    if (currentPeers.size() != lastSize) {
      if (lastSize > currentPeers.size()) // If list got smaller and selected was last option, place selected at the new last option
        current = currentPeers.size() - 1;
      lastSize = currentPeers.size();
      wclear(optionWin);
      wmove(optionWin, 0, 0);
      
      for (int i = 0; i < currentPeers.size(); ++i) {
        if (i == current) {
          wprintw(optionWin, "[*] %s|%s\n", currentPeers[i].IP.c_str(), currentPeers[i].nickname.c_str());
        } else
          wprintw(optionWin, "[ ] %s|%s\n", currentPeers[i].IP.c_str(), currentPeers[i].nickname.c_str());
      }
      wrefresh(optionWin);
    }
    discoverMutex.unlock();
  }
}
