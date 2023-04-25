#include "unit_mem_cache.h"

UnitMemCache::UnitMemCache(unsigned int per_mem_size) {
  per_mem_size_ = per_mem_size;
  last_heap_ = 0;
}

UnitMemCache::~UnitMemCache() {
  locker_.lock();
  while (!heap_list_.empty()) {
    UnitHeap *sHeap = heap_list_.front();
    heap_list_.pop_front();
    if (sHeap->CheckSpace() == heap_no_used) {
      delete sHeap;
    } else {
      FL::g()->write("RdpMemCache find some memory leak!\n");
    }
  }
  locker_.unlock();
}

UnitHeap *UnitMemCache::CreateHeap() {
  const int sMaxSize = 4096000;
  int additional = (int)offsetof(struct SdmMemHeapHeader, data);
  int scnt = sMaxSize / (per_mem_size_ + additional);
  if (scnt) {
    if (scnt < 10) {
      int ss = 10 / scnt + 1;
      scnt = sMaxSize * ss / (per_mem_size_ + additional);
    }
  } else {
    scnt = 10;
  }
  UnitHeap *sHeap = new UnitHeap(per_mem_size_ + additional, scnt);
  if (sHeap && sHeap->IsContainData()) {
    return sHeap;
  } else {
    delete sHeap;
    return 0;
  }
}

int UnitMemCache::FreeUnit(char *aMem) {
  locker_.lock();
  UnitHeap *s_heap = UnitHeap::FreePureMem(aMem);
  if (s_heap) {
    if (s_heap->CheckSpace() == heap_no_used) {
      if (s_heap == last_heap_) {
        last_heap_ = 0;
      }
      std::list<UnitHeap *>::iterator ite = heap_list_.begin();
      while (ite != heap_list_.end()) {
        if (*ite == s_heap) {
          heap_list_.erase(ite);
          break;
        }
        ite++;
      }
      delete s_heap;
    }
  }
  locker_.unlock();
  return 0;
}

char *UnitMemCache::MallocUnit(unsigned int mem_limit) {
  char *ret = 0;
  locker_.lock();
  if (last_heap_ && last_heap_->CheckSpace() != UNIT_HEAP_FULL) {
    ret = last_heap_->MallocUnit();
  } else {
    unsigned int alloc_size = 0;
    std::list<UnitHeap *>::iterator ite;
    for (ite = heap_list_.begin(); ite != heap_list_.end(); ite++) {
      if ((*ite)->CheckSpace() != UNIT_HEAP_FULL) {
        last_heap_ = *ite;
        ret = last_heap_->MallocUnit();
        break;
      }
      alloc_size += (*ite)->GetAllocSpace();
    }
    if (ret == 0 && (mem_limit == 0 || (alloc_size < mem_limit))) {
      UnitHeap *sHeap = this->CreateHeap();
      if (sHeap) {
        heap_list_.push_back(sHeap);
        last_heap_ = sHeap;
        ret = last_heap_->MallocUnit();
      } else {
        // failed to create heap
      }
    }
  }
  locker_.unlock();
  return ret;
}