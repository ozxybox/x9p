#pragma once
#include <stdint.h>


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

typedef const char* xerr_t;

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
};
