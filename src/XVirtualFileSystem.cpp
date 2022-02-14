#include "XVirtualFileSystem.h"
#include "X9PFileSystem.h"
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <time.h>
#include <assert.h>

#define TEMP_IOUNIT 512


qid_t filenodeqid(XVirtualFile* node)
{
	qid_t qid;
	qid.path = (uint64_t)node;
	qid.version = node->Version();
	qid.type = 0;
	ftype_t type = node->Type();
	if (type == X9P_FT_DIRECTORY)
		qid.type |= X9P_QT_DIR;
	if (type == X9P_FT_APPEND)
		qid.type |= X9P_QT_APPEND;

	return qid;
}

size_t vfstat(direntry_t* de, stat_t* stats, uint32_t max)
{

	XVirtualFile* fnode = de->node;

	xstr_t user = fnode->OwnerUser();
	xstr_t group = fnode->OwnerGroup();
	xstr_t modifier = fnode->LastUserToModify();

	size_t sz = sizeof(stat_t) + de->name->size() + user->size() + group->size() + modifier->size();

	if (!stats) return sz;
	assert(sz <= max);

	stats->size = sz;

	// We should really just zero everything that might go over the network for security reasons
	memset(&stats->_unused0[0], 0, 6);

	stats->qid = filenodeqid(fnode);

	dirmode_t mode = X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC;
	if (fnode->Type() == X9P_FT_DIRECTORY)
		mode |= X9P_DM_DIR;

	stats->mode = mode;

	stats->atime = fnode->AccessTime();
	stats->mtime = fnode->ModifyTime();

	stats->length = fnode->Type() == X9P_FT_DIRECTORY ? 0 : fnode->Length();

	xstrcpy(stats->name(), de->name);
	xstrcpy(stats->uid(), user);
	xstrcpy(stats->gid(), group);
	xstrcpy(stats->muid(), modifier);

	return sz;
}




class XVFSFile : public XVirtualFile
{
public:
	XVFSFile()
	{
		m_version = 0;
		//m_locked = false;
		m_mode = 0;
		
		m_accesstime = time(0);
		m_modifytime = time(0);
	}

	virtual uint32_t Version() { return m_version; }
	virtual epoch_t AccessTime() { return m_accesstime; }
	virtual epoch_t ModifyTime() { return m_modifytime; }
	virtual ftype_t Type()
	{
		return X9P_FT_FILE;
	}
	virtual const xstr_t OwnerUser()        { return (xstr_t)"\x4\x0user"; }
	virtual const xstr_t OwnerGroup()       { return (xstr_t)"\x5\x0group"; }
	virtual const xstr_t LastUserToModify() { return (xstr_t)"\x9\x0otheruser"; }

	virtual size_t Length()
	{
		return m_buf.size();
	}

	// IO functions
	virtual bool Locked()
	{
		return false;
		//return m_locked;
	}
	virtual void Open(openmode_t mode)
	{
		//if (m_locked) return;

		m_mode = mode;
		//m_locked = true;
	}
	virtual void Close()
	{
		m_mode = 0;
		//m_locked = false;
	}

