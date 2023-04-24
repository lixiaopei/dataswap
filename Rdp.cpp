#include "Rdp.h"

#include<sys/stat.h>  
#include<sys/types.h> 
#include <stdlib.h>
#include <LocalFileAttrOp.h>
#include <StringTool.h>
#include "CacheList.h"
#include "DiskLimit.h"
#include "MemoryLimit.h"
#include "RdpMemCache.h"
#include "DriverIo.h"
#include "StartRuleSet.h"
#include "StartRule.h"

#include "RdpBuffer.h"
#include "RdpBuffContainer.h"
#include "FileLogger.h"
#include "RdpBlock.h"
#include "MemoryLimit.h"
#include "G.h"

#ifdef WIN32
#include <direct.h>
#include <io.h>
#include "Fltuser.h"
#else
#include <dirent.h>
#endif

CRdp::CRdp(){
	bkBuffer = 0;
	nlBuffer = 0;
}
int CRdp::init_sdm_write_read_thread()
{
	gRdpBuffMainThread = boost::thread(boost::bind(&CRdp::sdm_buff_main_threa, this));
	gRdpBuffWriteThread = boost::thread(boost::bind(&CRdp::sdm_buff_write_thread, this));
	gRdpBuffReadThread = boost::thread(boost::bind(&CRdp::sdm_buff_read_thread, this));
	return 0;
}
int  CRdp::destroy() {
	if (nlBuffer)
	{
		CRdpMemCache::nlCache()->free(nlBuffer);
		G::g()->getMemLimit()->release(CRdpMemCache::nlCache()->perMemSize());
	}
	if (bkBuffer)
	{
		CRdpMemCache::bkCache()->free(bkBuffer);
		G::g()->getMemLimit()->release(CRdpMemCache::bkCache()->perMemSize());
	}
	return clearCacheDir();
}
int  CRdp::init(int64_t memLimit, int64_t CDiskLimit)
{
	setPara(memLimit, CDiskLimit);
	if (mkCacheDir())
	{
		return -1;
	}
	if (init_sdm_write_read_thread())
	{
		return -1;
	}

	this->bkBuffer = (char*)CRdpMemCache::rdpBkMalloc(e_data_from_cache_pool);
	this->nlBuffer = (char*)CRdpMemCache::rdpMalloc();
	//gRdpBkBuffer = rdpBkMalloc(1);
	//gRdpNlBuffer = rdpMalloc();
	Env::g()->gRdpInitTime = CTimeTool::getNowUsec();
	return 0;
}
void CRdp::setPara(int64_t memLimit, int64_t CDiskLimit)
{
	if (memLimit <= 0)
		memLimit = 524288000;
	if (CDiskLimit < 0)
		CDiskLimit = 0;
	G::g()->getMemLimit()->setMax(memLimit);
	G::g()->getDiskLimit()->setMax(CDiskLimit);
	FL::g()->write(LL_DEBUG,"Inner G::g()->rdp()->setPara with memlimit:%lld, CDiskLimit:%lld", memLimit, CDiskLimit);
	return;
}
int CRdp::clearCacheDir() {
#ifdef WIN32
	std::wstring searchPath = Env::g()->cachePath + L"sdm_*.swp";
	_wfinddata_t findData;
	int64_t fsize = 0;
	intptr_t findHanle = ::_wfindfirst(searchPath.c_str(), &findData);
	if (findHanle != -1)
	{
		do
		{
			std::wstring dtr = Env::g()->cachePath +  findData.name;
			::_wunlink(dtr.c_str());
		} while (!::_wfindnext(findHanle, &findData));
		::_findclose(findHanle);
	}
	return 0;
#else
  std::string searchPath = Env::g()->cacheDir;
  struct dirent *dirent1;
  DIR *dir = opendir(searchPath.c_str());
  if (dir) {
    if (dirfd(dir) != -1) {
      while (true) {
        dirent1 = readdir(dir);
        if (!dirent1) break;
        if (dirent1->d_type == 8) {
          std::string fname = Env::g()->cacheDir + PATHSEPARATOR_STR + dirent1->d_name;
          unlink(fname.c_str());
        }
      }
      closedir(dir);
    }
  }
  return 0;
#endif
}
int CRdp::mkCacheDir() {
#ifdef WIN32
	if (_waccess(Env::g()->cachePath.c_str(), 0)!=0)
	{
		int ret = _wmkdir(Env::g()->cachePath.c_str());
		int serr = errno_t();
		if (ret && serr != EEXIST)
		{
			return -1;
		}
	}
	else
	{
		clearCacheDir();
	}
	return 0;
#else
	if (access(Env::g()->cacheDir.c_str(), F_OK) != 0) {
		int ret = mkdir(Env::g()->cacheDir.c_str(), 0755);
		if (ret != 0) {
			int errNo = errno;
			if (errNo != EEXIST) {
				ret = 0 - errNo;
				return ret;
			}
		}
	} else {
		clearCacheDir();
	}
	return 0;
#endif
}



