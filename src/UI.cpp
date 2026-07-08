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

/* Checks error queue for any errors. If there are errors present, it calls displatError
 * to display it. */
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

/* Creates an error window which is put into a box. It displays header that 
 * this is error window. Except for that it aso shows provided error message.
 * The mesage is qrapped if it does not fit into single line. */
void displayError(std::string error) {
  WINDOW* win;
  curs_set(0);
  halfdelay(5);
  // Center the window
  int startY = ROWS / 4;
  int startX = COLS / 4;
  int availableSize = COLS / 2 - 2;
  win = newwin(ROWS / 2, COLS / 2, startY, startX);
  box(win, 0, 0);
  mvwprintw(win, 1, 1, "%s", format("====LAN-chat Error====", COLS / 2 - 2, '^').c_str());
  int offset = 0;
  int i = 0;

  while (i < ROWS / 2 - 2 && offset < error.size()) {
    std::string msgRow = error.substr(i * availableSize, availableSize);
    mvwprintw(win, i + 2, 1, "%s", format(msgRow, availableSize, '^').c_str());
    i++;
    offset += availableSize;
  }

  mvwprintw(win, ROWS / 2 - 2, 1, "%s", format("Press Escape to close this window", COLS / 2 - 2, '^').c_str());
  wrefresh(win);
  int ch = ERR;
  while (ch != 27) {
    ch = wgetch(win);
  }
  wclear(win);
  wrefresh(win);
}

/* A thread safe way to add a message into chat history of given IP. 
 * creator means either sender or reciever on who added the message.
 * cretator=0 means that sender added (you wrote the message), and creator=1
 * means that message was recieved*/
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
/* Calls initsrc() and gets dimensions of the screen. */
void beginUI() {
  initscr();
  noecho();
  halfdelay(1);
  getmaxyx(stdscr, ROWS, COLS);
}

/* Just read the name and statement in it. */
void endUI() {
  endwin();
}
/* Displays the chat history with given peer, and it starts displaying it from scrollIndex.
 * It requires already premade window. */
// Inspired from https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html#INIT 
void displayChatLog(WINDOW* win, std::string IP, uint32_t scrollIndex) {
  /* y is used to track on which row to display next message */
  int x, y = 0;
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
  //TODO fix the scroll index such that it increments when messages are sent beyond the visible borders
  if (scrollIndex > curChat.size() - 1)
    scrollIndex = curChat.size() - 1;
  /* Displaying the chat history, with messages telling who send what */
  werase(win);
  for (auto it = curChat.begin() + scrollIndex; it != curChat.end(); ++it) {
    getyx(win, y, x);
    /* Message sent */
    if (it->creator == 0) { 
      if (!it->isFile) 
        mvwprintw(win, y + 1, 0, "Message sent: %s\n", it->data.c_str());
      else
        mvwprintw(win, y + 1, 0, "File sent: %s\n", it->data.c_str());
    } else { /* Message recieved */
      if (!it->isFile)
        mvwprintw(win, y + 1, 0, "Message recieved: %s\n", it->data.c_str());
      else
        mvwprintw(win, y + 1, 0, "File recieved: %s\n", it->data.c_str());
    }
  }
  wrefresh(win); // To display it into real terminal

  // TODO add color to distinguish between peers
  /* Clearing the notifications for this chat */
  notificationMutex.lock();
  totalNotifications -= notifications[IP];
  notifications[IP] = 0;
  notificationMutex.unlock();

  chatHistoryMutex.unlock();
}

