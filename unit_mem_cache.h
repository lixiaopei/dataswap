#ifndef SHARECODE_UNIT_MEM_CACHE_H
#define SHARECODE_UNIT_MEM_CACHE_H
#include "sync.h"
#include "unit_heap.h"

class UnitMemCache {
public:
  UnitMemCache(uint32_t aSize);
  ~UnitMemCache();
  int FreeUnit(char *aMem);
  char *MallocUnit();
  inline uint32_t GetUnitMemSize() { return per_mem_size_; }

protected:
  UnitHeap *createHeap();

protected:
  CMutex locker_;
  uint32_t per_mem_size_;
  std::list<UnitHeap *> heap_list_;
  UnitHeap *last_heap_;
};
#endif