void CRdp::sdm_buff_main_threa()
{
	int sCount = 0;
	int sv2;
	while (!Env::g()->extFlag)
	{
		CRdpBuffContainer* p = G::g()->sbc();
		p->wait(e_main_event);
		p->lock();
		sCount = p->count();
		while (!Env::g()->extFlag && sCount > 0)
		{
			sCount--;

			if (p->empty())
				break;
			boost::shared_ptr<CRdpBuffer> pCRdpBuffer = p->front();
			if (pCRdpBuffer == NULL || pCRdpBuffer.get() == NULL)
				break;
			p->frontToEnd();
			p->unlock();
			pCRdpBuffer->lock();
			int memLevel = G::g()->getMemLimit()->test();
			pCRdpBuffer->loadReadyData(memLevel, CRdpLimits::inst()->mReadyLimit());
			pCRdpBuffer->saveNewToDisk(memLevel, CRdpLimits::inst()->mReadyLimit(), CRdpLimits::inst()->mDiskLimit());
			pCRdpBuffer->unlock();
			if (memLevel > 1){
				p->wakeup(e_write_event);
			}
			if (memLevel < 5){
				p->wakeup(e_read_event);
			}
			pCRdpBuffer->testCallSend();
			p->lock();
			pCRdpBuffer.reset();
		}
		p->unlock();
	}
}
void CRdp::sdm_buff_write_thread()
{
	int sCount = 0;
	int sv2;
	int ret = 0;
	while (!Env::g()->extFlag)
	{
		CRdpBuffContainer* p = G::g()->sbc();
		p->wait(e_write_event);
		p->lock();
		sCount = p->count();
		while (!Env::g()->extFlag && sCount > 0)
		{
			sCount--;
			if (p->empty())
				break;
			boost::shared_ptr<CRdpBuffer> pb = p->front();
			if (pb == NULL || pb.get() == NULL)
				break;
			p->frontToEnd();
			p->unlock();
			ret = pb->doRealWrite();
			if (ret < 0)
			{
				if (pb->ruleNode() != NULL)
				{
					FL::g()->write(pb->ruleNode()->GetUuid(),LL_FAULT,"E Failed to write monitor data into disk, code=%d",ret);
					pb->ruleNode()->AddNextAction(sdm_err_action);
				}
			}
			p->lock();
			pb.reset();
		}
		p->unlock();
	}
}
void CRdp::sdm_buff_read_thread()
{
	int sCount = 0;
	int sv2;
	int ret = 0;
	while (!Env::g()->extFlag)
	{
		CRdpBuffContainer* p = G::g()->sbc();
		p->wait(e_read_event);
		p->lock();
		sCount = p->count();		
		while (!Env::g()->extFlag && sCount > 0)
		{	
			sCount--;
			if (p->empty())
				break;
			boost::shared_ptr<CRdpBuffer> pb = p->front();
			if (pb == NULL || pb.get() == NULL)
				break;
			p->frontToEnd();
			p->unlock();
			ret = pb->doRealRead();
			if (ret < 0)
			{
				if (pb->ruleNode()) {
					FL::g()->write(pb->ruleNode()->GetUuid(),LL_FAULT,"E Failed to read monitor data from disk, code=%d",ret);
					pb->ruleNode()->AddNextAction(sdm_err_action);
				}
			}
			pb->testCallSend();
			p->lock();
			pb.reset();
		}
		p->unlock();
	}
}

