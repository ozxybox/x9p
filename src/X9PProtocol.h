#pragma once
#include <stdint.h>
#include "XErrors.h"

// The Linux Kernel's 9P implementation, v9fs, seems to deviate a bit from the spec
// Any protocol compat for it should be described with this
#define X9P_V9FS_COMPAT 1

//
// NOTICE: An important goal of this library is to be 100% compatible with 9P2000.
//         There is ONE extension to the protocol though.
//         This extension DOES NOT change the protocol's format, but DOES change SERVER behavior.
// 
//         If a file has the type MOUNT, it can be hooked.
//         If a client opens a MOUNT file with "EXEC", the file is hooked until the FID is clunked.
//         Only after the server responds with a valid Ropen, is it allowed to send "Twrite" messages to the client.
//         These messages will correspond with the hooked FID and use the tag NOTAG.
//         The client IS NOT allowed respond to the server with a "Rwrite" message.
//         
//         The server should still allow the client to Tread the content of a would be hook without the client hooking.
//         This is to maintain functionality of a 9P client that lacks this extension.
//
//         This feature could be accomplished by negotiating hooks between the server and the client, and the server connecting to a client hosted hook server.
//         However this is more complexity and data the server should not have to handle. 
//         The extension covers this problem and covers it well enough.
//
//
// NOTICE: 9P2000 currently uses 32 bit Unix time. 
//         This will overflow in 2038, but I would be surprised if I'm still using this code by then.
//         If we ever get to 2030 or so, epoch_t is around to replace epoch32_t.
//
//



#define X9P_VERSION "9P2000"
#define X9P_VERSION_LEN (sizeof(X9P_VERSION) - 1)

#define X9P_MSIZE_DEFAULT 8192


// Unix Time
typedef uint64_t epoch_t;

// Unix Time - 32 bit
typedef uint32_t epoch32_t;

// FIDs, client specified IDs to Directory Entries
typedef uint32_t fid_t;

// Unique message identifying tags
typedef uint16_t mtag_t;


typedef uint8_t qidtype_t;
enum : qidtype_t
{
	X9P_QT_DIR    = 0b10000000, // Directory
	X9P_QT_APPEND = 0b01000000, // Append only
	X9P_QT_EXCL   = 0b00100000, // Exclusive use
	X9P_QT_MOUNT  = 0b00010000, // Mounted channel -- Unused?
	X9P_QT_AUTH   = 0b00001000, // Authentication file
	X9P_QT_TMP    = 0b00000100, // Temporary/not-backed-up file 
	X9P_QT_FILE   = 0b00000000, // File
};


typedef uint32_t dirmode_t;
enum : dirmode_t
{
	// High byte is the same as qidtype_t
	X9P_DM_DIR    = 0x80000000,	// Directory
	X9P_DM_APPEND = 0x40000000,	// Append only
	X9P_DM_EXCL   = 0x20000000,	// Exclusive use
	X9P_DM_MOUNT  = 0x10000000,	// Mounted channel -- Unused?
	X9P_DM_AUTH   = 0x08000000,	// Authentication file
	X9P_DM_TMP    = 0x04000000,	// Temporary/not-backed-up file  
	
	// A user that is the file's owner checks "other", "group", and "other"
	X9P_DM_OWNER_READ   = 0b100000000,	// Can the owner read?
	X9P_DM_OWNER_WRITE  = 0b010000000,	// Can the owner write?
	X9P_DM_OWNER_EXEC   = 0b001000000,	// Can the owner execute?

	// A user from the file's group checks both "other" and "group"
	X9P_DM_GROUP_READ   = 0b000100000,	// Can the group read?
	X9P_DM_GROUP_WRITE  = 0b000010000,	// Can the group write?
	X9P_DM_GROUP_EXEC   = 0b000001000,	// Can the group execute?
	
	// A user unrelated to the file checks only "other"
	X9P_DM_OTHER_READ   = 0b000000100,	// Can read?
	X9P_DM_OTHER_WRITE  = 0b000000010,	// Can write?
	X9P_DM_OTHER_EXEC   = 0b000000001,	// Can execute?

	// Aggregated permissions
	X9P_DM_READ  = X9P_DM_OWNER_READ  | X9P_DM_GROUP_READ  | X9P_DM_OTHER_READ,
	X9P_DM_WRITE = X9P_DM_OWNER_WRITE | X9P_DM_GROUP_WRITE | X9P_DM_OTHER_WRITE,
	X9P_DM_EXEC  = X9P_DM_OWNER_EXEC  | X9P_DM_GROUP_EXEC  | X9P_DM_OTHER_EXEC,

	// Masks
	X9P_DM_MASK_OWNER = X9P_DM_OWNER_READ | X9P_DM_OWNER_WRITE | X9P_DM_OWNER_EXEC,
	X9P_DM_MASK_GROUP = X9P_DM_GROUP_READ | X9P_DM_GROUP_WRITE | X9P_DM_GROUP_EXEC,
	X9P_DM_MASK_OTHER = X9P_DM_OTHER_READ | X9P_DM_OTHER_WRITE | X9P_DM_OTHER_EXEC,

