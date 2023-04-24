#ifndef SHARECODE_UNIT_HEAP_H
#define SHARECODE_UNIT_HEAP_H

enum UnitHeapStatus {
  UNIT_HEAP_NORMAL = 0,
  UNIT_HEAP_EMPTY = 1,
  UNIT_HEAP_FULL = 2
};

struct UnitHeapHeader {
  unsigned int flag_;
  UnitHeapHeader *next_ptr_;
  char data_[1];
};

template <class T_MAX_SPACE = 4096000> class UnitHeap {
public:
  UnitHeap(int aPerMemSize, int aMemCount);
  ~UnitHeap();

public:
  char *SetData(char *dt, char *len);
  bool IsContainData();
  UnitHeapStatus CheckSpace();
  char *MallocUnit();
  UnitHeap *FreeUnit(char *dt);

public:
  static UnitHeap *FreePureMem(char *dt);

protected:
  int per_mem_size_;
  int mem_count_;
  int usefull_count_;
  UnitHeapHeader *next_heap_header_;
  char *p_start_data_;
};
#endif
