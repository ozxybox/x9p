#pragma once

typedef const char* xerr_t;

// Generic error strings

#define XERR_NOT_DIR "Not a directory!"
#define XERR_NOT_FILE "Not a file!"
#define XERR_XHND_INVALID "Invalid handle!"
#define XERR_DIR_NO_ADD "Cannot add file to directory!"
#define XERR_DIR_NO_REMOVE "Cannot remove file from directory!"
#define XERR_NO_PERM "You do not have permission for this request!"
#define XERR_ALREADY_OPEN "FID already open"

// Could not find the file while walking
#define XERR_FILE_DNE "Path does not exist"

// The file must be opened before any reads or writes are requested
#define XERR_OPEN_MODE "Must open first"

// The request's FID does not exist yet
#define XERR_FID_DNE "FID does not exist"

// Cannot walk with a newfid that is already assigned 
#define XERR_FID_USED "FID is already in use"

// Extra macros to resolve out the define :P
#define __XERR_WALK_TOO_FAR(maxwelem) "Cannot walk past " #maxwelem " elements!"
#define _XERR_WALK_TOO_FAR(maxwelem) __XERR_WALK_TOO_FAR(maxwelem) 
#define XERR_WALK_TOO_FAR _XERR_WALK_TOO_FAR(X9P_TWALK_MAXELEM) 

#define XERR_FILENAME_DOT "Creating a file with the name \".\" is not allowed!"
#define XERR_FILENAME_DOTDOT "Creating a file with the name \"..\" is not allowed!"

#define XERR_CANT_CREATE "Could not create in this directory!"

#define XERR_UNSUPPORTED "Unsupported request"

#define XERR_IOOR "Index out of range!"

#define XERR_PARTIAL_RW "Partial read/write is not allowed"