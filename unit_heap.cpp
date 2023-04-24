#include "unit_heap.h"

#define D_MEM_HEAP_FLAG 0x83062521
UnitHeap::UnitHeap(int32_t a_per_mem_size, int32_t a_mem_count) {
  const int s_max_size = T_MAX_SPACE;
  int32_t s_total_mem_size = s_max_size;

  UnitHeapHeader *p_current_header = NULL;
  UnitHeapHeader *p_previous_header = NULL;

  per_mem_size_ = a_per_mem_size;
  per_mem_size_ = (per_mem_size_ + 8) & 0xFFFFFFF8;
  if (per_mem_size_ * a_mem_count >= s_max_size) {
    mem_count_ = a_mem_count;
    s_total_mem_size = mem_count_ * per_mem_size_;
  } else {
    mem_count_ = s_max_size / per_mem_size_;
    s_total_mem_size = s_max_size;
  }
  if (mem_count_ < 10) {
    mem_count_ = 10;
    s_total_mem_size = per_mem_size_ * mem_count_;
  }
  next_heap_header_ = NULL;
  usefull_count_ = 0;
  p_start_data_ = (char *)malloc(s_total_mem_size);
  if (p_start_data_) {
    for (int j = 0; j < mem_count_; ++j) {
      p_current_header = (UnitHeapHeader *)(p_start_data_ + j * per_mem_size_);
      if (next_heap_header_) {
        p_previous_header->next_ptr_ = p_current_header;
      } else {
        next_heap_header_ = p_current_header;
      }
      p_previous_header = p_current_header;
      p_current_header->next_ptr_ = NULL;
      p_current_header->flag = D_MEM_HEAP_FLAG;
    }
    usefull_count_ = mem_count_;
  }
}

UnitHeap::~UnitHeap() {
  if (p_start_data_) {
    free(p_start_data_);
    p_start_data_ = 0;
  }
}

int UnitHeap::CheckSpace() {
  if (mem_count_ == usefull_count_) {
    return UNIT_HEAP_EMPTY;
  } else {
    if (usefull_count_)
      return UNIT_HEAP_NORMAL;
    else
      return UNIT_HEAP_FULL;
  }
}

char *UnitHeap::SetData(char *sbegin, char *send) {
  p_start_data_ = sbegin;
  next_heap_header_ = (UnitHeapHeader *)send;
  return (char *)next_heap_header_;
}

char *UnitHeap::MallocUnit() {
  char *pUnit = 0;
  if (usefull_count_) {
    UnitHeapHeader *sOutChunk = next_heap_header_;
    next_heap_header_ = sOutChunk->next_ptr_;
    sOutChunk->next_ptr_ = (UnitHeapHeader *)this;
    usefull_count_--;
    pUnit = sOutChunk->data;
  } else {
    // it`s full
  }
  return pUnit;
}

UnitHeap *UnitHeap::FreePureMem(char *p_data) {
  UnitHeap *sHeap =
      *((UnitHeap **)(p_data - (int)offsetof(struct UnitHeapHeader, data_) +
                      sizeof(unsigned int)));
  return sHeap->FreeUnit(p_data);
}

UnitHeap *UnitHeap::FreeUnit(char *p_data) {
  if (p_data >= p_start_data_ &&
      p_data < p_start_data_ + (mem_count_ * per_mem_size_)) {
    UnitHeapHeader *pt =
        (UnitHeapHeader *)(p_data -
                           (int)offsetof(struct UnitHeapHeader, data_));
    if (*(unsigned int *)pt != D_MEM_HEAP_FLAG) {
      // memory leak or overflow, fatal error, just exit
      exit(-1);
    }
    UnitHeapHeader *s_tmp_header = next_heap_header_;
    next_heap_header_ = pt;
    pt->next_ptr_ = s_tmp_header;
    usefull_count_++;
    return this;
  } else {
    // want to free invalid buff
    return NULL;
  }
}

bool UnitHeap::IsContainData() // memPointorOk
{
  return p_start_data_ != NULL;
}