	virtual uint32_t Read(uint64_t offset, uint32_t count, uint8_t* buf)
	{
		//if ((m_mode & X9P_OPEN_WRITE) && !(m_mode & X9P_OPEN_READWRITE)) return 0;

		// Clamp the read within the bounds of the file
		uint64_t sz = offset + count;
		if (sz > m_buf.size())
			sz = m_buf.size();
		sz -= offset;

		// Only copy if we have a dest
		if (buf)
		{
			uint8_t* data = m_buf.data();
			memcpy(buf, data + offset, count);
		}

		m_accesstime = time(0);
		return (uint32_t)sz;
	}
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf)
	{
		//if (!(m_mode & X9P_OPEN_WRITE) && !(m_mode & X9P_OPEN_READWRITE)) return 0;

		// Only copy if we have a src
		if (buf)
		{
			// Append files only use an offset of the end of the file
			if (Type() & X9P_FT_APPEND)
				offset = m_buf.size();

			// Make sure we have enough room for this write
			uint64_t sz = offset + count;
			if (sz > m_buf.size())
				m_buf.resize(sz);

			// Copy it in
			uint8_t* data = m_buf.data();
			memcpy(data + offset, buf, count);


			m_version++;
		}
		m_modifytime = time(0);

		return count;
	}

	// As a file, we don't do directory ops

	virtual xerr_t AddChild(xstr_t name, XVirtualFile* file, direntry_t** out) { if (out) *out = 0; return "Not a directory!"; }
	virtual xerr_t RemoveChild(xstr_t name) { return "Not a directory!"; }
	virtual xerr_t FindChild(xstr_t name, direntry_t** out) { if(out) *out = 0; return "Not a directory!"; }

	virtual uint32_t ChildCount() { return 0; }
	virtual xerr_t GetChild(uint32_t index, direntry_t** out) { if (out) *out = 0; return "Not a directory!"; }
private:

	std::vector<uint8_t> m_buf;
	uint32_t m_version;
	epoch_t m_accesstime;
	epoch_t m_modifytime;
//	bool m_locked;
	openmode_t m_mode;

};

class XVFSDirectory : public XVirtualFile
{
public:

	XVFSDirectory()
	{
		m_version = 0;

		m_accesstime = time(0);
		m_modifytime = time(0);

		
		AddChild(XSTR_L("."), this, 0);
	}

	virtual uint32_t Version() { return m_version; }
	virtual epoch_t AccessTime() { return m_accesstime; }
	virtual epoch_t ModifyTime() { return m_modifytime; }
	virtual ftype_t Type() { return X9P_FT_DIRECTORY; }
	virtual const xstr_t OwnerUser()        { return (xstr_t)"\x4\x0user"; }
	virtual const xstr_t OwnerGroup()       { return (xstr_t)"\x5\x0group"; }
	virtual const xstr_t LastUserToModify() { return (xstr_t)"\x9\x0otheruser"; }

	// As a directory, we don't do file ops
	virtual size_t Length() { return 0; }
	virtual bool Locked() { return false; }
	virtual void Open(openmode_t mode) { }
	virtual void Close() { }
	virtual uint32_t Read(uint64_t offset, uint32_t count, uint8_t* buf) { return 0; }
	virtual uint32_t Write(uint64_t offset, uint32_t count, uint8_t* buf) { return 0; }

	// Directory Functions

	virtual xerr_t AddChild(xstr_t name, XVirtualFile* file, direntry_t** out)
	{
		auto f = m_children.find(name);
		if (f != m_children.end()) return "File already exists!";

		direntry_t de;
		de.name = xstrdup(name);
		de.parent = this;
		de.node = file;

		m_children.emplace(de.name, de);

		// FIXME: what?
		if (out)
			*out = &m_children[de.name];

		m_version++;
		m_modifytime = time(0);
		return 0;
	}
	virtual xerr_t RemoveChild(xstr_t name)
	{
		auto f = m_children.find(name);
		if (f == m_children.end()) return "File does not exist!";

		free(f->second.name);
		m_children.erase(f);

		m_version++;
		m_modifytime = time(0);
		return 0;
	}
	virtual xerr_t FindChild(xstr_t name, direntry_t** out)
	{
		auto f = m_children.find(name);
		if (f == m_children.end())
		{
			if(out) 
				*out = 0;
			return "File does not exist!";
		}

		if(out)
			*out = &f->second;

		m_accesstime = time(0);
		return 0;
	}

