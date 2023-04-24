#include "RdpBlock.h"
#include "RdpMemCache.h"
#include "MemoryLimit.h"
#include "DiskLimit.h"
#include "FileLogger.h"
#include "stdio.h"
#include "G.h"
#ifdef WIN32
	#include "io.h"
#endif
#include "FileTool.h"

CRdpBlock::CRdpBlock(EnumSdmBlockMemSource stype)
{
	ref = 0;
	mplocker = new CMutex();
	mpmemList = new std::list<char*>();
	memSource = stype;
	dtLen = 0;
	diskFileSeq = 0;
	dt = NULL;
	status = e_data_in_memory; // in mem
}
CRdpBlock::CRdpBlock()
{
	ref = 0;
	mplocker = new CMutex();
	mpmemList = new std::list<char*>();
	memSource = e_data_from_cache_pool;
	dtLen = 0;
	diskFileSeq = 0;
	dt = NULL;
	status = e_data_in_memory; // in mem
}
CRdpBlock::~CRdpBlock()
{
	this->clear();
}
std::wstring CRdpBlock::swpFilePath(int dfs, int len) {
	std::wstring s;
	wchar_t* buf = new wchar_t[len + 1];
	if (!buf)
		return s;
	swprintf(buf,L"%s\\sdm_%010u.swp", Env::g()->cachePath.c_str(),dfs);
	s = buf;
	delete[]buf;
	return s;
}


CRdpBlock* CRdpBlock::initMem(void* p, EnumSdmBlockMemSource atype) {
	CRdpBlock* pt = new (p) CRdpBlock(atype);
	return pt;
}


int CRdpBlock::get()
{
	return ++ref;
}
void CRdpBlock::put()
{
	--ref;
	if (ref == 0) {	
		this->~CRdpBlock();
		CRdpMemCache::bkCache()->free((char*)this);
		G::g()->getMemLimit()->release(CRdpMemCache::bkCache()->perMemSize());
	}
}

int CRdpBlock::clear()
{
	this->releaseSource();
	if(mplocker){
		delete mplocker;
		mplocker = NULL;
	}
	if(mpmemList){
		delete mpmemList;
		mpmemList = NULL;
	}
	return 0;
}


int CRdpBlock::merge(std::list<CRdpBlock*> aList, int len)
{
	if (aList.empty()) {
		if (Env::debug() >= 1)
			FL::g()->write("merge head pointer is NUL");
		return -1;
	}
	char* sTotalData = (char*)malloc(len);
	if (!sTotalData) {
		if (Env::debug() >= 1)
			FL::g()->write("merge malloc memory fail.");
		return -1;
	}
	int64_t idx = 0;
	for(std::list<CRdpBlock*>::iterator ite = aList.begin();
		ite != aList.end();
		++ite)
	{
		CRdpBlock* pt = *ite;
		memcpy(sTotalData + idx, pt->data(), pt->dataLen());
		idx += pt->dataLen();
	}
	G::g()->getMemLimit()->alloc(len);
	mplocker->lock();
	dt = sTotalData;
	dtLen = idx;
	mplocker->unlock();
	return 0;
}
int CRdpBlock::realRead()
{
	mplocker->lock();
	if (status || dt) {
		mplocker->unlock();
		return -100;
	}
	std::wstring sFilePath = this->swpFilePath(diskFileSeq, 1024);
	FILE* file;
#ifdef WIN32
	if(_wfopen_s(&file,sFilePath.c_str(), L"rb")){
#else
	if (file = fopen(sFilePath.c_str(), "rb")) {
#endif
		mplocker->unlock();
		return -1;
	}
	int ret = 0;
	do
	{
		int fileLength = CFileTool::fileLength(file);
		if (fileLength < 0)
			return -2;
		if (fileLength != dtLen)
		{
			ret = -3; break;
		}
		int sFileNo = ::fileno(file);
		if (sFileNo < 0)
		{
			ret = -4; break;
		}
		char* sBuf = (char*)malloc(dtLen);
		if (sBuf == 0)
		{
			ret = -5; break;
		}
#ifdef WIN32
		int sReadLen = fread_s(sBuf, dtLen, 1, dtLen, file);
#else
		int sReadLen = fread(sBuf, 1, dtLen, file);
#endif
		if (sReadLen != dtLen)
		{
			if (Env::debug() >= 1)
				FL::g()->write("read from disk error");
			free(sBuf);
			{
				ret = -6; break;
			}
		}
		diskFileSeq = 0;
		G::g()->getMemLimit()->alloc(dtLen);
		G::g()->getDiskLimit()->release(dtLen);
		++ Env::g()->diskReadCount;
		dt = sBuf;
		status = e_data_in_memory; // in memory
		memSource = e_data_from_new_heap;
	} while (false);
	fclose(file);
	if (!ret)
		_wunlink(sFilePath.c_str());
	mplocker->unlock();
	return 0;
}
int CRdpBlock::realWrite()
{
	mplocker->lock();
	if (!(status == e_data_ready_to_write && dt))//status = 1, ready to write
	{
		mplocker->unlock();
		return -100;
	}

	std::wstring sFilePath = L"";
	boost::atomic<int32_t> sSeq( diskFileSeq );
	do {
		++sSeq;
		sFilePath = this->swpFilePath(sSeq, 1024);
	} while (!::_waccess(sFilePath.c_str(), 0));
	FILE* file;
#ifdef WIN32
	if (_wfopen_s(&file, sFilePath.c_str(), L"wb+"))
#else
	if (file = fopen(sFilePath.c_str(), "wb+"))
#endif
	{
		mplocker->unlock();
		return -1;
	}
	int ret = 0;
	do
	{
		int sHandle = fileno(file);
		if (sHandle < 0)
		{
			ret = -2;
			break;
		}
		int64_t sDataLen = this->dtLen;
		char* pd = this->dt;
		while (true) {
			int sWriteLen = fwrite(pd, 1, sDataLen, file);
			if (sWriteLen < 0)
				break;
			if (sWriteLen >= sDataLen) {
				free(this->dt);
				diskFileSeq = sSeq;
				dt = 0;
				G::g()->getMemLimit()->release(dtLen);
				G::g()->getDiskLimit()->alloc(dtLen);
				Env::g()->diskWriteCount += dtLen;
				status = e_data_write_in_disk;
				break;
			}
			sDataLen -= sWriteLen;
			pd = pd + sWriteLen;
		}

	} while (false);
	fclose(file);
	if (ret)
		_wunlink(sFilePath.c_str());
	mplocker->unlock();
	return ret;
}
int CRdpBlock::releaseSource()
{
	mplocker->lock();
	if (diskFileSeq) {
		::_wunlink(swpFilePath(diskFileSeq, 1024).c_str());
		G::g()->getDiskLimit()->release(dtLen);
		diskFileSeq = 0;
	}
	if (dt) {
		if (memSource == e_data_from_cache_pool) //cache mem
		{
			char* p = this->dt - 32;
			CRdpMemCache::nlCache()->free(p);
			G::g()->getMemLimit()->release(CRdpMemCache::nlCache()->perMemSize());
		}
		else
		{
			free(dt);
			G::g()->getMemLimit()->release(dtLen);
		}
		this->dt = NULL;
	}
	mplocker->unlock();
	return 0;
}