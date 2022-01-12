#pragma once

#include "X9P.h"
#include "X9PFid.h"
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


struct message_t
{
	uint32_t size; // message length
	uint8_t  type; // message type
	uint16_t tag;  // unique message tag
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
	uint32_t* perm() { return reinterpret_cast<uint32_t*>(name()->str() + name()->len); }
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