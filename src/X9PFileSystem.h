#pragma once

#include "X9PProtocol.h"
#include "XAuth.h"
#include "xstring.h"

#include <functional>



typedef std::function<void(xerr_t err, uint32_t msize, xstr_t version)> Rversion_fn;
typedef std::function<void(xerr_t err, qid_t* aqid                   )> Rauth_fn;
//typedef std::function<void(xerr_t err, xstr_t ename                  )> Rerror_fn;
typedef std::function<void(xerr_t err                                )> Rflush_fn;
typedef std::function<void(xerr_t err, qid_t* qid                    )> Rattach_fn;
typedef std::function<void(xerr_t err, uint16_t nwqid, qid_t* wqid   )> Rwalk_fn;
typedef std::function<void(xerr_t err, qid_t* qid, uint32_t iounit   )> Ropen_fn;
typedef std::function<void(xerr_t err, qid_t* qid, uint32_t iounit   )> Rcreate_fn;
typedef std::function<void(xerr_t err, uint32_t count, void* data    )> Rread_fn;
typedef std::function<void(xerr_t err, uint32_t count                )> Rwrite_fn;
typedef std::function<void(xerr_t err                                )> Rclunk_fn;
typedef std::function<void(xerr_t err                                )> Rremove_fn;
typedef std::function<void(xerr_t err, stat_t* stat                  )> Rstat_fn;
typedef std::function<void(xerr_t err                                )> Rwstat_fn;

typedef std::function<void(xerr_t err, uint64_t offset, uint32_t count, void* data)> Twrite_fn;


#define XHID_UNINITIALIZED (0xFFFFFFFFFFFFFFFF)
typedef uint64_t xhid;


class X9PFileSystem;
typedef struct xhnd_data
{
	fid_t fid;
	XAuth* auth;
	X9PFileSystem* fs;

	// TODO: rename this to hid, instead of just id
	xhid id;
} *xhnd;


class X9PFileSystem
{
public:

	// virtual void Tversion(uint32_t messagesize, xstr_t version, Rversion_fn callback) = 0;
	// Tauth
	// Tflush
	virtual void Tattach (xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback) = 0;
	virtual void Twalk   (xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback)      = 0;
	virtual void Topen   (xhnd hnd, openmode_t mode, Ropen_fn callback)                                 = 0;
	virtual void Tcreate (xhnd hnd, xstr_t name, dirmode_t perm, openmode_t mode, Rcreate_fn callback)  = 0;
	virtual void Tread   (xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)                 = 0;
	virtual void Twrite  (xhnd hnd, uint64_t offset, uint32_t count, void* data, Rwrite_fn callback)    = 0;
	virtual void Tclunk  (xhnd hnd, Rclunk_fn callback)                                                 = 0;
	virtual void Tremove (xhnd hnd, Rremove_fn callback)                                                = 0;
	virtual void Tstat   (xhnd hnd, Rstat_fn callback)                                                  = 0;
	virtual void Twstat  (xhnd hnd, stat_t* stat, Rwstat_fn callback)                                   = 0;


	virtual void Disconnect(XAuth* auth) {};
};


uint32_t X9PDefaultIOUnit(xhnd hnd);