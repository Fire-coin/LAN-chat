#include "UI.hpp"
#include "peer.hpp"
#include "formatString.hpp"
#include "fileSelector.hpp"
#include "appErrors.hpp"
#include <ncurses.h>
#include <cstring> // memset
#include <algorithm>
#include <cassert>
#include "socketFuncs.hpp"

int ROWS, COLS;
std::string inputBuffer = "";
std::atomic<bool> updateScreen = false;
bool displayChat = true;
std::atomic<bool> showUI = true;
IPToHistoryMap chatHistory;
std::mutex chatHistoryMutex;

std::mutex notificationMutex;
std::unordered_map<std::string, int> notifications;
int totalNotifications = 0;

std::vector<std::string> homeScreenOptions = {"New Chat", "Chats", "Change Nickname", "Exit"};
std::atomic<HOME_OPTIONS> selectedOption;


int checkError() {
  errorMutex.lock();
  if (errorQueue.empty()) {
    errorMutex.unlock();
    return 0;
  }
  
  LCError lce = errorQueue.front();
  errorQueue.pop();
  displayError(lce.message);
  errorMutex.unlock();
  return lce.errorCode;
}

void displayError(std::string error) {
  WINDOW* win;
  curs_set(0);
  halfdelay(5);
  // Center the window
  int startY = ROWS / 4;
  int startX = COLS / 4;
  win = newwin(ROWS / 2, COLS / 2, startY, startX);
  box(win, 0, 0);
  mvwprintw(win, 1, 1, "%s", format("====LAN-chat Error====", COLS / 2 - 2, '^').c_str());
  mvwprintw(win, 2, 1, "%s", format(error, COLS / 2 - 2, '^').c_str());
  mvwprintw(win, ROWS / 2 - 2, 1, "%s", format("Press Escape to close this window", COLS / 2 - 2, '^').c_str());
  wrefresh(win);
  int ch = ERR;
  while (ch != 27) {
    ch = wgetch(win);
  }
  wclear(win);
  wrefresh(win);
}

