#include "fs.h"
#include "X9PFileSystem.h"
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>


#define TEMP_IOUNIT 512

// Too long to type!
typedef std::unordered_map<xstr_t, direntry_t, xstrhash_t, xstrequality_t> vflookup_t;

struct virt_filedata_t
{
	union
	{
	std::vector<char>* buf;
	vflookup_t* children;
	};
	size_t len;
	uint32_t ver;
};

static direntry_t s_rootdirentry;
static filenode_t* s_rootnode;



direntry_t* virt_rootdirentry() { return &s_rootdirentry; }


filenode_t* createNode(bool directory, X9PFileSystem* fs)
{
	virt_filedata_t* vfd = new virt_filedata_t;
	vfd->children = directory ? new vflookup_t : 0;
	vfd->len = 0;
	vfd->ver = 0;

	filenode_t* node = new filenode_t;
	node->nid = filenodenewid();
	node->atime = 0;
	node->mtime = 0;
	node->muid = 0;
	node->uid = 0;
	node->gid = 0;
	node->type = directory ? X9P_FT_DIRECTORY : X9P_FT_FILE;
	node->data = (void*)vfd;
	node->fs = fs;

	return node;
}

direntry_t* adddentry(filenode_t* parent, filenode_t* child, const char* filename)
{
	virt_filedata_t* vfd = (virt_filedata_t*)parent->data;

	direntry_t de;
	de.name = xstrdup(filename);
	de.parent = parent;
	de.node = child;

	vfd->children->emplace(de.name, de);
	return &vfd->children->at(de.name);
}

direntry_t* adddentry(filenode_t* parent, filenode_t* child, xstr_t filename)
{
	virt_filedata_t* vfd = (virt_filedata_t*)parent->data;

	direntry_t de;
	de.name = xstrdup(filename);
	de.parent = parent;
	de.node = child;

	vfd->children->emplace(de.name, de);
	return &vfd->children->at(de.name);
}

