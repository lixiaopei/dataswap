#ifndef SSTART_SDMBLOCK_H
#define SSTART_SDMBLOCK_H
#include "PI.h"
#include "Semaphore.h"



enum EnumSdmBlockStatus
{
	e_data_in_memory = 0,
	e_data_ready_to_write = 1,
	e_data_write_in_disk = 2,
};
enum EnumSdmBlockMemSource
{
	e_data_from_cache_pool = 1,
	e_data_from_new_heap = 2
};
class CRdpBlock
{
protected:
	CMutex* mplocker;
	std::list<char*>* mpmemList;
	int64_t dtLen; //24

	int diskFileSeq;
	EnumSdmBlockMemSource memSource; //1: mem 2: disk
	char* dt;
	EnumSdmBlockStatus status;
	boost::atomic<int> ref;
public:
	CRdpBlock(EnumSdmBlockMemSource stype);
	CRdpBlock();
	~CRdpBlock();
protected:
	std::wstring swpFilePath(int dfs, int len);
public:
	static CRdpBlock* initMem(void* p, EnumSdmBlockMemSource atype);
	EnumSdmBlockMemSource getMemSource() { return memSource; }
	int get();
	void put();
	int clear();
	int merge(std::list<CRdpBlock*> aList, int len);
	int realRead();
	int realWrite();
	int releaseSource();

	inline char* data(){ return dt;}
	inline int64_t dataLen(){return dtLen;}
	inline void lock(){mplocker->lock();}
	inline int memStatus(){return status;}
	inline void setData(char* dt, int64_t len){
		this->dt = dt;this->dtLen = len;
		//printf("CRdpBlock dtLen:%d\n",len);
	}
	inline void setMemStatus(EnumSdmBlockStatus v) { this->status = v; }
	inline void unlock(){	mplocker->unlock(); }
};
#endif