void displayChatScreen(std::string IP) {
  int ch;
  uint32_t scrollIndex = 0;

  WINDOW* chatWin;
  WINDOW* inputWin;

  chatWin = newwin(ROWS - 3, COLS, 0, 0);
  inputWin = newwin(3, COLS, ROWS - 3, 0);

  keypad(inputWin, true);
  halfdelay(1); // Check for request every 100 milliseconds that user does not type
  noecho();

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
    /* Updating chat log (new message added or chat was scrolled */
    if (updateScreen) {
      displayChatLog(chatWin, IP, scrollIndex);
      updateScreen = false;
    }
    
    ch = wgetch(inputWin);
    /* Nothing was pressed */
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
        displayChatScreen(IP);
        break;
      }
      wrefresh(inputWin);
      /*XXX Extremely ugly fix because I am unable to locate the issue*/
      /* XXX The issue is that when file selector is displayed and destroyed, no new messages
       * render from other peer - displayChatLog function is not called at all, despite addMsg
       * working properly.
       * This fix is more memory inneficient, but if you take into account that most people will send less than 10 files in one go without switching chat, I believe it is good enough.*/
    }
    
    // Enter has been pressed
    if (ch == 10) {
      sendMessage(IP, inputBuffer);
      /* This way clearing the line is good, because if somehow error happens, the dividing
       * line will still be drawn*/
      werase(inputWin);
      mvwprintw(inputWin, 0, 0, "%s", separator);
      wrefresh(inputWin);
    }
    /* Scroll one message up */
    if (ch == KEY_UP) {
      if (scrollIndex > 0) {
        scrollIndex--;
        updateScreen = true;
      }
    }
    /* Scroll one message down */
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
  }
  
  delete[] separator;
  endwin();
}

/* Displays option menu and returns the index of selected option 
 * Currently not used anymore, but I will leave it here for possible future implementations
 * */
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
        werase(win);
        wrefresh(win);
        return current;
      case 27: // Escape
        werase(win);
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
  int ch;
  bool displaySelector = true;
  int userChoice = -1;
  int current = 0;
  auto hso = homeScreenOptions;
  
  WINDOW* homeWin;
  WINDOW* win;

  clear();
  homeWin = newwin(9, COLS, 0, 0);
  win = newwin(homeScreenOptions.size(), COLS, 9, 0);
  // TODO make the newChatScreen not accept anything when the displayed things are empty
  /* Setting some parameters for selection window */
  noecho();
  halfdelay(10);
  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  
  /* Displaying logo */             
  mvwprintw(homeWin, 0, 0, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n", 
      R"( __         ______   __    __                  __                    __     )",
      R"(/  |       /      \ /  \  /  |                /  |                  /  |    )",
      R"(## |      /######  |##  \ ## |        _______ ## |____    ______   _## |_   )",
      R"(## |      ## |__## |###  \## |       /       |##      \  /      \ / ##   |  )",
      R"(## |      ##    ## |####  ## |      /#######/ #######  | ######  |######/   )",
      R"(## |      ######## |## ## ## |      ## |      ## |  ## | /    ## |  ## | __ )",
      R"(## |_____ ## |  ## |## |#### |      ## \_____ ## |  ## |/####### |  ## |/  |)",
      R"(##       |## |  ## |## | ### |      ##       |## |  ## |##    ## |  ##  ##/ )",
      R"(########/ ##/   ##/ ##/   ##/        #######/ ##/   ##/  #######/    ####/  )"
  );
  wrefresh(homeWin);
               
  while (displaySelector) {
    hso = homeScreenOptions;
      
    /* If there are any notifications, add them after the Chats field */
    notificationMutex.lock();
    if (totalNotifications > 0) {
      hso[1] += " {";
      hso[1] += std::to_string(totalNotifications);
      hso[1] += '}';
    }
    notificationMutex.unlock();

    
    /* Redrawing all the options from selector */
    werase(win);
    for (int i = 0; i < homeScreenOptions.size(); ++i) {
      if (i >= hso.size()) {
        wprintw(win, "%s\n", format("", COLS - 1, '<').c_str());
        continue;
      }
      if (i == current) {
        wprintw(win, "[*] %s\n", hso[i].c_str());
      } else
        wprintw(win, "[ ] %s\n", hso[i].c_str());
    }
    wrefresh(win);
    
    /* Taking key pressed and performing an action from it */
    ch = wgetch(win);
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
        userChoice = current;
        displaySelector = false;
        break;
      case 27: // Escape
        userChoice = -1;
        displaySelector = false;
        break;
      case ERR:
        break;
      default:
        break;
    }
  }

  /* Returning the window back to original state */
  werase(win);
  wrefresh(win);
  keypad(win, false);
  curs_set(1);

  /* Returning which option was selected */
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

