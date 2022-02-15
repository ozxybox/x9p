#pragma once

#include "X9PFileSystem.h"
#include <stdint.h>
#include <stddef.h>
#include <unordered_map>

// FIDs are directory entries
// QIDs are virtual files


// File Type
typedef uint32_t ftype_t;
enum : ftype_t
{
	X9P_FT_FILE      = 0,
	X9P_FT_DIRECTORY = 1,
	X9P_FT_APPEND    = 2,
};


class XVirtualFile;
struct direntry_t
{
	xstr_t name;
	XVirtualFile* parent;
	XVirtualFile* node;
};



class XVirtualFile
{
public:
	virtual uint32_t Version() = 0;
	virtual epoch_t AccessTime() = 0;
	virtual epoch_t ModifyTime() = 0;

	virtual ftype_t Type() = 0;

	// Permissions
	virtual const xstr_t OwnerUser() = 0;
	virtual const xstr_t OwnerGroup() = 0;
	virtual const xstr_t LastUserToModify() = 0;

	// IO Functions
	virtual size_t Length() = 0;
	virtual bool Locked() = 0;
	virtual void Open(openmode_t mode) = 0;
	virtual void Close() = 0;
	virtual uint32_t Read (uint64_t offset, uint32_t count, uint8_t* buf) = 0;
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf) = 0;

	// Directory Functions
	virtual xerr_t AddChild(xstr_t name, XVirtualFile* file, direntry_t** out) = 0;
	virtual xerr_t RemoveChild(xstr_t name) = 0;
	virtual xerr_t FindChild(xstr_t name, direntry_t** out) = 0;
	virtual uint32_t ChildCount() = 0;
	virtual xerr_t GetChild(uint32_t index, direntry_t** out) = 0;
};

class XVirtualFileSystem : public X9PFileSystem
{
public:
	XVirtualFileSystem();
	
	direntry_t* RootDirectory();


	virtual xhnd NewFileHandle(int connection);


	// 9P RPC calls
	virtual void Tattach(xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback);
	virtual void Twalk  (xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback);
	virtual void Topen  (xhnd hnd, openmode_t mode, Ropen_fn callback);
	virtual void Tcreate(xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback);
	virtual void Tread  (xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback);
	virtual void Twrite (xhnd hnd, uint64_t offset, uint32_t count, uint8_t* data, Rwrite_fn callback);
	virtual void Tclunk (xhnd hnd, Rclunk_fn callback);
	virtual void Tremove(xhnd hnd, Rremove_fn callback);
	virtual void Tstat  (xhnd hnd, Rstat_fn callback);
	virtual void Twstat (xhnd hnd, stat_t* stat, Rwstat_fn callback);

	
	bool isValidXHND(xhnd hnd, direntry_t*& out);

private:
	std::unordered_map<xhnd, direntry_t*> m_handles;
	xhnd m_hndserial;

	direntry_t m_rootdirentry;
	XVirtualFile* m_rootnode;
};



// XVirtualFile with defaults filled in 
class XBaseVirtualFile : public XVirtualFile
{
public:

	virtual uint32_t Version()    { return 0; }
	virtual epoch_t  AccessTime() { return 0; }
	virtual epoch_t  ModifyTime() { return 0; }
	virtual ftype_t  Type()       { return X9P_FT_FILE; }
	
	// Permissions
	virtual const xstr_t OwnerUser()        { return (xstr_t)"\x4\x0user"; }
	virtual const xstr_t OwnerGroup()       { return (xstr_t)"\x5\x0group"; }
	virtual const xstr_t LastUserToModify() { return (xstr_t)"\x9\x0otheruser"; }

	// IO Functions
	virtual size_t Length() { return 0; }
	virtual bool Locked()   { return false; }
	virtual void Open(openmode_t mode) { }
	virtual void Close() { }
	virtual uint32_t Read (uint64_t offset, uint32_t count, uint8_t* buf) { return 0; }
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf) { return 0; }

	// Directory Functions
	virtual xerr_t AddChild(xstr_t name, XVirtualFile* file, direntry_t** out) { if (out) *out = 0; return "Not a directory!"; }
	virtual xerr_t RemoveChild(xstr_t name) { return "Not a directory!"; }
	virtual xerr_t FindChild(xstr_t name, direntry_t** out) { if (out) *out = 0; return "Not a directory!"; }
	virtual uint32_t ChildCount() { return 0; }
	virtual xerr_t GetChild(uint32_t index, direntry_t** out) { if (out) *out = 0; return "Not a directory!"; }

};


// Basic virtual file
class XVFSFile : public XBaseVirtualFile
{
public:
	XVFSFile();

	virtual uint32_t Version() { return m_version; }
	virtual epoch_t AccessTime() { return m_accesstime; }
	virtual epoch_t ModifyTime() { return m_modifytime; }
	virtual ftype_t Type() { return X9P_FT_FILE; }

	// IO functions
	virtual size_t Length() { return m_buf.size(); }
	virtual bool Locked();
	virtual void Open(openmode_t mode);
	virtual void Close();
	virtual uint32_t Read(uint64_t offset, uint32_t count, uint8_t* buf);
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf);

private:

	std::vector<uint8_t> m_buf;
	uint32_t m_version;
	epoch_t m_accesstime;
	epoch_t m_modifytime;
//	bool m_locked;
	openmode_t m_mode;

};


// Basic virtual directory
class XVFSDirectory : public XBaseVirtualFile
{
public:

	XVFSDirectory();

	virtual uint32_t Version()    { return m_version; }
	virtual epoch_t  AccessTime() { return m_accesstime; }
	virtual epoch_t  ModifyTime() { return m_modifytime; }
	virtual ftype_t  Type()       { return X9P_FT_DIRECTORY; }

	// Directory Functions
	virtual xerr_t AddChild(xstr_t name, XVirtualFile* file, direntry_t** out);
	virtual xerr_t RemoveChild(xstr_t name);
	virtual xerr_t FindChild(xstr_t name, direntry_t** out);
	virtual uint32_t ChildCount();
	virtual xerr_t GetChild(uint32_t index, direntry_t** out);

private:
	std::unordered_map<xstr_t, direntry_t, xstrhash_t, xstrequality_t> m_children;
	uint32_t m_version;
	epoch_t m_accesstime;
	epoch_t m_modifytime;
};

qid_t filenodeqid(XVirtualFile* node);



// Utils

// File that directly reads from and writes to a struct
template<typename T>
class XVFSTypePointerFile : public XBaseVirtualFile
{
public:
	XVFSTypePointerFile(T* data) : m_data(data) {}

	virtual ftype_t Type() { return X9P_FT_FILE; }


	// IO functions
	virtual size_t Length() { return sizeof(T); }
	virtual bool Locked() { return false; }
	virtual void Open(openmode_t mode) {}
	virtual void Close() {}

	virtual uint32_t Read(uint64_t offset, uint32_t count, uint8_t* buf)
	{
		if (offset >= sizeof(T))
			return 0;
		uint32_t sz = offset + count;
		if (sz > sizeof(T))
			sz = sizeof(T);
		sz -= offset;
		if(buf)
			memcpy(buf, reinterpret_cast<char*>(m_data) + offset, sz);
		return sz;
	}
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf)
	{
		if (offset >= sizeof(T))
			return 0;
		uint32_t sz = offset + count;
		if (sz > sizeof(T))
			sz = sizeof(T);
		sz -= offset;
		memcpy(reinterpret_cast<char*>(m_data) + offset, buf, sz);
		return sz;
	}

private:
	T* m_data;
};