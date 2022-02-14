#pragma once




enum XOPEN_FLAG
{
	XO_RDONLY = 0,    // Read Only
	XO_WRONLY = 1,    // Write Only
	XO_RDWR   = 2,    // Read and Write 
	XO_NDELAY = 4,    // Open without blocking 
	XO_APPEND = 8,    // Append on write
	XO_CREAT  = 512,  // Create on open
	XO_TRUNC  = 1024, // ?
	XO_EXCL   = 2048, // Error if the file exists on create
};

struct xaiocb
{
	int aio_fildes;
	off_t aio_offset;
	void* aio_buf;
	size_t aio_nbytes;
	// MISSING FIELDS
};

typedef void xfd_t;

xfd_t* xopen(const char* path, enum XOPEN_FLAG oflag);
void xclose(xfd_t* fd);
int xwrite(xfd_t* fd, const void* buf, size_t nbytes);
int xread(xfd_t* fd, void* buf, size_t nbytes);



