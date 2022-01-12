#pragma once

#include "X9PMessages.h"
#include <stdint.h>
#include <stddef.h>

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

struct fileops_t;

// File information
struct filenode_t
{
	uint32_t nid;   // File Node ID
	uid_t uid;      // Owning User
	uid_t gid;      // Owning Group
	uid_t muid;     // Last user to modify

	epoch_t atime;  // Access time
	epoch_t mtime;  // Modification time

	ftype_t type;   // File Type

	void* data;     // File Userdata
	fileops_t* ops; // File Operations
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

typedef const char* foperr_t;

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
struct walkret_t // Rwalk
{
	direntry_t* walked;

	uint16_t nwqid;
	filenode_t** wqid;
};



// PG 285

// Virtual table of functions implemented by filesystems
struct fileops_t
{
	foperr_t(*walk)(fauth_t* auth, filenode_t* fnode, walkparam_t* param, walkret_t* ret);
	foperr_t(*open)(fauth_t* auth, filenode_t* fnode, openmode_t mode, uint64_t* size);
	foperr_t(*create)(fauth_t* auth, filenode_t* dir, xstr_t name, dirmode_t perm, openmode_t mode, direntry_t** ret);
	foperr_t(*read)(fauth_t* auth, filenode_t* fnode, fileio_t* fio);
	foperr_t(*write)(fauth_t* auth, filenode_t* fnode, fileio_t* fio);
	foperr_t(*remove)(fauth_t* auth, direntry_t* dentry);//filenode_t* dir, filenode_t* file);
	foperr_t(*stat)(fauth_t* auth, filenode_t* fnode, filestats_t* stats);
	foperr_t(*wstat)(fauth_t* auth, filenode_t* fnode, filestats_t* stats);
	
	// Non 9P RPC calls
	uint32_t(*fileversion)(filenode_t* fnode);
	
	foperr_t(*rename)(fauth_t* auth, direntry_t* entry, xstr_t name);
};


qid_t filenodeqid(filenode_t* node);
uint32_t filenodenewid();

direntry_t* virt_rootdirentry();
void virt_init();
