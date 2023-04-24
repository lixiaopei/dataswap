#ifndef SHARECODE_SYNC_H
#define SHARECODE_SYNC_H
#include <syncapi.h>

class CMutex {
  HANDLE handle_;

public:
  CMutex();
  virtual ~CMutex();
  void lock();
  void unlock();
  void destroy();
};

class CSemaphore {
  HANDLE handle_;
  int mc_count_;

public:
  CSemaphore(int maxCount = 1, int initCount = 0);
  void destroy();
  virtual ~CSemaphore();
  void Signal(int n = 1);
  bool WaitMs(DWORD dwMilliseconds = INFINITE);
  bool Wait(DWORD second);
  bool PostSem();
  bool WaitSem(DWORD dwMilliseconds = 1000);
};
#endif