	virtual uint32_t ChildCount()
	{
		return m_children.size();
	}
	virtual xerr_t GetChild(uint32_t index, direntry_t** out)
	{
		if (index >= m_children.size())
			return "Invalid index!";

		if (out)
		{
			// FIXME: gross
			auto it = m_children.begin();
			std::advance(it, index);
			*out = &it->second;
		}

		return 0;
	}

private:
	std::unordered_map<xstr_t, direntry_t, xstrhash_t, xstrequality_t> m_children;
	uint32_t m_version;
	epoch_t m_accesstime;
	epoch_t m_modifytime;
};

static direntry_t s_rootdirentry;
static XVFSDirectory* s_rootnode;

direntry_t* virt_rootdirentry() { return &s_rootdirentry; }



XVirtualFileSystem::XVirtualFileSystem()
{
	m_hndserial = 0;


	// Root Directory
	s_rootnode = new XVFSDirectory();
	s_rootnode->AddChild(XSTR_L(".."), s_rootnode, 0);

	s_rootdirentry.name = xstrdup("/");
	s_rootdirentry.parent = s_rootnode;
	s_rootdirentry.node = s_rootnode;



	// Text file
	XVFSFile* txt = new XVFSFile();

	const char str[] = "Hello world!\n";
	txt->Open(X9P_OPEN_WRITE);
	txt->Write(0, sizeof(str), (uint8_t*)&str[0]);
	txt->Close();

	s_rootnode->AddChild(XSTR_L("hello.txt"), txt, 0);

	// Directory
	XVFSDirectory* folder = new XVFSDirectory();
	folder->AddChild(XSTR_L("hoopla.txt"), txt, 0);

	folder->AddChild(XSTR_L(".."), s_rootnode, 0);
	s_rootnode->AddChild(XSTR_L("files"), folder, 0);

}

bool XVirtualFileSystem::isValidXHND(xhnd hnd, direntry_t*& out)
{
	auto f = m_handles.find(hnd);
	if (f == m_handles.end())
	{
		out = 0;
		return false;
	}
	out = f->second;
	return true;
}

xhnd XVirtualFileSystem::NewFileHandle(int connection)
{
	xhnd hnd = m_hndserial++;
	m_handles.emplace(hnd, (direntry_t*)0);
	return hnd;
}



void XVirtualFileSystem::Tattach(xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback)
{
	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd))
	{
		callback("Invalid handle!", 0);
		return;
	}

	m_handles[hnd] = &s_rootdirentry;

	qid_t q = filenodeqid(s_rootnode);
	callback(0, &q);
}

void XVirtualFileSystem::Twalk(xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback)
{
	// MAXWELEM pg 750
	if (nwname > 16)
	{
		callback("WALKED TOO FAR", 0, 0);
		return;
	}

#if XLOG_VERBOSE
	putchar('\n');
#endif

	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!", 0, 0);
		return;
	}

	direntry_t* de = dehnd;
	XVirtualFile* wn = de->node;

	qid_t wqid[16];

	int i = 0;
	uint16_t n = nwname;
	for (xstr_t name = wname; i < n; name = xstrnext(name))
	{
#if XLOG_VERBOSE 
		xstrprint(name);
		putchar('\n');
#endif

		if (wn->Type() == X9P_FT_DIRECTORY)
		{
			direntry_t* f = 0;
			xerr_t ferr = wn->FindChild(name, &f);
			if (ferr)
			{
				// Does not exist!
				break;
			}
			de = f;
			wn = de->node;
			wqid[i] = filenodeqid(de->node);
		}
		else
		{
			// Cant walk into a file!
			break;
		}


		i++;
	}
	// According to the 9P man documents, this *should* be an error
	// v9fs REFUSES to call Tcreate if we don't return Rcreate though
	// So instead, we just return that we walked 0 directories
	//if (i == 0)
	//	return "File does not exist!";
	

	m_handles[newhnd] = de;
	callback(0, i, &wqid[0]);
}


