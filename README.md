# LAN-chat
A simple P2P CLI tool that enables chatting and file sending across LAN network with TUI.

## Functionality
## Intuitive selector which can be navigated with Vim motions and arrow keys
<img width="804" height="340" alt="Screenshot from 2026-06-30 15-59-54" src="https://github.com/user-attachments/assets/a33634ec-b6fc-4c77-9790-dddabf884b5e" />


### Network Scanning
<img width="486" height="201" alt="Screenshot from 2026-07-10 11-31-48" src="https://github.com/user-attachments/assets/764cd5b2-acb8-414b-ac32-41a788e40b4b" />


Automatical discovery of other peers on the same local network.
### Send Messages and Files
LAN-chat enables to transfer text messages and files.

Messages are sent by typing them into input window and then pressing enter:
<img width="1910" height="1038" alt="Screenshot from 2026-07-10 11-39-31" src="https://github.com/user-attachments/assets/3324d29b-c784-4014-b25f-5eb6b6b34fbb" />

#### Files
To send a file, user shall type :file and then a file selector window will pop up. Again navigating using Vim motions or arrow keys.

<img width="1911" height="1041" alt="Screenshot from 2026-07-10 11-41-23" src="https://github.com/user-attachments/assets/10634b1b-c9f4-49d1-bc52-b7758bceb277" />


Limitation is that either file, or message can be sent at a time. Not both.

### Select chat to talk to
When selecting option Chats in home screen, following window with all connected chats will pop up:

<img width="446" height="76" alt="Screenshot from 2026-07-10 11-47-04" src="https://github.com/user-attachments/assets/41f6fbea-b73e-4f97-807c-d98e160d1ad9" />


### Set custom nickname
One is be able to set custom nickname so it is easier to recognise devices on network.

When selecting Change Nickname option, following screen will show up:
<img width="1904" height="122" alt="Screenshot from 2026-07-10 11-49-13" src="https://github.com/user-attachments/assets/bfefd2c9-bc6b-42b9-8df4-f183d68e4c53" />

Initially it is empty, but one can type nickname there.

By pressing Esc, a pop up will show asking for confirmation, again selecting option with Vim motions or arrows:
<img width="639" height="155" alt="Screenshot from 2026-07-10 11-49-28" src="https://github.com/user-attachments/assets/dfc28f6e-b5c1-4762-8c3c-886e7cc01344" />


### Notifications
LAN-chat shows notifications for unread messages sent from other peers.
Notifications are both shown in home screen, and in Chats screen:
<img width="285" height="91" alt="Screenshot from 2026-07-10 11-53-04" src="https://github.com/user-attachments/assets/829125cd-bbd3-453b-88d4-fedab2a69410" />

<img width="462" height="82" alt="Screenshot from 2026-07-10 11-53-15" src="https://github.com/user-attachments/assets/46671373-7466-4906-8643-1272d8aea762" />

### Session preserved chat history
If you or other peer close connection and then reconnect back, the chat history will be preserved.

The all chat histories are deleted for the peer who exits the app with Exit or any other way. 

### Scrolling
You can also scroll through messages using arrow keys <b>only</b>. Unfortunately to use Vim motions I would need to implement Vim modes, but it is too complex for this project. 

### TUI interface
A minimal TUI interface made with ncurses. Which should be able to run on almost any Linux device

# How to use?
1. Select New Chat option with Enter in home screen
2. Select peer to which you want to connect by pressing Enter
3. The chat screen shows up, now type any messages you want, or select a file by typing <b>:file</b> into input window

## Additional features
* Exit current chat with by pressing Esc, it will move you back to hoome screen, while not closing connection with peer
* In chats screen, you are able to close connection with peer by pressing Backspace, the chat history will be preserved.
* If any error pops up, close error window with Esc.


# How do I get this masterpiece running on my machine?
## Prerequirements
* Linux environment, or anything which will support ncurses and POSIX socket API.
* Ncurses
* C++ 17 or higher my personal choice is g++ from gcc.
* Cmake
* Have the guts to build something from source.
## Downloading prerequirements, code snippets are for Arch Linux, but use package manager of your distro

````bash
# Getting nccurses (if cmake complains)
sudo pacman -S ncurses

# Getting g++ with gcc package
sudo pacman -S gcc

# Getting cmake
sudo -S cmake
````

## Installing the actuall stuff

````bash
# Clone the github repo
git clone https://github.com/Fire-coin/LAN-chat
cd LAN-chat

# Making build directory, it can be anywhere and any other name
mkdir build

# Doing cmake magic
cmake -B build
cmake --build build

# Run the compiled file, it can be moved anyhere or you can also create a symbolic link for it
./build/LAN-chat
````

If there are any errors thrown by cmake, it means that you most probably did not install something from the prerequirements. Either way consult with cmake manual: https://cmake.org/

# Resources
## Sockets on linux
https://www.linuxhowtos.org/C_C++/socket.htm

https://man7.org/linux/man-pages/man2/socket.2.html

[https://github.com/Johannes4Linux/linux_socket_examples/tree/main?tab=Unlicense-1-ov-file](https://github.com/Johannes4Linux/linux_socket_examples)

## Other sources from where I took inspiration or copied code should be indicated in comments in code.

# What did I learn from this project and what can you learn making similar one?
## POSIX sockets
Basics of working with sockets, primararly streaming TCP sockets and datagram UDP sockets.
## Network stuff
Turns out there are private broadcast addresses, and different routers can have different structures of them.
## Working with man pages
I read through a quite a lot of man pages when searching how to work with sockets. They quite informative, but only when you already understand the terminology.
