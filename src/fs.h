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


// User auth info for acting out file ops
struct fauth_t
{
	uid_t user;
	gid_t group;
	// TODO: Connection info here
};


// File information
struct filenode_t
{
	uint32_t nid;    // File Node ID
	uid_t uid;       // Owning User
	uid_t gid;       // Owning Group
	uid_t muid;      // Last user to modify

	epoch_t atime;   // Access time
	epoch_t mtime;   // Modification time

	ftype_t type;    // File Type

	void* data;      // File Userdata
	X9PFileSystem* fs; // File Operations
};


struct direntry_t
{
	xstr_t name;
	//direntry_t* parent;
	filenode_t* parent;
	filenode_t* node;
};

///////////////////
// Virtual Files //
///////////////////


struct filestats_t
{
	const char* uid;  // Owning User
	const char* gid;  // Owning Group
	const char* muid; // Last user to modify

	epoch_t atime;    // Access time
	epoch_t mtime;    // Modification time

	dirmode_t mode;
	uint64_t size;

};

struct fileio_t
{
	unsigned char* data;
	uint64_t offset;
	uint32_t count;
	uint32_t result;
};

struct walkparam_t // Twalk
{
	uint16_t nwname;
	xstr_t wname; // This is an array! One string after the other!
};




qid_t filenodeqid(filenode_t* node);
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

	
	virtual uint32_t fileversion(filenode_t* fnode);

	bool isValidXHND(xhnd hnd, direntry_t*& out);

	std::unordered_map<xhnd, direntry_t*> m_handles;
	xhnd m_hndserial;
};