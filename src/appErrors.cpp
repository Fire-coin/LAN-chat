#include "appErrors.hpp"


std::queue<LCError> errorQueue;
std::mutex errorMutex;


void pushError(std::string message, int errorCode) {
  errorMutex.lock();
  LCError err{};
  err.message = message;
  err.errorCode = errorCode;

  errorQueue.push(err);
  errorMutex.unlock();
}