	X9P_DM_PERM_MASK = X9P_DM_MASK_OWNER | X9P_DM_MASK_GROUP | X9P_DM_MASK_OTHER,
	X9P_DM_PROTOCOL_MASK = X9P_DM_PERM_MASK | X9P_DM_DIR | X9P_DM_APPEND | X9P_DM_EXCL | X9P_DM_MOUNT | X9P_DM_AUTH | X9P_DM_TMP
};

typedef uint8_t openmode_t;
enum : openmode_t
{
	X9P_OPEN_READ,
	X9P_OPEN_WRITE,
	X9P_OPEN_READWRITE,
	X9P_OPEN_EXEC,
	X9P_OPEN_TRUNC = 0x10,
	X9P_OPEN_CLOSE = 0x40,

	X9P_OPEN_STATE_MASK = X9P_OPEN_READ | X9P_OPEN_WRITE | X9P_OPEN_READWRITE | X9P_OPEN_EXEC,
	X9P_OPEN_PROTOCOL_MASK = X9P_OPEN_STATE_MASK | X9P_OPEN_TRUNC | X9P_OPEN_CLOSE,

	// Bits not used by by 9P
	X9P_OPEN_EX_INVALID = 128,
};


inline dirmode_t X9POpenModeToDirMode(openmode_t mode)
{
	const dirmode_t dm[] = {
		X9P_DM_READ,                              // X9P_OPEN_READ
		              X9P_DM_WRITE,               // X9P_OPEN_WRITE
		X9P_DM_READ | X9P_DM_WRITE,               // X9P_OPEN_READWRITE
		X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC, // X9P_OPEN_EXEC
	};

	return dm[mode & X9P_OPEN_STATE_MASK];
}

inline qidtype_t X9PDirModeToQIDType(dirmode_t mode)
{
	return reinterpret_cast<qidtype_t*>(&mode)[3];
}



#include "xstring.h"


// FIXME: We should probably just scrap this funny compiler thing and marshall the data ourselves on recv/send


// We need these structs packed with no padding
#ifdef _MSC_VER
#pragma pack(push, 1)
#elif defined(__GNUC__)
// lol
#define struct struct __attribute__((packed))
#else
#error UNSUPPORTED_PLATFORM
#endif


// No tag attached to this message
#define NOTAG (0xFFFF)
#define NOFID (0xFFFFFFFF)


// Message types
enum X9P_MESSAGE
{
	X9P_TVERSION = 100, X9P_RVERSION,
	X9P_TAUTH    = 102, X9P_RAUTH,
	X9P_TATTACH  = 104, X9P_RATTACH,
	X9P_TFLUSH   = 108, X9P_RFLUSH,
	X9P_TWALK    = 110, X9P_RWALK,
	X9P_TOPEN    = 112, X9P_ROPEN,
	X9P_TCREATE  = 114, X9P_RCREATE,
	X9P_TREAD    = 116, X9P_RREAD,
	X9P_TWRITE   = 118, X9P_RWRITE,
	X9P_TCLUNK   = 120, X9P_RCLUNK,
	X9P_TREMOVE  = 122, X9P_RREMOVE,
	X9P_TSTAT    = 124, X9P_RSTAT,
	X9P_TWSTAT   = 126, X9P_RWSTAT,

  //X9P_TERROR = 106,
	X9P_RERROR = 107,
};

// Message names
inline const char* X9PMessageTypeName(uint8_t type)
{
	static const char* s_messageTypeNames[] =
	{
		"Tversion", "Rversion",
		"Tauth",    "Rauth",
		"Tattach",  "Rattach",
		"Terror",   "Rerror",
		"Tflush",   "Rflush",
		"Twalk",    "Rwalk",
		"Topen",    "Ropen",
		"Tcreate",  "Rcreate",
		"Tread",    "Rread",
		"Twrite",   "Rwrite",
		"Tclunk",   "Rclunk",
		"Tremove",  "Rremove",
		"Tstat",    "Rstat",
		"Twstat",   "Rwstat",
	};

	return s_messageTypeNames[type - X9P_TVERSION];
}


struct qid_t
{
	qidtype_t type;
	uint32_t version;
	uint64_t path;
};

struct stat_t
{
	uint16_t size;    // Total bytes used by following data

	//uint16_t type;  // For kernel use
	//uint32_t dev;   // For kernel use
	char _unused[6];  // type & dev

	qid_t qid;        // File QID

	dirmode_t mode;   // Permissions and flags
	epoch32_t atime;  // Access time (unix time)
	epoch32_t mtime;  // Modification time (unix time)
	uint64_t length;  // File Length in bytes