void addMsg(std::string IP, Msg msg, int creator) {
  chatHistoryMutex.lock();
  if (creator == 1) { // A message recieved
    notificationMutex.lock();
    notifications[IP]++;
    totalNotifications++;
    notificationMutex.unlock();
  }
  
  ChatMsg cMsg{};
  cMsg.isFile = !msg.filename.empty();
  if (cMsg.isFile)
    cMsg.data = msg.filename;
  else
    cMsg.data = msg.data;
  cMsg.creator = creator;

  chatHistory[IP].push_back(cMsg);
  updateScreen = true;
  chatHistoryMutex.unlock();
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
void displayChatLog(WINDOW* win, std::string IP, uint32_t scrollIndex) {
  int x, y = 0;
  wclear(win);
  chatHistoryMutex.lock();
  if (chatHistory.find(IP) == chatHistory.end()) {
    chatHistoryMutex.unlock();
    return;
  }
  auto& curChat = chatHistory[IP];
  if (curChat.empty()) {
    chatHistoryMutex.unlock();
    return;
  }
  if (scrollIndex > curChat.size() - 1)
    scrollIndex = curChat.size() - 1;

  for (auto it = curChat.begin() + scrollIndex; it != curChat.end(); ++it) {
    getyx(win, y, x);
    if (it->creator == 0) { // Message sent
      if (!it->isFile) 
        mvwprintw(win, y + 1, 0, "Message sent: %s\n", it->data.c_str());
      else
        mvwprintw(win, y + 1, 0, "File sent: %s\n", it->data.c_str());
    } else { // Message recieved
      if (!it->isFile)
        mvwprintw(win, y + 1, 0, "Message recieved: %s\n", it->data.c_str());
      else
        mvwprintw(win, y + 1, 0, "File recieved: %s\n", it->data.c_str());
    }
  }
  // TODO add color to distinguish between peers
  notificationMutex.lock();
  totalNotifications -= notifications[IP];
  notifications[IP] = 0;
  notificationMutex.unlock();
  wrefresh(win); // To display it into real terminal
  chatHistoryMutex.unlock();
}

void displayChatScreen(std::string IP) {
  noecho();
  WINDOW* chatWin;
  WINDOW* inputWin;
  int ch;
  uint32_t scrollIndex = 0;
  halfdelay(1); // Check for request every 200 milliseconds that user does not type


  chatWin = newwin(ROWS - 3, COLS, 0, 0);
  inputWin = newwin(3, COLS, ROWS - 3, 0);
  keypad(inputWin, true);

  wclear(chatWin);
  wclear(inputWin);

  // Display a bar separating chat and input windows
  char* separator = new char[COLS];
  memset(separator, '-', COLS);
  mvwprintw(inputWin, 0, 0, "%s", separator);

  wrefresh(chatWin);
  wrefresh(inputWin);

  displayChat = true;
  updateScreen = true;
  wprintw(inputWin, "%s", inputBuffer.c_str());
  wrefresh(inputWin);
  while (displayChat) {
    if (checkError() != 0) {
      while (checkError() != 0) {}
      updateScreen = true;
    }
    if (updateScreen) {
      displayChatLog(chatWin, IP, scrollIndex);
      updateScreen = false;
    }
    
    ch = wgetch(inputWin);
    if (ch == ERR)
      continue;

    // 127 is delete
    if (ch >= ' ' && ch <= 127 || ch == KEY_BACKSPACE) {
      if (ch != KEY_BACKSPACE && ch != 127) {
        inputBuffer.append(reinterpret_cast<char*>(&ch));
        waddch(inputWin, ch);
      } else {
        int x, y;
        getyx(inputWin, y, x);
        if (x == 0)
          continue;
        inputBuffer.erase(inputBuffer.size() - 1, 1); // Erase last character from buffer
        mvwdelch(inputWin, y, x - 1);
      }

      int index = inputBuffer.find(":file");
      if (index != inputBuffer.npos) {
        updateScreen = true;
        inputBuffer.erase(index, 5); // Erase the :file
        std::string filename = displayFileSelector();
        if (filename != "") {
          inputBuffer = inputBuffer + "$file={" + filename + "}";
        }
        mvwprintw(inputWin, 1, 0, "%s", format(inputBuffer, COLS, '<').c_str());
        wmove(inputWin, 1, inputBuffer.size());
      }
      wrefresh(inputWin);
      /*XXX Extremely ugly fix because I am unable to locate the issue*/
      /* XXX The issue is that when file selector is displayed and destroyed, no new messages
       * render from other peer - displayChatLog function is not called at all, despite addMsg
       * working properly.
       * This fix is more memory inneficient, but if you take into account that most people will send less than 10 files in one go without switching chat, I believe it is good enough.*/
      displayChatScreen(IP);
      break;
    }
    
    // Enter has been pressed
    if (ch == 10) {
      sendMessage(IP, inputBuffer);
      // TODO make better way to clear the line
      wclear(inputWin);
      mvwprintw(inputWin, 0, 0, "%s", separator);
      wrefresh(inputWin);
    }
    if (ch == KEY_UP) {
      if (scrollIndex > 0) {
        scrollIndex--;
        updateScreen = true;
      }
    }
    if (ch == KEY_DOWN) {
      chatHistoryMutex.lock();
      if (scrollIndex < chatHistory[IP].size()) {
        scrollIndex++;
        updateScreen = true;
      }
      chatHistoryMutex.unlock();
    }

    if (ch == 27) { // Escape
      clear();
      refresh();
      displayChat = false;
    }

    if (ch == '|')
      pushError("Test error screen", -1);
  }
  
  delete[] separator;
  endwin();
}

/* Displays option menu and returns the index of selected option */
int selector(std::vector<std::string>& options, WINDOW* win, int rows, int cols) {
  noecho();
  cbreak();

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  set_escdelay(25);

  int current = 0;
  for (int i = 0; i < rows; ++i) {
    if (i >= options.size()) {
      wprintw(win, "%s\n", format("", cols - 1, '<').c_str());
      continue;
    }
    if (i == current) {
      wprintw(win, "[*] %s\n", options[i].c_str());
    } else
      wprintw(win, "[ ] %s\n", options[i].c_str());
  }
  wrefresh(win);
  int ch;
  bool update = false;
  int arrayBegin = 0;
  while (1) {
    ch = wgetch(win);
    
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
      case 10: // Enter key
        wclear(win);
        wrefresh(win);
        return current;
      case 27: // Escape
        wclear(win);
        wrefresh(win);
        keypad(win, false);
        return -1;
      default:
        update = false;
    }

    if (ch == '|')
      pushError("Test error screen", -1);
    if (checkError() != 0) {
      while (checkError() != 0) {}
      update = true;
    }

    if (update) {
      if (arrayBegin != current / rows) { // It does not fit into single window, scroll down
        arrayBegin = (current / rows) * rows; // should work, lazy to prove why
        for (int i = 0; i < rows; ++i) {
          if (i + arrayBegin < options.size())
            mvwprintw(win, i, 0, "[ ] %s", format(options[i + arrayBegin], cols, '<').c_str());
          else
            mvwprintw(win, i, 0, "%s", format("", cols, '^').c_str());
        }
      }
      for (int i = 0; i < rows; ++i) {
        mvwprintw(win, i, 1, " ");
        if (i == (current % rows))
          mvwprintw(win, i, 1, "*");
      }
      wrefresh(win);
    }
  }
}

