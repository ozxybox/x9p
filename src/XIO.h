#pragma once
#include "X9PFileSystem.h"


class XFile
{
public:

	XFile();
	
	~XFile(); // Clunk
	void Attach(XAuth* auth, const xstr_t aname, X9PFileSystem* fs, Rattach_fn callback = 0);
	void Walk(XFile& newhnd, const char* path, Rwalk_fn callback = 0);

	// TODO: an xstr walk can only walk one dir at a time. How do we want to solve input?
	void Walk(XFile& newhnd, const xstr_t path, Rwalk_fn callback = 0);

	void Open(openmode_t mode, Ropen_fn callback = 0);
	void Create(xstr_t name, dirmode_t perm, openmode_t mode, Rcreate_fn callback = 0);
	void Read(uint64_t offset, uint32_t count, Rread_fn callback = 0);
	void Write(uint64_t offset, uint32_t count, void* data, Rwrite_fn callback = 0);
	//void WriteString(char* str, Rwrite_fn callback = 0);
	void Remove(Rremove_fn callback = 0);
	void Stat(Rstat_fn callback = 0);
	void WStat(stat_t* stat, Rwstat_fn callback = 0);
	void Clunk(Rclunk_fn callback = 0);

	void Hook(Twrite_fn hookcallback, Ropen_fn begincallback = 0);


	// Wait for all outstanding msgs to be received
	void Await();
private:

	xhnd_data m_hnd;
	int m_outstanding;

};

// TODO: Store FIDS, their QID, DMODE, etc. in a tracker somewhere without attaching to the client 

