#pragma once

#include "X9PFileSystem.h"
#include "X9PMessages.h"
#include <stdint.h>
#include <stddef.h>
#include <unordered_map>

// Virtual filesystem
// Exists on both ends of a 9P connection
// Somewhat like vnodes and vnodeops but with 9P messages
// <3 BSD docs
//
//
// FIDs are directory entries
// QIDs are filenodes
//

// User ID
typedef uint32_t uid_t;

// Group ID
typedef uint32_t gid_t;

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
	
	virtual size_t Length() = 0;

	// Permissions
	virtual const xstr_t OwnerUser() = 0;
	virtual const xstr_t OwnerGroup() = 0;
	virtual const xstr_t LastUserToModify() = 0;

	// IO Functions
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

///////////////////
// Virtual Files //
///////////////////





qid_t filenodeqid(XVirtualFile* node);
uint32_t filenodenewid();


class XVirtualFileSystem : public X9PFileSystem
{
public:
	XVirtualFileSystem();

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

	std::unordered_map<xhnd, direntry_t*> m_handles;
	xhnd m_hndserial;
};

