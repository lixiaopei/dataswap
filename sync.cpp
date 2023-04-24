#include "sync.h"

CMutex::CMutex() { handle_ = ::CreateMutexA(NULL, 0, NULL); }
CMutex::~CMutex() { this->destroy(); }
void CMutex::lock() { ::WaitForSingleObject(handle_, INFINITE); }
void CMutex::unlock() { ::ReleaseMutex(handle_); }
void CMutex::destroy() {
  if (handle_) {
    ::CloseHandle(handle_);
    handle_ = NULL;
  }
}

CSemaphore::CSemaphore(int maxCount = 1, int initCount = 0) {
  handle_ = ::CreateSemaphoreA(NULL, initCount, maxCount, NULL);
  mc_count_ = 0;
}
void CSemaphore::destroy() {
  if (handle_) {
    ::CloseHandle(handle_);
    handle_ = NULL;
  }
}
CSemaphore::~CSemaphore() { this->destroy(); }
void CSemaphore::Signal(int n) { ::ReleaseSemaphore(handle_, n, NULL); }
bool CSemaphore::WaitMs(DWORD dwMilliseconds = INFINITE) {
  return (::WaitForSingleObject(handle_, dwMilliseconds) == WAIT_OBJECT_0);
}
bool CSemaphore::Wait(DWORD second) {
  return (::WaitForSingleObject(handle_, second * 1000) == WAIT_OBJECT_0);
}
bool CSemaphore::PostSem() // postActionSem
{
  bool bRealRet = false;
  if (mc_count_ < 2) {
    Signal(1);
    ++mc_count_;
    bRealRet = true;
  }
  return bRealRet;
}
bool CSemaphore::WaitSem(DWORD dwMilliseconds = 1000) // WaitActionSem
{
  bool ret = WaitMs(dwMilliseconds);
  if (mc_count_ > 0) {
    --mc_count_;
  }
  return ret;
}