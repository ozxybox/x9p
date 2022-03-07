#pragma once

#include "XVirtualFileSystem.h"



// auth correlates with a FS made on attach
// the xhnd's are dished out by the new fs
class XBaseApiFileSystem : public XBaseFileSystem<xhnd>
{
public:

	virtual xhnd NullEntry();


	virtual X9PFileSystem* CreateUserFS(XAuth* user) = 0;
	//virtual void ReleaseUserFS(XAuth* user, X9PFileSystem* fs) = 0;

	bool GetTrueHND(xhnd hnd, X9PFileSystem*& fsOut, xhnd& hndOut);


	virtual xhnd NewFileHandle(XAuth* user);
	virtual xhnd DeriveFileHandle(xhnd hnd);
	virtual void ReleaseFileHandle(XAuth* user, xhnd hnd);



	virtual void Tattach(xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback);
	virtual void Twalk(xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback);
	virtual void Topen(xhnd hnd, openmode_t mode, Ropen_fn callback);
	virtual void Tcreate(xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback);
	virtual void Tread(xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback);
	virtual void Twrite(xhnd hnd, uint64_t offset, uint32_t count, uint8_t* data, Rwrite_fn callback);
	virtual void Tclunk(xhnd hnd, Rclunk_fn callback);
	virtual void Tremove(xhnd hnd, Rremove_fn callback);
	virtual void Tstat(xhnd hnd, Rstat_fn callback);
	virtual void Twstat(xhnd hnd, stat_t* stat, Rwstat_fn callback);

protected:

	std::unordered_map<XAuth*, X9PFileSystem*> m_userfilesystems;
};



class XApiFSClass: public XVirtualFile
{
public:
	virtual ftype_t Type() { return X9P_FT_DIRECTORY; }

	virtual xerr_t FindChild(xstr_t name, direntry_t** out);
	virtual uint32_t ChildCount();
	virtual xerr_t GetChild(uint32_t index, direntry_t** out);


	// Stubs
	virtual uint32_t Version() { return 0; }
	virtual epoch_t AccessTime() { return 0; }
	virtual epoch_t ModifyTime() { return 0; }
	virtual const xstr_t OwnerUser() { return (xstr_t)"\x00\x00"; }
	virtual const xstr_t OwnerGroup() { return (xstr_t)"\x00\x00"; }
	virtual const xstr_t LastUserToModify() { return (xstr_t)"\x00\x00"; }
	virtual size_t Length() { return 0; }
	virtual bool Locked() { return false; }
	virtual void Open(openmode_t mode) {}
	virtual void Close() {}
	virtual uint32_t Read(uint64_t offset, uint32_t count, uint8_t* buf) { return 0; }
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf) { return 0; }
	virtual xerr_t AddChild(xstr_t name, XVirtualFile* file, direntry_t** out) { return XERR_DIR_NO_ADD; }
	virtual xerr_t RemoveChild(xstr_t name) { return XERR_DIR_NO_REMOVE; }
};

template<typename Ret, typename Arg>
class XApiFSMethod : public XVirtualFile
{
public:

	typedef Ret(*api_func)(Arg);

	XApiFSMethod(api_func method)
	{

	}

	virtual ftype_t Type() { return X9P_FT_APPEND; }

	virtual size_t Length() {}
	virtual bool Locked();
	virtual void Open(openmode_t mode);
	virtual void Close();
	virtual uint32_t Read(uint64_t offset, uint32_t count, uint8_t* buf);
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf);
	
	// Stubs
	virtual uint32_t Version() { return 0; }
	virtual epoch_t AccessTime() { return 0; }
	virtual epoch_t ModifyTime() { return 0; }
	virtual const xstr_t OwnerUser() { return (xstr_t)"\x00\x00"; }
	virtual const xstr_t OwnerGroup() { return (xstr_t)"\x00\x00"; }
	virtual const xstr_t LastUserToModify() { return (xstr_t)"\x00\x00"; }
	virtual xerr_t AddChild(xstr_t name, XVirtualFile* file, direntry_t** out) { return XERR_NOT_DIR; }
	virtual xerr_t RemoveChild(xstr_t name) { return XERR_NOT_DIR; }
	virtual xerr_t FindChild(xstr_t name, direntry_t** out) { return XERR_NOT_DIR; }
	virtual uint32_t ChildCount() { return 0; }
	virtual xerr_t GetChild(uint32_t index, direntry_t** out) { return XERR_NOT_DIR; }



};


class XApiFSList : public XVirtualFile
{
public:

};


/*


"data/class/method"

single argument
just a struct
- means we can abstract stuff so it's fine






*/