	// File name
	xstr_t name() { return reinterpret_cast<xstr_t>(this + 1); }
	// Owner name
	xstr_t uid()  { return xstrnext(name()); }
	// Group name
	xstr_t gid()  { return xstrnext(uid()); }
	// Name of the last modifying user
	xstr_t muid() { return xstrnext(gid()); }
};


//////////////////////////
// Base of all messages //
//////////////////////////
struct message_t
{
	uint32_t size; // message length
	uint8_t  type; // message type
	mtag_t   tag;  // unique message tag
};

/////////////////////////
// Tversion & Rversion //
/////////////////////////
struct Tversion_t : public message_t
{
	// tag must be NOTAG
	uint32_t msize; // suggested size
	xstr_t version() { return reinterpret_cast<xstr_t>(this + 1); }
};
typedef Tversion_t Rversion_t;

///////////////////////
// Tattach & Rattach //
///////////////////////
struct Tattach_t : public message_t
{
	fid_t fid;
	fid_t afid;

	xstr_t uname() { return reinterpret_cast<xstr_t>(this + 1); }
	xstr_t aname() { return xstrnext(uname()); }
};
struct Rattach_t : public message_t
{
	qid_t qid;
};

///////////////////
// Tstat & Rstat //
///////////////////
struct Tstat_t : public message_t
{
	fid_t fid; // Get info on this fid
};
struct Rstat_t : public message_t
{
	uint16_t n;
	stat_t stat;
};

/////////////////////
// Twstat & Rwstat //
/////////////////////
struct Twstat_t : public message_t
{
	fid_t fid; // Get info on this fid
	uint16_t n;
	stat_t stat;
};
typedef message_t Rwstat_t;

///////////////////
// Topen & Ropen //
///////////////////
struct Topen_t : public message_t
{
	fid_t fid;
	openmode_t mode;
};
struct Ropen_t : public message_t
{
	qid_t qid;
	uint32_t iounit;
};

///////////////////////
// Tcreate & Rcreate //
///////////////////////
struct Tcreate_t : public message_t
{
	fid_t fid;
	xstr_t name() { return reinterpret_cast<xstr_t>(this + 1); }
	dirmode_t* perm() { return reinterpret_cast<dirmode_t*>(xstrnext(name())); }
	openmode_t* mode() { return reinterpret_cast<openmode_t*>(perm() + 1); };
};
struct Rcreate_t : public message_t
{
	qid_t qid;
	uint32_t iounit;
};


///////////////////
// Tread & Rread //
///////////////////
struct Tread_t : public message_t
{
	fid_t fid;
	uint64_t offset;
	uint32_t count;
};
struct Rread_t : public message_t
{
	uint32_t count;
	unsigned char* data() { return reinterpret_cast<unsigned char*>(this + 1); }
};

/////////////////////
// Twrite & Rwrite //
/////////////////////
struct Twrite_t : public message_t
{
	fid_t fid;
	uint64_t offset;
	uint32_t count;
	unsigned char* data() { return reinterpret_cast<unsigned char*>(this + 1); }
};
struct Rwrite_t : public message_t
{
	uint32_t count;
};

///////////////////
// Tauth & Rauth //
///////////////////
struct Tauth_t : public message_t
{
	fid_t afid;

	xstr_t uname() { return reinterpret_cast<xstr_t>(this + 1); }
	xstr_t aname() { return xstrnext(uname()); }
};
struct Rauth_t : public message_t
{
	qid_t aqid;
};

///////////////////
// Tauth & Rauth //
///////////////////
struct Tflush_t : public message_t
{
	uint16_t oldtag;
};
typedef message_t Rflush_t;

/////////////////////
// Tclunk & Rclunk //
/////////////////////
struct Tclunk_t : public message_t
{
	fid_t fid;
};
typedef message_t Rclunk_t;

///////////////////////
// Tremove & Rremove //
///////////////////////
struct Tremove_t : public message_t
{
	fid_t fid;
};
typedef message_t Rremove_t;

///////////////////
// Twalk & Rwalk //
///////////////////
#define X9P_TWALK_MAXELEM 16
struct Twalk_t : public message_t
{
	fid_t fid;
	fid_t newfid;

	uint16_t nwname;
	
	// wname[nwname]
	xstr_t wname() { return reinterpret_cast<xstr_t>(this + 1); }
};
struct Rwalk_t : public message_t
{
	uint16_t nwqid;

	// wqid[nwqid]
	qid_t* wqid() { return reinterpret_cast<qid_t*>(this + 1); }
};

////////////
// Rerror //
////////////
struct Rerror_t : public message_t
{
	xstr_t ename() { return reinterpret_cast<xstr_t>(this + 1); }
};




#ifdef _MSC_VER
#pragma pack(pop)
#elif defined(__GNUC__)
// lol
#undef struct
#else
#error UNSUPPORTED_PLATFORM
#endif
// END OF PACKED STRUCTS


