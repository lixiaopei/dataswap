#ifndef SHARECODE_UNIT_MEM_CACHE_H
#define SHARECODE_UNIT_MEM_CACHE_H
#include "sync.h"
#include "unit_heap.h"

class UnitMemCache {
public:
  UnitMemCache(unsigned int per_mem_size = 128 * 1024);
  ~UnitMemCache();
  int FreeUnit(char *aMem);
  char *MallocUnit(unsigned int mem_limit = 0);
  inline void SetUnitMemSize(unsigned int per_mem_size) {
    per_mem_size_ = per_mem_size;
  }
  inline unsigned int GetUnitMemSize() { return per_mem_size_; }

protected:
  UnitHeap *CreateHeap();

protected:
  CMutex locker_;
  unsigned int per_mem_size_;
  std::list<UnitHeap *> heap_list_;
  UnitHeap *last_heap_;
};
#endif
