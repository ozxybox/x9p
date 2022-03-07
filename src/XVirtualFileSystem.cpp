#include "XVirtualFileSystem.h"
#include "X9PFileSystem.h"
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <time.h>
#include <assert.h>
#include "XLogging.h"

#define TEMP_IOUNIT 2048


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
	

	// TODO: resolve null users & groups to the parent's owners!
	assert(user);
	assert(group);
	assert(modifier);
	

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



XVFSFile::XVFSFile()
{
	m_version = 0;
		
	m_accesstime = time(0);
	m_modifytime = time(0);
}


// IO functions
bool XVFSFile::Locked()
{
	return false;
}
xerr_t XVFSFile::Open(XAuth* user, openmode_t mode)
{
	return 0;
}
void XVFSFile::Close()
{
}

uint32_t XVFSFile::Read(uint64_t offset, uint32_t count, uint8_t* buf)
{

	// Clamp the read within the bounds of the file
	if (offset >= m_buf.size())
		return 0;
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

uint32_t XVFSFile::Write(uint64_t offset, uint32_t count, uint8_t* buf)
{

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



XVFSDirectory::XVFSDirectory()
{
	m_version = 0;

	m_accesstime = time(0);
	m_modifytime = time(0);

		
	//AddChild(XSTR_L("."), this, 0);
}

// Directory Functions
xerr_t XVFSDirectory::AddChild(xstr_t name, XVirtualFile* file, direntry_t** out)
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
xerr_t XVFSDirectory::RemoveChild(xstr_t name)
{
	auto f = m_children.find(name);
	if (f == m_children.end()) return XERR_FILE_DNE;

	free(f->second.name);
	m_children.erase(f);

	m_version++;
	m_modifytime = time(0);
	return 0;
}
xerr_t XVFSDirectory::FindChild(xstr_t name, direntry_t** out)
{
	auto f = m_children.find(name);
	if (f == m_children.end())
	{
		if(out) 
			*out = 0;
		return XERR_FILE_DNE;
	}

	if(out)
		*out = &f->second;

	m_accesstime = time(0);
	return 0;
}

uint32_t XVFSDirectory::ChildCount()
{
	return m_children.size();
}
xerr_t XVFSDirectory::GetChild(uint32_t index, direntry_t** out)
{
	if (index >= m_children.size())
		return "Out of range!";

	if (out)
	{
		// FIXME: gross
		auto it = m_children.begin();
		std::advance(it, index);
		*out = &it->second;
	}

	return 0;
}




XVirtualFileSystem::XVirtualFileSystem()
{
	m_hndserial = 0;

	m_rootnode = 0;
	m_rootdirentry = { 0,0,0 };
}

void XVirtualFileSystem::Init()
{

	// Root Directory
	m_rootnode = CreateRootDirectory();

	m_rootdirentry.name = xstrdup("/");
	m_rootdirentry.parent = m_rootnode;
	m_rootdirentry.node = m_rootnode;
}


XVirtualFile* XVirtualFileSystem::CreateRootDirectory()
{
	XVFSDirectory* dir = new XVFSDirectory();
	//dir->AddChild(XSTR_L(".."), dir, 0);
	return dir;
}


direntry_t* XVirtualFileSystem::RootDirectory()
{
	return &m_rootdirentry;
}

vfilehandle_t XVirtualFileSystem::NullEntry()
{
	return {};
}



void XVirtualFileSystem::Tattach(xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback)
{
	direntry_t* dehnd = 0;
	if (!GetHNDEntry(hnd, dehnd))
	{
		callback(XERR_XHND_INVALID, 0);
		return;
	}

	m_handles[hnd].m_value = { &m_rootdirentry, X9P_OPEN_EX_INVALID };

	qid_t q = filenodeqid(m_rootnode);
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

	direntry_t* dehnd = 0;
	if (!GetHNDEntry(hnd, dehnd) || !dehnd)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

#if XLOG_VERBOSE
	printf("__Currently at %d ", hnd);
	xstrprint(dehnd->name);
	putchar('\n');
#endif

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

			// Accessing 'Dot'? It's the same dir
			if (name->len == 1 && *name->str() == '.')
			{
				// No change.

			}
			else
			{
				direntry_t* f = 0;

				xerr_t ferr = wn->FindChild(name, &f);
				if (ferr)
				{
#if XLOG_VERBOSE
					printf("Directory does not exist!");
#endif
					// Does not exist!
					break;
				}

				// Update our pos
				de = f;
				wn = de->node;
			}

			// Add the qid to the arr
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
	//	return XERR_FILE_DNE;
	

	m_handles[newhnd].m_value = { de, X9P_OPEN_EX_INVALID };
	callback(0, i, &wqid[0]);
}


void XVirtualFileSystem::Topen(xhnd hnd, openmode_t mode, Ropen_fn callback)
{
	XAuthEntry* ae = 0;
	if (!GetHND(hnd, ae) || !ae->m_value.entry)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	vfilehandle_t* vfh = &ae->m_value;
	XVirtualFile* node = vfh->entry->node;
	
	dirmode_t rp = X9POpenModeToDirMode(mode);
	dirmode_t ap = node->Permissions(ae->m_auth);
	if ((rp & ap) != rp)
	{
		callback(XERR_NO_PERM, 0, 0);
		return;
	}

	vfh->open = mode;
	qid_t q = filenodeqid(node);
	callback(0, &q, 512);
}



void XVirtualFileSystem::Tcreate(xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback)
{
	// TODO: Add filtering for invalid names, such as "." ".." "a/b" "a\\b" "a:b"
	XAuthEntry* ae = 0;
	if (!GetHND(hnd, ae) || !ae->m_value.entry)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}
	
	XVirtualFile* dir = ae->m_value.entry->node;

	XVirtualFile* node;
	if (perm & X9P_DM_DIR)
	{
		// Directory
		node = new XVFSDirectory();
		//node->AddChild(XSTR_L(".."), dir, 0);
	}
	else
	{
		// Text file
		node = new XVFSFile();
	}

	direntry_t* rde = 0;
	xerr_t err = dir->AddChild(name, node, &rde);

	if (err || !rde)
	{
		callback("Cannot create file!", 0, 0);
		delete node;
		return;
	}

	// TODO: Set owner group to the parent group
	// TODO: Set owner user to the XAuth
	// TODO: Set permissions based on the parent dir
	// TODO: Check for write permissions on the parent dir

	openmode_t m = X9P_OPEN_EX_INVALID;
	if (!(perm & X9P_DM_DIR))
	{
		// Open the file 
		dirmode_t rp = X9POpenModeToDirMode(mode);
		dirmode_t ap = node->Permissions(ae->m_auth);
		if ((rp & ap) == rp)
			m = mode;
	}
	m_handles[hnd].m_value = { rde, m };

	qid_t q = filenodeqid(rde->node);
	callback(0, &q, TEMP_IOUNIT);
}

void XVirtualFileSystem::Tread(xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)
{
	XAuthEntry* ae = 0;
	if (!GetHND(hnd, ae) || !ae->m_value.entry)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	// Check the handle's open mode
	openmode_t curmode = ae->m_value.open;
	if ((curmode & X9P_OPEN_EX_INVALID) || (curmode & X9P_OPEN_STATE_MASK == X9P_OPEN_WRITE))
	{
		callback(XERR_OPEN_MODE, 0, 0);
		return;
	}

	XVirtualFile* node = ae->m_value.entry->node;


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
	XAuthEntry* ae = 0;
	if (!GetHND(hnd, ae) || !ae->m_value.entry)
	{
		callback(XERR_XHND_INVALID, 0);
		return;
	}

	// Check the handle's open mode
	openmode_t curmode = ae->m_value.open;
	if ((curmode & X9P_OPEN_EX_INVALID) || (curmode & X9P_OPEN_STATE_MASK == X9P_OPEN_READ))
	{
		callback(XERR_OPEN_MODE, 0);
		return;
	}

	XVirtualFile* node = ae->m_value.entry->node;


	if (node->Type() == X9P_FT_DIRECTORY)
	{
		callback(XERR_NOT_FILE, 0);
	}
	else
	{
		uint32_t sz = node->Write(offset, count, data);
		callback(0, sz);
	}
}



void XVirtualFileSystem::Tremove(xhnd hnd, Rremove_fn callback)
{
	XAuthEntry* ae = 0;
	if (!GetHND(hnd, ae) || !ae->m_value.entry)
	{
		callback(XERR_XHND_INVALID);
		return;
	}

	// TODO: Check for write permission!
	// Check the handle's open mode
	/*
	{
		callback(XERR_NO_PERM);
		return;
	}
	*/

	direntry_t* de = ae->m_value.entry;
	de->node->RemoveChild(de->name);
	

	m_handles.erase(hnd);

	callback(0);
}


void XVirtualFileSystem::Tstat(xhnd hnd, Rstat_fn callback)
{
	direntry_t* dehnd = 0;
	if (!GetHNDEntry(hnd, dehnd) || !dehnd)
	{
		callback(XERR_XHND_INVALID, 0);
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
	if (!GetHNDEntry(hnd, dehnd) || !dehnd)
	{
		callback(XERR_XHND_INVALID);
		return;
	}

	// TODO: check parent dir write perms
	// TODO: check node write perms

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


bool XVirtualFileSystem::GetHNDEntry(xhnd hnd, direntry_t*& out)
{
	auto f = m_handles.find(hnd);
	if (f == m_handles.end())
	{
		out = 0;
		return false;
	}
	out = f->second.m_value.entry;
	return true;
}