/* Displays a screen, which shows discovered peers which are currently not connected. Returns IP of selected peer when enter is pressed. */
std::string displayNewChatScreen() {
  int ch;
  int current = 0;
  std::vector<Peer> displayedPeers;

  WINDOW* win;
  WINDOW* optionWin;

  clear();
  win = newwin(ROWS, COLS, 0, 0);
  optionWin = newwin(ROWS - 2, COLS, 2, 0);

  /* displaying the screen header */
  wprintw(win, "%s\n", format("==========SELECT PEER TO CONNECT==========", 4 + 4 * 3 + 3 + MAX_NICKNAME_LENGTH, '<').c_str());
  wprintw(win, "    %s|%s\n", format("IP", 4 * 3 + 3, '^').c_str(), format("nickname", MAX_NICKNAME_LENGTH, '^').c_str());
  
  wrefresh(win);

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  halfdelay(1);

  while (1) {
    displayedPeers.clear();
    discoveredPeersMutex.lock();
    /* Finding discovered Peers which are not connected, and available to be connected to */
    for (int i = 0; i < ROWS - 2; ++i) {
      /* If index is past the number of discovered peers, just print empty line */
      if (i >= discoveredPeers.size()) {
        wprintw(optionWin, "%s", format("", COLS, '<').c_str());
        continue;
      }

      // Not showing IP with which the connection is already established
      connectedPeersMutex.lock();
      auto it = std::find_if(connectedPeers.begin(), connectedPeers.end(), [i](std::shared_ptr<Peer> p) {return p->IP == discoveredPeers[i].IP;});
      if (it != connectedPeers.end()) {
          connectedPeersMutex.unlock();
          continue;
      }

      displayedPeers.push_back(discoveredPeers[i]);
      connectedPeersMutex.unlock();
    }
    discoveredPeersMutex.unlock();
    
    /* Redrawing whole selector and displaying which option is selected */
    werase(optionWin);
    for (int i = 0; i < displayedPeers.size(); ++i) {
      
      if (i == current) {
        mvwprintw(optionWin, i, 0, "[*] %s|%s\n", format(displayedPeers[i].IP, 3 * 4 + 3, '<').c_str(), displayedPeers[i].nickname.c_str());
      } else
        mvwprintw(optionWin, i, 0, "[ ] %s|%s\n", format(displayedPeers[i].IP, 3 * 4 + 3, '<').c_str(), displayedPeers[i].nickname.c_str());
    }
    wrefresh(optionWin);
    
    /* Check if there are any errors thrown */
    if (checkError() != 0) {
      while (checkError() != 0) {}
    }

    /* Get the pressed key and do some action depending on it */
    ch = wgetch(optionWin);
    if (ch == 27) // Escape
      return "";
    if (ch == 10) {
      discoveredPeersMutex.lock();
      std::string out = displayedPeers.size() > 0 ? displayedPeers[current].IP : "";
      discoveredPeersMutex.unlock();

      werase(optionWin);
      wrefresh(optionWin);
      keypad(win, false);
      return out;
    }

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
        break;
    }
  }
}

/* Classic style confirmation window. It displays gives question
 * and it has two options: Yes and No, user switches between them by moving right / left
 * with arrows or vim motions. It returns true / false depending on option selected. */
