#ifndef SHARECODE_LAYER_H
#define SHARECODE_LAYER_H

enum ELayerPageType {
  ELPT_HEAD_PAGE = 0,
  ELPT_TAIL_PAGE = 1,
  ELPT_DATA_PAGE = 2,
};
enum ELayerStrategy {
  ELS_FRONT_CACHE = 0,
  ELS_BACK_CACHE = 1,
  ELS_MIDDLE_CACHE = 2,
};
enum ElayerMode {
  ELM_WRITE = 0,
  ELM_READ = 1,
};

// max memory limit 400M
class Layer {
private:
  Page head_page_;
  Page tail_page_;
  std::vector<Page> page_list_;
  CMutex page_locker_;
  UnitMemCache unit_mem_cache_;

  int job_id_;
  UnitDiskCache unit_disk_cache_;

  ELayerStrategy layer_strategy_;
  unsigned int memory_limit_;
  ElayerMode layer_mode_;

public:
  Layer() {
    layer_strategy_ = ELS_MIDDLE_CACHE;
    memory_limit_ = 4096 * 1000 * 100;
    layer_mode_ = ELM_WRITE;
  }

  void SetStrategy(ELayerStrategy strategy, unsigned int memory_limit,
                   ElayerMode layer_mode_) {
    layer_strategy_ = strategy;
    memory_limit_ = memory_limit;
    layer_mode_ = layer_mode;
  }

  void int WritePage(char *data, int length,
                     ELayerPageType pt = ELPT_DATA_PAGE);
  // trigger to start to save to disk
  void int AddPage();

public:
  Page *GetHeadPage() { return &head_page_; }
  Page *GetTailPage() { return &tail_page_; }
  // wait to load from disk
  Page *GetDataPage(size_t idx) { return &page_list_[idx]; }
};
#endif