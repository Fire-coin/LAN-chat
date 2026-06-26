#ifndef APP_ERRORS_HPP
#define APP_ERRORS_HPP

#include <string>
#include <mutex>
#include <queue>
// LCE - LAN chat error
constexpr int LCE_CONNECTION_SOCK = -1;
constexpr int LCE_SEND = -2;
constexpr int LCE_EPOLL_CTL = -3;
constexpr int LCE_FILE_OP = -4; // file operation
constexpr int LCE_FCNTL = -5;
constexpr int LCE_BIND = -6;
constexpr int LCE_EPOLL_WAIT = -7;
constexpr int LCE_ACCEPT = -8;
constexpr int LCE_IMPOSSIBLE = -9; // smth that should not be possible to happen
constexpr int LCE_PEER_OFFLINE = -10;
constexpr int LCE_ALREADY_REPORTED = -11; // When the function reports error with pushError, but it needs to return a value, so function calling it, knows that smth is wrong
constexpr int LCE_BAD_PACKET = -12;
constexpr int LCE_CONNECT = -13;
constexpr int LCE_FD_CLOSED = -14;
constexpr int LCE_RECIEVE = -15;
constexpr int LCE_NOT_FULL_MSG = -16;
constexpr int LCE_EPOLL_CREATE = -17;
constexpr int LCE_SYS_CALL= -18;

struct LCError {
  std::string message;
  int errorCode;
};

extern std::queue<LCError> errorQueue;
extern std::mutex errorMutex;
void pushError(std::string, int errorCode);
#endif