bool displayConfirmWin(std::string question) {
  int ch;
  int height = 8, width = COLS / 3;
  int startX, startY;
  bool current = true; // For yes option
  startY = (ROWS - height) / 2;
  startX = (COLS - width) / 2;

  WINDOW* win;

  win = newwin(height, width, startY, startX);
  box(win, 0, 0);

  /* Printing the question and wrapping it if needed */
  int offset = 0;
  int i = 0;

  while (i < ROWS / 2 - 2 && offset < question.size()) {
    std::string msgRow = question.substr(i * (width - 2), width - 2);
    mvwprintw(win, i + 1, 1, "%s", format(question, width - 2, '^').c_str());
    i++;
    offset += width - 2;
  }

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

  halfdelay(1);
  keypad(win, true);
  

  
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

/* Checks if nickname satisfies max length and if it does not contain | symbol */
bool checkNickname(std::string nick) {
  if (nick.size() > MAX_NICKNAME_LENGTH) {
    pushError("Nickname should not be longer than " + std::to_string(MAX_NICKNAME_LENGTH), -1);
    return false;
  }

  if (nick.find_first_of('|') != nick.npos) {
    pushError("Nickname cannot contain | character", -1);
    return false;
  }
  
  return true;
}

/* Displays screen, where user can change nickname. It can be either
 * changed without confirmation by pressing enter, or by pressing 
 * enter confirmation window will pop up asking whether to save changes.
 * Before changes are saved checkNickname is called. */
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
      mvwprintw(textWin, 0, 0, "Your nickname\n");
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

    if (ch == 10) {
      bool result = checkNickname(curNick);

      if (checkError() != 0) {
        while (checkError() != 0) {}
        box(nickWin, 0, 0);
        mvwprintw(textWin, 0, 0, "Your nickname\n");
        mvwprintw(nickWin, 1, 1, "%s", curNick.c_str());
        continue;
      }

      if (result)
        return curNick;
      else
        return "~|NO_change|~";
    }

    if (ch == 27) { // Escape
      bool responce = displayConfirmWin("Do you want to save changes?");
      if (!responce)
        return "~|NO_change|~";

      bool result = checkNickname(curNick);

      if (checkError() != 0) {
        while (checkError() != 0) {}
        box(nickWin, 0, 0);
        mvwprintw(textWin, 0, 0, "Your nickname\n");
        mvwprintw(nickWin, 1, 1, "%s", curNick.c_str());
        continue;
      }
      
      if (result)
        return curNick;
    }
  }
}


/* Displays screen where there are connected chats.
 * User can select chat and enter there using enter key,
 * or delete the chat, while preserving chat history for 
 * current session, using backspace. */
std::string displayChatsScreen(std::vector<std::shared_ptr<Peer>>& connectedPeers) {
  int ch;
  int current = 0;
  int rows = ROWS - 2;
  int cols = COLS;
  std::vector<Peer> options;
  std::string entry;
  

  WINDOW* win;
  WINDOW* selectorWin;

  clear();
  win = newwin(ROWS, COLS, 0, 0);
  /* Displaying the header of sreen */
  wprintw(win, "Select a chat to enter or delete with backspace\n");
  wprintw(win, "nickname   | IP\n");
  wrefresh(win);
  selectorWin = newwin(rows, cols, 2, 0);

  
  noecho();
  halfdelay(10); // Check for update every second

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  while (1) {
    options.clear();

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
      mvwprintw(selectorWin, i, 0, "[ ] %s", entry.c_str());
      options.push_back(*connectedPeers[i]);
    }
    connectedPeersMutex.unlock();


    if (checkError() != 0) {
      while (checkError() != 0) {}
    }
    /* Clearing unused lines and setting * next to the currently selected peer */
    for (int i = 0; i < rows; ++i) {
      if (i >= options.size()) {
        mvwprintw(selectorWin, i, 0, "%s\n", format("", cols - 1, '<').c_str());
        continue;
      }
      if (i == current)
        mvwprintw(selectorWin, i, 1, "*");
    }

    if (options.size() == 0)
      mvwprintw(selectorWin, 0, 0, "No Peer Connected\n");
    wrefresh(selectorWin);

    ch = wgetch(selectorWin);
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
        std::string returnIP = options.size() > 0 ? options[current].IP : ""; 
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
        if (responce)
          closePeerConnection(connectedPeers[current]->IP);
        break;
      }
      default:
        break;
    }
  }
}
