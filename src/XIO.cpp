#include "XIO.h"
#include "X9PClient.h"
#include <assert.h>


XFile::XFile()
{
	m_hnd = { 0,0,0 };
	m_outstanding = 0;
}

XFile::~XFile()
{
	// Clunk if refcount is 0 and flush requests
	// 
	// FIXME: We need to flush any outstanding msgs and ensure their callbacks cannot return back here! 
	//	      Otherwise, we will have invalid memory accesses!
	// 
	// TODO: We need to ref count our hnds!
	//
	//m_hnd.fs->Tclunk(&m_hnd, 0);
}

void XFile::Attach(XAuth* auth, xstr_t aname, X9PFileSystem* fs, Rattach_fn callback)
{
	m_hnd.auth = auth;
	m_hnd.fs = fs;
	m_hnd.id = XHID_UNINITIALIZED;

	fs->Tattach(&m_hnd, 0, auth->uname, aname,
		[=](xerr_t err, qid_t* qid) {
			this->m_outstanding--;
			if(callback) callback(err, qid);
		});
	m_outstanding++;
}


void XFile::Walk(XFile& newhnd, const char* path, Rwalk_fn callback)
{
	int nwalk = 0;
	xstr_t walk = 0;
	xstrwalkfrompath(path, nwalk, walk);

	// TODO: Break walk up into chunks maybe? Or force the user to only send in blocks of 16 but idk
	assert(nwalk < X9P_TWALK_MAXELEM);

	newhnd.m_hnd = m_hnd;
	newhnd.m_hnd.id = XHID_UNINITIALIZED;
	m_hnd.fs->Twalk(&m_hnd, &newhnd.m_hnd, nwalk, walk, 
		[=](xerr_t err, uint16_t nwqid, qid_t* wqid) {
			this->m_outstanding--;
			if (callback) callback(err, nwqid, wqid);
		});

	// LOL
	free(walk);

	m_outstanding++;
}

void XFile::Walk(XFile& newhnd, xstr_t path, Rwalk_fn callback)
{
	newhnd.m_hnd = m_hnd;
	newhnd.m_hnd.id = XHID_UNINITIALIZED;
	m_hnd.fs->Twalk(&m_hnd, &newhnd.m_hnd, 1, path,
		[=](xerr_t err, uint16_t nwqid, qid_t* wqid) {
			this->m_outstanding--;
			if (callback) callback(err, nwqid, wqid);
		});
	m_outstanding++;
}

void XFile::Open(openmode_t mode, Ropen_fn callback)
{
	m_hnd.fs->Topen(&m_hnd, mode,
		[=](xerr_t err, qid_t* qid, uint32_t iounit) {
			this->m_outstanding--;
			if (callback) callback(err, qid, iounit);
		});
	m_outstanding++;
}
void XFile::Create(xstr_t name, dirmode_t perm, openmode_t mode, Rcreate_fn callback)
{
	m_hnd.fs->Tcreate(&m_hnd, name, perm, mode,
		[=](xerr_t err, qid_t* qid, uint32_t iounit) {
			this->m_outstanding--;
			if (callback) callback(err, qid, iounit);
		});
	m_outstanding++;
}
void XFile::Read(uint64_t offset, uint32_t count, Rread_fn callback)
{
	m_hnd.fs->Tread(&m_hnd, offset, count,
		[=](xerr_t err, uint32_t count, void* data) {
			this->m_outstanding--;
			if (callback) callback(err, count, data);
		});
	m_outstanding++;
}
void XFile::Write(uint64_t offset, uint32_t count, void* data, Rwrite_fn callback)
{
	m_hnd.fs->Twrite(&m_hnd, offset, count, data,
		[=](xerr_t err, uint32_t count) {
			this->m_outstanding--;
			if (callback) callback(err, count);
		});
	m_outstanding++;
}
void XFile::Remove(Rremove_fn callback)
{
	m_hnd.fs->Tremove(&m_hnd,
		[=](xerr_t err) {
			this->m_outstanding--;
			if (callback) callback(err);
		});
	m_outstanding++;
}
void XFile::Stat(Rstat_fn callback)
{
	m_hnd.fs->Tstat(&m_hnd,
		[=](xerr_t err, stat_t* stat) {
			this->m_outstanding--;
			if (callback) callback(err, stat);
		});
	m_outstanding++;
}
void XFile::WStat(stat_t* stat, Rwstat_fn callback)
{
	m_hnd.fs->Twstat(&m_hnd, stat,
		[=](xerr_t err) {
			this->m_outstanding--;
			if (callback) callback(err);
		});
	m_outstanding++;
}

void XFile::Clunk(Rclunk_fn callback)
{
	m_hnd.fs->Tclunk(&m_hnd,
		[=](xerr_t err) {
			this->m_outstanding--;
			if (callback) callback(err);
		});
	m_outstanding++;
}


void XFile::Hook(Twrite_fn hookcallback, Ropen_fn begincallback)
{
	m_hnd.fs->Topen(&m_hnd, X9P_OPEN_EXEC,
		[=](xerr_t err, qid_t* qid, uint32_t iounit) {
			this->m_outstanding--;
			if (begincallback) begincallback(err, qid, iounit);

			// FIXME: We need to move this out of the client and into an independent system!
			if (!err)
				m_hnd.auth->client->m_fiddata[m_hnd.id]->hooks.push_back(new Twrite_fn(hookcallback));
		});
	m_outstanding++;
}


void XFile::Await()
{

	// FIXME: We need to move this out of the client and into an independent system!
	while (m_outstanding > 0)
		m_hnd.auth->client->ProcessPackets();
}