#pragma once

#include "XBaseFileSystem.h"
#include <stdint.h>
#include <stddef.h>
#include <unordered_map>

class XApiFSNode;
class XApiFSDir;
struct XApiFSHandle
{
	xhnd        hnd;

	openmode_t  openmode;
	// TODO: Propagate permissions as we walk around
	//dirmode_t   permissions; 

	uint64_t    qidpath;
	XApiFSNode* node;
	XApiFSDir*  parent;

	inline void copyfrom(XApiFSHandle* h) {
		qidpath = h->qidpath;
		node    = h->node;
		parent  = h->parent;
	}
};


// FIXME: We shouldn't need this...
class XApiFSNode;
struct fsapisetcaller
{
	// Store the current caller and apply the new caller
	fsapisetcaller(XApiFSHandle* de, XApiFSNode* fnode) 
		: m_fnode(fnode),
		  m_lastcaller(s_caller)
	{
		s_caller = de;
	}

	fsapisetcaller(XApiFSHandle* de) : fsapisetcaller(de, de->node) {}

	
	// Restore the old caller
	~fsapisetcaller()
	{
		s_caller = m_lastcaller;
	}

	XApiFSNode* const m_fnode;
	XApiFSHandle* const m_lastcaller;


	// FIXME: This is so dumb
	static XApiFSHandle* s_caller;
};


class XApiFSNode
{
public:

	// This will be called before any other function gets called
	virtual XApiFSHandle* GetCaller() { return fsapisetcaller::s_caller; }

	// File stats
	virtual size_t    Name(xstr_t out, size_t avail) = 0;
	virtual qidtype_t Type()       = 0;
	virtual uint32_t  Version()    { return 0; }
	virtual epoch_t   AccessTime() { return 0; }
	virtual epoch_t   ModifyTime() { return 0; }
	virtual uint64_t  Length()     { return 0; }

	// Permissions
	virtual xstr_t    OwnerUser()        { return XSTRL("OwnerUser");  }
	virtual xstr_t    OwnerGroup()       { return XSTRL("OwnerGroup"); }
	virtual xstr_t    LastUserToModify() { return XSTRL("LastUser");   }
	virtual dirmode_t Permissions()      { return X9P_DM_PERM_MASK;    }

	virtual void Open(openmode_t mode);
	virtual void Clunk();
	// Could we open if we wanted to?
	virtual bool CanOpen(openmode_t mode);


	// Util
	uint64_t  Path()
	{
		XApiFSHandle* ndh = GetCaller();
		return ndh->qidpath;
	}
	inline qid_t QID() { return { Type(), Version(), Path() }; }


	// X9P Extension
	virtual bool CanHook() { return false; }
	virtual xerr_t Hook() { return XERR_UNSUPPORTED; }
};


// Base of all file nodes
class XApiFSFile : public XApiFSNode
{
public:
	virtual qidtype_t Type() { return X9P_QT_FILE; }

	// IO Functions
	virtual xerr_t Read  (uint64_t offset, uint32_t count, void* buf, uint32_t& result) = 0;
	virtual xerr_t Write (uint64_t offset, uint32_t count, void* buf, uint32_t& result) = 0;
};

// Base of all directory nodes
class XApiFSDir : public XApiFSNode
{
public:
	virtual qidtype_t Type() { return X9P_QT_DIR; }

	// Directory Functions
	virtual xerr_t Create(xstr_t name, dirmode_t mode, XApiFSHandle* out) { return XERR_UNSUPPORTED; }
	virtual xerr_t Remove(XApiFSHandle* ndh) { return XERR_UNSUPPORTED; }
	virtual uint32_t ChildCount() = 0;
	virtual xerr_t FindChild(xstr_t name, XApiFSHandle* out) = 0;
	virtual xerr_t GetChild(uint32_t index, XApiFSHandle* out) = 0;
};



struct XApiFSAuthData
{
	xhid hidserial;
	std::unordered_map<xhid, XApiFSHandle*> handles;
	std::vector<xhid> hooks;
};

class XApiFileSystem : public X9PFileSystem
{
public:
	XApiFileSystem(XApiFSDir* root);
	


	// 9P RPC calls
	virtual void Tattach (xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback) override;
	virtual void Twalk   (xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback)      override;
	virtual void Topen   (xhnd hnd, openmode_t mode, Ropen_fn callback)                                 override;
	virtual void Tcreate (xhnd hnd, xstr_t name, dirmode_t perm, openmode_t mode, Rcreate_fn callback)  override;
	virtual void Tread   (xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)                 override;
	virtual void Twrite  (xhnd hnd, uint64_t offset, uint32_t count, void* data, Rwrite_fn callback)    override;
	virtual void Tclunk  (xhnd hnd, Rclunk_fn callback)                                                 override;
	virtual void Tremove (xhnd hnd, Rremove_fn callback)                                                override;
	virtual void Tstat   (xhnd hnd, Rstat_fn callback)                                                  override;
	virtual void Twstat  (xhnd hnd, stat_t* stat, Rwstat_fn callback)                                   override;
	
	virtual void Disconnect(XAuth* auth)                                                                override;


	virtual void NewConnection(XAuth* auth) {};

private:

	XApiFSHandle* GetHnd(xhnd hnd);
	XApiFSHandle* TagHnd(xhnd hnd);
	void UntagHnd(xhnd hnd);

	std::unordered_map<XAuth*, XApiFSAuthData> m_userhandles;

	XApiFSDir* m_rootdir;
};