void CRdr::sdataHookScheduleThread()
{
	TSCacheList* gm = TSCacheList::gMallocMem();
	while (!Env::g()->extFlag && !this->ghHookUserExit)
	{
		gm->waitPop();
		while (gm->canPush())
		{
			char* pm = CRdpMemCache::rdpMalloc();
			if (pm == NULL)
				break;
			gm->push(pm);

		}
	}
	return;
}
int CRdr::hookuser_main_thread_process(char* pm)
{
	FltMessage* pMsg = (FltMessage*)pm;
	if (!pm)
		return KErrorMonitorDataNull;
	boost::shared_ptr<CStartRule> pRule = CStartRuleSet::instance()->findByUuid(pMsg->body.getUuid());
	if (pRule == NULL || pRule.get() == NULL)
		return -kErrTaskNotMatch;
	/*
	if (pMsg->attrBody.action84 == rule_rmdir){
		FL::g()->write(LL_DEBUG,"A hookuser_main_thread_process rmdir action");
	}
	if (pMsg->attrBody.action84 == rule_unlink){
		FL::g()->write(LL_DEBUG,"A hookuser_main_thread_process cunlink action");
	}
	if (pMsg->attrBody.action84 == rule_link){
		FL::g()->write(LL_DEBUG,"A hookuser_main_thread_process check out the link data for ret");
	}
	if (pMsg->attrBody.action84 == rule_symblink){
		FL::g()->write(LL_DEBUG,"A hookuser_main_thread_process check out the rule_symblink data for ret");
	}
	if (pMsg->attrBody.action84 == rule_rename){
		FL::g()->write(LL_DEBUG,"A rule_rename:[%s]->[%s]", CStringTool::toUtf8((wchar_t*)pMsg->paraBody.getPath()).c_str(), CStringTool::toUtf8((wchar_t*)pMsg->paraBody.getDPath()).c_str());
	}
	if (pMsg->attrBody.action84 == rule_writedata){
		FL::g()->write(LL_DEBUG,"A rule_writedata path:%s, offset:%d, dataLen:%d", CStringTool::toUtf8((wchar_t*)pMsg->writeBody.getPath()).c_str(), pMsg->writeBody.datalen92, pMsg->writeBody.offset96);
	}*/
	if (pMsg->attrBody.action84 == rule_setattr)
	{
		//todo03
		if(pMsg->attrBody.subType96 & e_security)
		{
			static wchar_t dstbuf[2048] = {0};
			memcpy(dstbuf, pMsg->attrBody.path(), pMsg->attrBody.pathLenPlusOne92);
			wchar_t* dpath = (wchar_t*)DriverIo::inst()->getVolumeMap()->convertFileObjectNameToDosNameW(dstbuf,pMsg->attrBody.pathLenPlusOne92);	
			CLocalFileAttrOp sLocalAttrRw;
			CByteEncode sBytePacker;
			bool bRd = sLocalAttrRw.readInfo((dpath),sBytePacker);
			if(bRd)
			{
				char* sByteData = sBytePacker.Data();
				pMsg->attrBody.securityStrLen88 = *(WORD*)(sByteData + 40) - StuFileCommonAttr::headerLen();
				pMsg->attrBody.fileAttrSecurityType100 = *(DWORD*)(sByteData+4);
				pMsg->attrBody.allocateSize104 = *(uint64_t*)(sByteData+8);

				pMsg->attrBody.accessTime112 = *(uint64_t*)(sByteData+16);
				pMsg->attrBody.createTime128 = *(uint64_t*)(sByteData+32);
				pMsg->attrBody.writeTime120 = *(uint64_t*)(sByteData+24);
				pMsg->attrBody.dtLen136 = *(uint64_t*)(sByteData+40);
				pMsg->attrBody.fmode138 = *(WORD*)(sByteData+42);
				pMsg->attrBody.allocateSize104 = *(WORD*)(sByteData+8);
				memcpy(pMsg->attrBody.securityStr140, sByteData + StuFileCommonAttr::headerLen(), pMsg->attrBody.securityStrLen88);
				memcpy(pMsg->attrBody.path(), dstbuf, pMsg->attrBody.pathLenPlusOne92);
			}
		}
		else if(pMsg->attrBody.subType96 & e_fileattribute)
		{
			pMsg->attrBody.fmode138 = pMsg->attrBody.fileAttrSecurityType100;
		}
	}
	int ret = 0;
	do
	{
		int sMsgLen = pMsg->msgLen();
		pRule->lock();
		if (pRule->doAddupDiagnosis(sMsgLen) == 1)  //cluster
		{
			ret = -1;
			break;
		}
		if (pRule->getStatus() == stop_status ||
			pRule->getStatus() == stale_status)
		{
			FL::g()->write(LL_DEBUG, "when add_buff_to_node, state problem: %d\n", pRule->getStatus());
			ret = KErrorRuleStopStale;
			break;
		}
		pRule->processFltMsgPath(pMsg);
		//to debug
		ret = pRule->sdmBuffer()->addnlToBuff((char*)pMsg->body.uuid32, sMsgLen);
		if (!ret)
		{
			break;
		}
		pRule->unlock();
		if (ret == kErrMemFullNoDisk)
		{
			if(pRule->getLastAction() != stop_action){
				FL::g()->write(pRule->GetUuid(),LL_FAULT, "E call addnl_to_buff in buff full, turns rule to STOP.");
				pRule->AddNextAction(stop_action);
			}
			
		}
		else
		{
			if(pRule->getLastAction() != sdm_err_action){
				FL::g()->write(pRule->GetUuid(),LL_FAULT, "E call addnl_to_buff fault rc=%d, turns rule to STALE.",  ret);
				pRule->AddNextAction(sdm_err_action);
			}
		}
		pRule.reset();
		return ret;
	} while (false);
	pRule->unlock();
	pRule->sdmBuffer()->testCallSend();
	pRule.reset();
	return ret;
	
}
void CRdr::sdataHookMainThread()
{
	while (!Env::g()->extFlag && !this->ghHookUserExit)
	{
		char* pm = TSCacheList::gRecMem()->pop();
		if (pm)
		{
			int ret = hookuser_main_thread_process(pm);
			if (ret)
			{
				if (ret != -1 && Env::debug() >= 3)
				{
					//FL::g()->write("call and buff_to_node...");
				}
				CRdpMemCache::sdm_free(pm);
			}
		}
		else
		{
			TSCacheList::gRecMem()->waitPop();
		}
	}
	return;
}