HOME_OPTIONS displayHomeScreen() {
  // TODO make and display logo
  clear();
  int x, y;
  
  getyx(stdscr, y, x);
  WINDOW* homeWin;
  WINDOW* win;
  
  homeWin = newwin(ROWS, COLS, 0, 0);
  wprintw(homeWin, "====LAN-chat====\n");

  win = newwin(homeScreenOptions.size(), COLS, y + 1, 0);
  
  refresh();
  wrefresh(homeWin);
  int oldNotifications = 0;
  int ch;
  bool update = false;
  bool nothingPressed = true;
  int rows = homeScreenOptions.size();
  int cols = COLS;
  int userChoice = -1;
  bool updateAll = true;
  int current = 0;
  auto hso = homeScreenOptions;
  while (1) {
    notificationMutex.lock();
    if (totalNotifications > 0 && oldNotifications != totalNotifications) {
      hso = homeScreenOptions;
      hso[1] += " {";
      hso[1] += std::to_string(totalNotifications);
      hso[1] += '}';
      oldNotifications = totalNotifications;
      updateAll = true;
    }
    notificationMutex.unlock();

    noecho();
    halfdelay(10);

    keypad(win, true);
    curs_set(0); // Sets cursor to be invisible
    if (updateAll) {
      wclear(win);
      for (int i = 0; i < rows; ++i) {
        if (i >= hso.size()) {
          wprintw(win, "%s\n", format("", cols - 1, '<').c_str());
          continue;
        }
        if (i == current) {
          wprintw(win, "[*] %s\n", hso[i].c_str());
        } else
          wprintw(win, "[ ] %s\n", hso[i].c_str());
      }
      wrefresh(win);
    }
    update = false;
    int arrayBegin = 0;

    ch = wgetch(win);
    
    update = true;
    switch (ch) {
      case 'k':
      case KEY_UP:
        if (current == 0)
          current = hso.size() - 1;
        else
          current--;
        break;
      case 'j':
      case KEY_DOWN:
        if (current == hso.size() - 1)
          current = 0;
        else
          current++;
        break;
      case 10: // Enter key
        wclear(win);
        wrefresh(win);
        userChoice = current;
        nothingPressed = false;
        break;
      case 27: // Escape
        wclear(win);
        wrefresh(win);
        keypad(win, false);
        userChoice = -1;
        nothingPressed = false;
        break;
      case ERR:
        nothingPressed = true;
        break;
      default:
        update = false;
    }

    if (ch == '|')
      pushError("Test error screen", -1);
    if (checkError() != 0) {
      while (checkError() != 0) {}
      update = true;
    }
    
    if (!nothingPressed)
      break;

    if (update) {
      if (arrayBegin != current / rows) { // It does not fit into single window, scroll down
        arrayBegin = (current / rows) * rows; // should work, lazy to prove why
        for (int i = 0; i < rows; ++i) {
          if (i + arrayBegin < hso.size())
            mvwprintw(win, i, 0, "[ ] %s", format(hso[i + arrayBegin], cols, '<').c_str());
          else
            mvwprintw(win, i, 0, "%s", format("", cols, '^').c_str());
        }
      }
      for (int i = 0; i < rows; ++i) {
        mvwprintw(win, i, 1, " ");
        if (i == (current % rows))
          mvwprintw(win, i, 1, "*");
      }
      wrefresh(win);
    }
  }
  switch (userChoice) {
    case -1:
      return NO_OPTION;
    case 0: // New chat
      return NEW_CHAT;
    case 1: // Chats
      return CHATS;
    case 2: // Change nickname
      return CHANGE_NICKNAME;
    case 3: // Exit
      return EXIT;
    default: // How tf u managed to corrupt memory so bad to get new index?
      return NO_OPTION;
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
  wprintw(win, "        IP         |    nickname\n");
  
  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  halfdelay(1);

  discoveredPeersMutex.lock();
  int lastSize = discoveredPeers.size();


  int current = 0;
  for (int i = 0; i < discoveredPeers.size(); ++i) {
    // Not showing IP with which the connection is already established
    connectedPeersMutex.lock();
    if (std::find_if(connectedPeers.begin(), connectedPeers.end(), [i](std::shared_ptr<Peer> p) {return p->IP == discoveredPeers[i].IP;}) != connectedPeers.end()) {
        connectedPeersMutex.unlock();
        continue;
    }
    connectedPeersMutex.unlock();
    if (i == current) {
      wprintw(optionWin, "[*] %s|%s|%s\n", format(discoveredPeers[i].IP, 3 * 4 + 3, '<').c_str(), discoveredPeers[i].nickname.c_str(), std::to_string(discoveredPeers[i].portNum).c_str());
    } else
      wprintw(optionWin, "[ ] %s|%s\n", format(discoveredPeers[i].IP, 3 * 4 + 3, '<').c_str(), discoveredPeers[i].nickname.c_str());
  }
  discoveredPeersMutex.unlock();
  wrefresh(win);
  int ch;
  bool update = false;
  while (1) {
    ch = wgetch(optionWin);
    if (ch == 27) // Escape
      return "";
    if (ch == 10) {
      discoveredPeersMutex.lock();
      std::string out = discoveredPeers.size() > 0 ? discoveredPeers[current].IP : "";
      discoveredPeersMutex.unlock();

      wclear(optionWin);
      wrefresh(optionWin);
      keypad(win, false);
      return out;
    }
    update = true;
    switch (ch) {
      case 'k':
      case KEY_UP:
        if (current == 0) {
          discoveredPeersMutex.lock();
          current = discoveredPeers.size() > 0 ? discoveredPeers.size() - 1 : 0;
          discoveredPeersMutex.unlock();
        }
        else
          current--;
        break;
      case 'j':
      case KEY_DOWN:
        discoveredPeersMutex.lock();
        if (discoveredPeers.size() < 1)
          current = 0;
        if (current == discoveredPeers.size() - 1)
          current = 0;
        else
          current++;
        discoveredPeersMutex.unlock();
        break;
      default:
        update = false;
    }
    if (checkError() != 0) {
      while (checkError() != 0) {}
      update = true;
    }
    discoveredPeersMutex.lock();
    if (update) {
      for (int i = 0; i < discoveredPeers.size(); ++i) {
        mvwprintw(optionWin, i, 1, " ");
        if (i == current)
          mvwprintw(optionWin, i, 1, "*");
      }
      wrefresh(optionWin);
    }
    if (discoveredPeers.size() != lastSize) {
      if (lastSize > discoveredPeers.size()) // If list got smaller and selected was last option, place selected at the new last option
        current = discoveredPeers.size() - 1;
      lastSize = discoveredPeers.size();
      wclear(optionWin);
      wmove(optionWin, 0, 0);
      
      for (int i = 0; i < discoveredPeers.size(); ++i) {
        if (i == current) {
          wprintw(optionWin, "[*] %s|%s\n", discoveredPeers[i].IP.c_str(), discoveredPeers[i].nickname.c_str());
        } else
          wprintw(optionWin, "[ ] %s|%s\n", discoveredPeers[i].IP.c_str(), discoveredPeers[i].nickname.c_str());
      }
      wrefresh(optionWin);
    }
    discoveredPeersMutex.unlock();
  }
}

bool displayConfirmWin(std::string question) {
  WINDOW* win;
  int height = 8, width = COLS / 3;
  int startX, startY;
  startY = (ROWS - height) / 2;
  startX = (COLS - width) / 2;
  win = newwin(height, width, startY, startX);
  box(win, 0, 0);

  mvwprintw(win, 1, 1, "%s", format(question, width - 2, '^').c_str());

  WINDOW* yes;
  WINDOW* no;
  yes = newwin(3, 5, startY + height - 4, startX + 1);
  no = newwin(3, 4, startY + height - 4, startX + width - 5);

  box(yes, 0, 0);
  box(no, 0, 0);

  wattron(yes, A_REVERSE);
  mvwprintw(yes, 1, 1, "Yes");
  wattroff(yes, A_REVERSE);
  mvwprintw(no, 1, 1, "No");

  wrefresh(win);
  wrefresh(yes);
  wrefresh(no);
  //refresh();

  int ch;
  halfdelay(1);
  keypad(win, true);
  
  bool current = true; // For yes option

  
  while (1) {
    ch = wgetch(win);

    switch (ch) {
      case 'l':
      case 'h':
      case KEY_RIGHT:
      case KEY_LEFT:
        if (current) {
          mvwprintw(yes, 1, 1, "Yes");
          wattron(no, A_REVERSE);
          mvwprintw(no, 1, 1, "No");
          wattroff(no, A_REVERSE);
        } else {
          mvwprintw(no, 1, 1, "No");
          wattron(yes, A_REVERSE);
          mvwprintw(yes, 1, 1, "Yes");
          wattroff(yes, A_REVERSE);
        }
        wrefresh(no);
        wrefresh(yes);

        current = !current;
        break;
      case '\n':
        keypad(win, false);
        wclear(win);
        wrefresh(win);
        return current;
    }
  }
}

std::string displayChangeNicknameScreen(std::string curNick) {
  clear();
  WINDOW* textWin;
  WINDOW* nickWin;

  // Center nickname window
  int length = MAX_NICKNAME_LENGTH;
  int avgX, avgY, startX, startY;
  avgX = COLS / 2;
  
  startX = avgX - (length + 1) / 2; // 1 for border
  
  nickWin = newwin(3, length + 2, 1, startX);
  box(nickWin, 0, 0);
  
  textWin = newwin(1, length + 2, 0, startX);
  wprintw(textWin, "Your nickname\n");

  // Putting current nickname into input window
  mvwprintw(nickWin, 1, 1, "%s", curNick.c_str());

  refresh();
  wrefresh(nickWin);
  wrefresh(textWin);

  halfdelay(1);
  int ch;
  while (1) {
    if (checkError() != 0) {
      while (checkError() != 0) {}
      box(nickWin, 0, 0);
      wprintw(textWin, "Your nickname\n");
      mvwprintw(nickWin, 1, 1, "%s", curNick.c_str());
    }

    ch = wgetch(nickWin);
    
    if (ch == ERR)
      continue;

    // 127 is delete
    if (ch >= ' ' && ch <= 127) {
      int x, y;
      getyx(nickWin, y, x);
      if (ch != 127 && x < length) {
        curNick.append(reinterpret_cast<char*>(&ch));
        waddch(nickWin, ch);
        wrefresh(nickWin);
      } else {
        if (x == 1)
          continue;
        curNick.erase(curNick.size() - 1, 1); // Erase last character from buffer
        wmove(nickWin, y, x - 1);
        waddch(nickWin, ' ');
        wmove(nickWin, y, x - 1);
        wrefresh(nickWin);
      }
    }

    if (ch == 10)
      return curNick;
    if (ch == 27) { // Escape
      bool responce = displayConfirmWin("Do you want to save changes?");
      if (responce)
        return curNick;
      else
        return "~|NO_change|~";
    }
  }
}

std::string displayChatsScreen(std::vector<std::shared_ptr<Peer>>& connectedPeers) {
  clear();
  std::vector<std::string> options;
  std::vector<std::string> tempOptions;
  
  std::string entry;

  WINDOW* win;
  WINDOW* selectorWin;

  win = newwin(ROWS, COLS, 0, 0);
  /* Displaying the header of sreen */
  wprintw(win, "Select a chat to enter or delete with backspace\n");
  wprintw(win, "nickname   | IP\n");
  wrefresh(win);
  int rows = ROWS - 2;
  int cols = COLS;
  selectorWin = newwin(rows, cols, 2, 0);

  int ch;
  bool update = false;
  int arrayBegin = 0;
  bool updateWhole = true;
  int current = 0;
  
  noecho();
  halfdelay(10); // Check for update every second

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  while (1) {
    tempOptions.clear();


    connectedPeersMutex.lock();
    /* Calculating if there were any changes of options */
    for (int i = 0; i < connectedPeers.size(); ++i) {
      entry.clear();
      entry += format(connectedPeers[i]->IP, 3 * 4 + 3, '<');
      entry += '|';
      entry += connectedPeers[i]->nickname;
      notificationMutex.lock();
      /* Showing the notification count */
      if (notifications[connectedPeers[i]->IP] > 0) {
        entry += " {";
        entry += std::to_string(notifications[connectedPeers[i]->IP]);
        entry += "}";
      }
      notificationMutex.unlock();
      tempOptions.push_back(entry);
    }
    connectedPeersMutex.unlock();

    // Check if new options are different
    if (options.size() != tempOptions.size()) {
      updateWhole = true;
      options = tempOptions;
    }
    else {
      for (int i = 0; i < options.size(); ++i) {
        if (options[i] != tempOptions[i]) {
          updateWhole = true;
          options = tempOptions;
          break;
        }
      }
    }
    if (checkError() != 0) {
      while (checkError() != 0) {}
      updateWhole = true;
    }
    if (updateWhole) {
      if (options.size() == 0) {
        wclear(selectorWin);
        mvwprintw(selectorWin, 0, 0, "No Peers Connected\n");
        wrefresh(selectorWin);
        updateWhole = false;
        continue;
      }
      /* Drawing the all the options onto the screen */
      for (int i = 0; i < rows; ++i) {
        if (i >= options.size()) {
          mvwprintw(selectorWin, i, 0, "%s\n", format("", cols - 1, '<').c_str());
          continue;
        }
        if (i == current) {
          mvwprintw(selectorWin, i, 0, "[*] %s\n", options[i].c_str());
        } else {
          mvwprintw(selectorWin, i, 0, "[ ] %s\n", options[i].c_str());
        }
      }
      wrefresh(selectorWin);
      updateWhole = false;
    }

    ch = wgetch(selectorWin);
    bool skip = false;
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
      case 10: { // Enter key
        wclear(selectorWin);
        wrefresh(selectorWin);
        keypad(win, false);
        connectedPeersMutex.lock();
        std::string returnIP = connectedPeers[current]->IP; 
        connectedPeersMutex.unlock();
        return returnIP;
      }
      case 27: // Escape
        wclear(selectorWin);
        wrefresh(selectorWin);
        keypad(win, false);
        return "";
      /* Disconnect from the selected peer by closing socket file descriptor */
      case KEY_BACKSPACE:
      case 127: {
        bool responce = displayConfirmWin("Close selected peer connection?");
        if (responce) {
          displayError(std::to_string(current) + std::to_string(connectedPeers.size()));
          closePeerConnection(connectedPeers[current]->IP);
          skip = true;
        }
        break;
      }

      default:
        update = false;
        break;
    }

    if (skip) {
      skip = false;
      continue;
    }

    if (update) {
      if (arrayBegin != current / rows) { // It does not fit into single window, scroll down
        arrayBegin = (current / rows) * rows; // should work, lazy to prove why
        for (int i = 0; i < rows; ++i) {
          if (i + arrayBegin < options.size()) {
            mvwprintw(selectorWin, i, 0, "[ ] %s", format(options[i + arrayBegin], cols, '<').c_str());
          }
          else
            mvwprintw(selectorWin, i, 0, "%s", format("", cols, '^').c_str());
        }
      }
      for (int i = 0; i < rows; ++i) {
        mvwprintw(selectorWin, i, 1, " ");
        if (i == (current % rows))
          mvwprintw(selectorWin, i, 1, "*");
      }
      wrefresh(selectorWin);
    }
  }
}