XVirtualFileSystem::XVirtualFileSystem()
{
	m_hndserial = 0;

	// Root
	s_rootnode = createNode(true, this);
	virt_filedata_t* s_rootfile = (virt_filedata_t*)s_rootnode->data;

	s_rootdirentry.name = xstrdup("/");
	s_rootdirentry.parent = s_rootnode;
	s_rootdirentry.node = s_rootnode;

	
	// Text file
	filenode_t* txt = createNode(false, this);
	virt_filedata_t* txtdat = (virt_filedata_t*)txt->data;

	txtdat->buf = new std::vector<char>();
	txtdat->len = strlen("Hello world!\n");
	txtdat->ver = 0;
	txtdat->buf->resize(1024);
	memcpy(txtdat->buf->data(), "Hello world!\n", txtdat->len);

	adddentry(s_rootnode, txt, "hello.txt");

	// Directory
	filenode_t* folder = createNode(true, this);
	virt_filedata_t* fvfd = (virt_filedata_t*)txt->data;
	direntry_t* files = adddentry(s_rootnode, folder, "files");
	adddentry(folder, txt, "hoopla.txt");




	// Extras
	adddentry(s_rootnode, s_rootnode, ".");
	adddentry(s_rootnode, s_rootnode, "..");

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

	putchar('\n');

	direntry_t* dehnd = 0;
	if (!isValidXHND(hnd, dehnd) || !dehnd)
	{
		callback("Invalid handle!", 0, 0);
		return;
	}
	filenode_t* fnode = dehnd->node;

	direntry_t* de = 0;
	filenode_t* wn = fnode;

	qid_t wqid[16];

	int i = 0;
	uint16_t n = nwname;
	for (xstr_t name = wname; i < n; name = xstrnext(name))
	{
		xstrprint(name);
		putchar('\n');

		if (wn->type == X9P_FT_DIRECTORY)
		{
			virt_filedata_t* vfd = (virt_filedata_t*)wn->data;
			auto f = vfd->children->find(name);
			if (f == vfd->children->end())
			{
				// Does not exist!
				break;
			}
			de = &f->second;
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
	direntry_t* de = m_handles[hnd];
	qid_t q = filenodeqid(de->node);
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
	filenode_t* dir = dehnd->node;

	filenode_t* node;
	if (perm & X9P_DM_DIR)
	{
		// Directory
		node = createNode(true, this);
		virt_filedata_t* vfd = (virt_filedata_t*)node->data;

		// Extras
		adddentry(node, node, (const char*)".");
		adddentry(node, dir,  (const char*)"..");
	}
	else
	{
		// Text file
		node = createNode(false, this);
		virt_filedata_t* nodedat = (virt_filedata_t*)node->data;

		nodedat->buf = new std::vector<char>();
		nodedat->len = 0;
		nodedat->ver = 0;
		nodedat->buf->resize(1024);
		memset(nodedat->buf->data(), 0, 1024);


	}

	direntry_t* rde = adddentry(dir, node, name);
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
	virt_filedata_t* vfd = (virt_filedata_t*)dehnd->node->data;

	if (dehnd->node->type == X9P_FT_DIRECTORY)
	{
#if 0
		//size_t tot = vfd->children->size() * sizeof(stat_t);
		char* buf = (char*)malloc(8096);
		stat_t* stat = (stat_t*)buf;
		//for (int i = 0; i < vfd->children->count(); i++)
		//int caa = 0;
		for(auto& p : *vfd->children)
		{
			direntry_t* de = &p.second;
			filenode_t* node = de->node;
			filestats_t stats;
			node->fs->Tstat(node->);
			if (err)
				return err;

			size_t szname = de->name->len;
			size_t szuid = strlen(stats.uid);
			size_t szgid = strlen(stats.gid);
			size_t szmuid = strlen(stats.muid);
			size_t size = sizeof(stat_t) + szname + 2 + szuid + 2 + szgid + 2 + szmuid + 2;

			/*
			if (size > m_maxmessagesize)
			{
				printf("errrrrrrrr");
			}
			*/

			stat->size = size;
			stat->atime = stats.atime;
			stat->mtime = stats.mtime;
			stat->mode = stats.mode | X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC; // X9P_DM_DIR ;
			stat->length = stats.size;
			stat->qid = filenodeqid(node);

			xstr_t name = stat->name();
			name->len = szname;
			memcpy(name->str(), de->name->str(), szname);

			xstr_t uid = stat->uid();
			uid->len = szuid;
			memcpy(uid->str(), stats.uid, szuid);

			xstr_t gid = stat->gid();
			gid->len = szgid;
			memcpy(gid->str(), stats.gid, szgid);

			xstr_t muid = stat->muid();
			muid->len = szmuid;
			memcpy(muid->str(), stats.muid, szmuid);

			stat = (stat_t*)(muid->str() + muid->len);
		}

		size_t total = ((char*)stat) - buf;


		// Reading outside of the file?
		if (fio->offset >= total)
		{
			fio->result = 0;
			return 0;
		}

		size_t sz = fio->offset + fio->count;

		// Cap the size off
		if (sz > total) sz = total;
		sz -= fio->offset;

		// File -> FIO 
		memcpy(fio->data, buf + fio->offset, sz);
		fio->result = sz;
		//fio->result = 
		free(buf);

		return 0;// "NOT IMPLEMENTED";
#else
		callback("Reading directories not implemented!", 0, 0);
#endif
	}
	else
	{
		
		// Reading outside of the file?
		if (offset >= vfd->len)
		{
			callback(0, 0, (uint8_t*)"");
			return;
		}

		size_t sz = offset + count;
		
		// Cap the size off
		if (sz > vfd->len) sz = vfd->len;
		sz -= offset;

		uint8_t* data = (uint8_t*)malloc(sz);

		// File -> FIO 
		memcpy(data, vfd->buf->data() + offset, sz);

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

	virt_filedata_t* vfd = (virt_filedata_t*)dehnd->node->data;
	if (dehnd->node->type == X9P_FT_DIRECTORY)
	{
		callback("Writing to directories not implemented!", 0);
	}
	else
	{
		size_t sz = offset + count;

		if (sz > vfd->len)
			vfd->len = sz;
		if (sz > vfd->buf->size())
			vfd->buf->resize(sz);


		// Cap the size off
		//if (sz > vfd->len) sz = vfd->len;
		sz -= offset;

		// FIO -> File
		memcpy(vfd->buf->data() + offset, data, sz);
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

	direntry_t* hndde = m_handles[hnd];
	virt_filedata_t* vfd = (virt_filedata_t*)hndde->parent->data;
	vfd->children->erase(vfd->children->find(hndde->name));
	
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

	filenode_t* fnode = dehnd->node;
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;

	size_t sz = sizeof(stat_t) + dehnd->name->size() + sizeof("user") + sizeof("groupuser") + sizeof("otheruser") - 3 + 3 * sizeof(xstrlen_t);
	stat_t* stats = (stat_t*)malloc(sz);

	stats->size = sz;

	// We should really just zero everything that might go over the network for security reasons
	memset(&stats->_unused0[0], 0, 6);
	
	stats->qid = filenodeqid(fnode);

	dirmode_t mode = X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC;
	if (fnode->type == X9P_FT_DIRECTORY)
		mode |= X9P_DM_DIR;

	stats->mode = mode;

	stats->atime = fnode->atime;
	stats->mtime = fnode->mtime;

	stats->length = fnode->type == X9P_FT_DIRECTORY ? 0 : vfd->len;

	xstrcpy(stats->name(), dehnd->name);
	xstrcpy(stats->uid(), "user");
	xstrcpy(stats->gid(), "group");
	xstrcpy(stats->muid(), "otheruser");

	
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

	filenode_t* fnode = dehnd->node;
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	
	if (stat->atime != UINT32_MAX)
		fnode->atime = stat->atime;
	if (stat->mtime != UINT32_MAX)
		fnode->mtime = stat->mtime;
	//if (stats->size != UINT64_MAX)
	//	fnode->size = stats->size;

	callback(0);
}

uint32_t XVirtualFileSystem::fileversion(filenode_t* fnode)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	return vfd->ver;
}