int CRdr::detailwith_buff_full(int debugLevel)
{
	char bf[DRIVER_PACKEG_LEN] = { 0 };
	int ret = DriverIo::inst()->recv(bf, DRIVER_PACKEG_LEN);
	if (ret > 0)
	{
		std::string uuid = ((FltMessage*)bf)->body.getUuid();
		boost::shared_ptr<CStartRule> pRule = CStartRuleSet::instance()->findByUuid(uuid);
		if (pRule && pRule.get())
		{
			pRule->AddNextAction(sdm_err_action);//cmd_6
			pRule->Put();
			return 0;
		}
		else
		{
			if (debugLevel >= 1)
				FL::g()->write("dealwith_buff full call find rule by uuid fault");
		}
	}
	else
	{
		if (ret != 0)
		{
			return ret;
		}
		else
		{
			return -1;
		}
	}
	return 0;
}
int CRdr::sdataHookRecvThread()
{
	int v10 = 0;
	int v11 = 0;
	while (!Env::g()->extFlag && !this->ghHookUserExit)
	{
		++v10;
		char* pm = TSCacheList::gMallocMem()->pop();
		if (pm == NULL)
		{
			++v11;
			TSCacheList::gMallocMem()->wakeupPop();
			//FL::g()->write(LL_INFO,"Free Memory is Empty.");
			pm = CRdpMemCache::rdpMalloc();
		}
		//TSCacheList::gMallocMem()->displaySize("rec gMallocMem");
		if (pm)
		{
			int ret = DriverIo::inst()->recv(pm, DRIVER_PACKEG_LEN);
			if (ret > 0)
			{
				FltMessage* spm = ((FltMessage*)pm);
				//printf("DriverIO::recv the uuid:%s, with action:%d\n",((FltMessage*)pm)->body.getUuid(), sAction);

				TSCacheList::gRecMem()->push(pm);
				TSCacheList::gRecMem()->displaySize("rec gRecMem");
				

				if (!TSCacheList::gRecMem()->canPush() && G::g()->getMemLimit()->test() > 4)
				{
					FL::g()->write(LL_FAULT,"Recv Memory queue is Full, limited in %d, sleep 1 s to wait", TSCacheList::gRecMem()->maxsize());
					SLEEP(1000);
					if (!TSCacheList::gRecMem()->canPush() && G::g()->getMemLimit()->test() > 4){
						FL::g()->write(LL_FAULT,"Recv Memory queue is Full, limited in %d,  to die", TSCacheList::gRecMem()->maxsize());
						CStartRuleSet::instance()->stateRuleSet(stale_status);
						TSCacheList::gRecMem()->clear();
					}
				}		
				if (TSCacheList::gRecMem()->canPop())
					TSCacheList::gRecMem()->wakeupPop();
			}
			else
			{
				if(Env::g()->extFlag)
					break;
				CRdpMemCache::sdm_free(pm);
				CStartRuleSet::instance()->stateRuleSet(stale_status);
				TSCacheList::gRecMem()->clear();
				break;
			}
		}
		else
		{
			int debugLevel = Env::g()->gDebug;
			if (debugLevel >= 1)
				FL::g()->write("Alloc memory fault. Memory achieve your limit.");
			int ret = detailwith_buff_full(debugLevel);
			if (ret == -6)
			{
				CStartRuleSet::instance()->stateRuleSet(stop_status);
				return -1;
			}
			if (ret > 0)
			{
				if (debugLevel >= 1) {
					FL::g()->write(LL_FAULT,"Call dealwith buff_full fault, rc=%d, Stale rules now", ret);
				}
				CStartRuleSet::instance()->stateRuleSet(stale_status);
				TSCacheList::gRecMem()->clear();
			}
		}
	}
	return 0;
}
int CRdr::sdhInit()
{
	TSCacheList::gRecMem();
	TSCacheList::gMallocMem();
	boost::thread s1(boost::bind(&CRdr::sdataHookScheduleThread,this));
	boost::thread s2(boost::bind(&CRdr::sdataHookMainThread, this));
	boost::thread s3(boost::bind(&CRdr::sdataHookRecvThread, this));
	s1.detach();
	s2.detach();
	s3.detach();
	return 0;
}

int CRdr::init() {
	ghHookUserExit = false;
	if (do_not_debug_driver)
		return 0;
	if (!DriverIo::inst()->connect())
	{
		return -1;
	}
	if (sdhInit() >= 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}
int CRdr::destroy() {
	if (ghHookUserExit == false) {
		ghHookUserExit = true;
		TSCacheList::gRecMem()->wakeupPop();
		TSCacheList::gMallocMem()->wakeupPop();
	}
	return 0;
}
CRdr::~CRdr()
{
	this->destroy();
}
CRdr* CRdr::inst()
{
	static CRdr gCRdr;
	return &gCRdr;
}