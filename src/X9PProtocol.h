#pragma once
#include <stdint.h>
#include "XErrors.h"

#define X9P_VERSION "9P2000"
#define X9P_VERSION_LEN (sizeof(X9P_VERSION) - 1)

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


// First byte is the same as a QID_TYPE
typedef uint32_t dirmode_t;
enum : dirmode_t
{
	X9P_DM_DIR    = 0x80000000,	// Directory
	X9P_DM_APPEND = 0x40000000,	// Append only
	X9P_DM_EXCL   = 0x20000000,	// Exclusive use
	X9P_DM_MOUNT  = 0x10000000,	// Mounted channel -- Unused?
	X9P_DM_AUTH   = 0x08000000,	// Authentication file
	X9P_DM_TMP    = 0x04000000,	// Temporary/not-backed-up file  
	X9P_DM_READ   = 0x00000004,	// Can Read?
	X9P_DM_WRITE  = 0x00000002,	// Can Write?
	X9P_DM_EXEC   = 0x00000001,	// Can Execute?

	X9P_DM_PERM_MASK = X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC,
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
	X9P_OPEN_EX_INVALID = 4,
};


inline dirmode_t X9POpenModeToDirMode(openmode_t mode)
{
	const dirmode_t dm[] = {
		X9P_DM_READ, // X9P_OPEN_READ
		X9P_DM_WRITE, // X9P_OPEN_WRITE
		X9P_DM_READ | X9P_DM_WRITE, // X9P_OPEN_READWRITE
		X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC, // X9P_OPEN_EXEC
	};

	return dm[mode & X9P_OPEN_STATE_MASK];
}




#include "xstring.h"

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
	X9P_TVERSION = 100,
	X9P_RVERSION = 101,
	X9P_TAUTH = 102,
	X9P_RAUTH = 103,
	X9P_TATTACH = 104,
	X9P_RATTACH = 105,
	//X9P_TERROR = 106,
	X9P_RERROR = 107,
	X9P_TFLUSH = 108,
	X9P_RFLUSH = 109,
	X9P_TWALK = 110,
	X9P_RWALK = 111,
	X9P_TOPEN = 112,
	X9P_ROPEN = 113,
	X9P_TCREATE = 114,
	X9P_RCREATE = 115,
	X9P_TREAD = 116,
	X9P_RREAD = 117,
	X9P_TWRITE = 118,
	X9P_RWRITE = 119,
	X9P_TCLUNK = 120,
	X9P_RCLUNK = 121,
	X9P_TREMOVE = 122,
	X9P_RREMOVE = 123,
	X9P_TSTAT = 124,
	X9P_RSTAT = 125,
	X9P_TWSTAT = 126,
	X9P_RWSTAT = 127,
};

// Message names
inline const char* X9PMessageTypeName(uint8_t type)
{
	static const char* s_messageTypeNames[] =
	{
		"Tversion",
		"Rversion",
		"Tauth",
		"Rauth",
		"Tattach",
		"Rattach",
		"Terror",
		"Rerror",
		"Tflush",
		"Rflush",
		"Twalk",
		"Rwalk",
		"Topen",
		"Ropen",
		"Tcreate",
		"Rcreate",
		"Tread",
		"Rread",
		"Twrite",
		"Rwrite",
		"Tclunk",
		"Rclunk",
		"Tremove",
		"Rremove",
		"Tstat",
		"Rstat",
		"Twstat",
		"Rwstat",
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
	char _unused0[6]; // type & dev

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
	dirmode_t* perm() { return reinterpret_cast<dirmode_t*>(name()->str() + name()->len); }
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