void XVirtualFileSystem::Topen(xhnd hnd, openmode_t mode, Ropen_fn callback)
{
	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!", 0, 0);
		return;
	}

	qid_t q = filenodeqid(dehnd->node);
	callback(0, &q, 512);
}



void XVirtualFileSystem::Tcreate(xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback)
{

	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!", 0, 0);
		return;
	}
	XVirtualFile* dir = dehnd->node;

	XVirtualFile* node;
	if (perm & X9P_DM_DIR)
	{
		// Directory
		node = new XVFSDirectory();
		node->AddChild(XSTR_L(".."), dir, 0);
	}
	else
	{
		// Text file
		node = new XVFSFile();
	}

	direntry_t* rde = 0;
	dir->AddChild(name, node, &rde);
	m_handles[hnd] = rde;

	qid_t q = filenodeqid(rde->node);
	callback(0, &q, TEMP_IOUNIT);
}

void XVirtualFileSystem::Tread(xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)
{


	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!", 0, 0);
		return;
	}

	XVirtualFile* node = dehnd->node;

	if (node->Type() == X9P_FT_DIRECTORY)
	{
		int childcount = node->ChildCount();
		
		uint32_t idx = 0;
		direntry_t* child = 0; 

		uint64_t o = 0;
		size_t sz = 0;
		while (o <= offset)
		{
			xerr_t err = node->GetChild(idx, &child);
			if (err)
			{
				// Probably out of range?
				callback(0, 0, 0);
				return;
			}
			idx++;
			
			sz = vfstat(child, 0, 0);
			o += sz;
		}

		assert(o - sz == offset);

		stat_t* stats = (stat_t*)malloc(sz);
		vfstat(child, stats, sz);

		callback(0, sz, (uint8_t*)stats);

		free(stats);
	}
	else
	{

		// Read with no buffer to see how much can be read
		// Allocate with that rather than count
		// Otherwise, someone could just say "READ 0xFFFFFFFF BYTES!" and we'd allocate way too much
		uint32_t sz = node->Read(offset, count, 0);
		uint8_t* data = (uint8_t*)malloc(sz);
		

		sz = node->Read(offset, sz, data);

		callback(0, sz, data);

		free(data);

	}
}


void XVirtualFileSystem::Twrite(xhnd hnd, uint64_t offset, uint32_t count, uint8_t* data, Rwrite_fn callback)
{

	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!", 0);
		return;
	}

	XVirtualFile* node = dehnd->node;
	if (node->Type() == X9P_FT_DIRECTORY)
	{
		callback("Directories may not be written to!", 0);
	}
	else
	{
		uint32_t sz = node->Write(offset, count, data);
		callback(0, sz);
	}
}


void XVirtualFileSystem::Tclunk(xhnd hnd, Rclunk_fn callback)
{
	m_handles.erase(hnd);
	callback(0);
}

void XVirtualFileSystem::Tremove(xhnd hnd, Rremove_fn callback)
{
	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!");
		return;
	}

	XVirtualFile* node = dehnd->node;

	node->RemoveChild(dehnd->name);
	
	m_handles.erase(hnd);

	callback(0);
}


void XVirtualFileSystem::Tstat(xhnd hnd, Rstat_fn callback)
{
	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!", 0);
		return;
	}

	uint32_t sz = vfstat(dehnd,0,0);
	stat_t* stats = (stat_t*)malloc(sz);
	vfstat(dehnd, stats, sz);

	callback(0, stats);

	free(stats);

}

void XVirtualFileSystem::Twstat(xhnd hnd, stat_t* stat, Rwstat_fn callback)
{
	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!");
		return;
	}

	XVirtualFile* fnode = dehnd->node;
	
	/*
	if (stat->atime != UINT32_MAX)
		fnode->atime = stat->atime;
	if (stat->mtime != UINT32_MAX)
		fnode->mtime = stat->mtime;
	*/
	//if (stats->size != UINT64_MAX)
	//	fnode->size = stats->size;

	callback(0);
}
