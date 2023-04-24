#ifndef SSTART_SDM_H
#define SSTART_SDM_H
#include "PI.h"
class CRdp
{
public:
	boost::thread gRdpBuffMainThread;
	boost::thread gRdpBuffWriteThread;
	boost::thread gRdpBuffReadThread;
	int destroy();
	int  init(int64_t memLimit, int64_t CDiskLimit);
	void setPara(int64_t memLimit, int64_t CDiskLimit);
public:
	CRdp();
	virtual ~CRdp(){ 
		gRdpBuffMainThread.join();
		gRdpBuffWriteThread.join();
		gRdpBuffReadThread.join();
	}
protected:
	int clearCacheDir();
	int mkCacheDir();

	int init_sdm_write_read_thread();
	void sdm_buff_main_threa();
	void sdm_buff_write_thread();
	void sdm_buff_read_thread();
protected:
	char* nlBuffer;
	char* bkBuffer;
};



class CRdr
{
public:
	virtual ~CRdr();
	 void sdataHookScheduleThread();
	 int hookuser_main_thread_process(char* pm);
	 void sdataHookMainThread();
	
	 int detailwith_buff_full(int debugLevel);
	 int sdataHookRecvThread();
	 int sdhInit();
	
	 int init();
	 int destroy();
public:
	static CRdr* inst();

protected:
	bool ghHookUserExit;//{ false };
};
#endif
