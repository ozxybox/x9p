#include "fs.h"
#include "X9PClient.h"

#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>



static direntry_t s_rootdirentry;
static filenode_t* s_rootnode;

class XRemoteFileSystem : public XFileSystem
{
public:
	XRemoteFileSystem(X9PClient* client, fid_t rootfid);

	virtual xerr_t walk  (fauth_t* auth, filenode_t* fnode, walkparam_t* param, Rwalk_t callback);
	virtual xerr_t open  (fauth_t* auth, filenode_t* fnode, openmode_t mode, Ropen_t callback);
	virtual xerr_t create(fauth_t* auth, filenode_t* dir, xstr_t name, dirmode_t perm, openmode_t mode, Rcreate_t callback);
	virtual xerr_t read  (fauth_t* auth, filenode_t* fnode, fileio_t* fio, Rread_t callback);
	virtual xerr_t write (fauth_t* auth, filenode_t* fnode, fileio_t* fio, Rwrite_t callback);
	virtual xerr_t remove(fauth_t* auth, direntry_t* dentry, Rremove_t callback);//filenode_t* dir, filenode_t* file);
	virtual xerr_t stat  (fauth_t* auth, filenode_t* fnode, Rstat_t callback);
	virtual xerr_t wstat (fauth_t* auth, filenode_t* fnode, filestats_t* stats, Rwstat_t callback);

	// Non 9P RPC calls
	virtual uint32_t fileversion(filenode_t* fnode);
private:
	X9PClient* m_client;
	fid_t m_rootfid;
};

XRemoteFileSystem s_vfs;

direntry_t* virt_rootdirentry() { return &s_rootdirentry; }


filenode_t* createNode(bool directory)
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
	node->fs = &s_vfs;

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

XRemoteFileSystem::XRemoteFileSystem(X9PClient* client, fid_t rootfid) : m_client(client), m_rootfid(rootfid)
{

	
}


// Non 9P RPC calls
xerr_t XRemoteFileSystem::walk(fauth_t* auth, filenode_t* fnode, walkparam_t* param, walkret_t* ret)
{
	// MAXWELEM pg 750
	if (param->nwname > 16) return "WALKED TOO FAR";

	putchar('\n');

	direntry_t* de = 0;
	filenode_t* wn = fnode;

	int i = 0;
	uint16_t n = param->nwname;
	for (xstr_t name = param->wname; i < n; name = xstrnext(name))
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
			ret->wqid[i] = wn;
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

	ret->nwqid = i;

	ret->walked = de;


	return 0;
}

xerr_t XRemoteFileSystem::open(fauth_t* auth, filenode_t* fnode, openmode_t mode)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;


	return 0;
}

xerr_t XRemoteFileSystem::create(fauth_t* auth, filenode_t* dir, xstr_t name, dirmode_t perm, openmode_t mode, direntry_t** ret)
{
	filenode_t* node;
	if (perm & X9P_DM_DIR)
	{
		// Directory
		node = createNode(true);
		virt_filedata_t* vfd = (virt_filedata_t*)node->data;

		// Extras
		adddentry(node, node, (const char*)".");
		adddentry(node, dir, (const char*)"..");
	}
	else
	{
		// Text file
		node = createNode(false);
		virt_filedata_t* nodedat = (virt_filedata_t*)node->data;

		nodedat->buf = new std::vector<char>();
		nodedat->len = 0;
		nodedat->ver = 0;
		nodedat->buf->resize(1024);
		memset(nodedat->buf->data(), 0, 1024);


	}

	*ret = adddentry(dir, node, name);


	return 0;
}

xerr_t XRemoteFileSystem::read(fauth_t* auth, filenode_t* fnode, fileio_t* fio)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	if (fnode->type == X9P_FT_DIRECTORY)
	{

		//size_t tot = vfd->children->size() * sizeof(stat_t);
		char* buf = (char*)malloc(8096);
		stat_t* stat = (stat_t*)buf;
		//for (int i = 0; i < vfd->children->count(); i++)
		//int caa = 0;
		for (auto& p : *vfd->children)
		{
			direntry_t* de = &p.second;
			filenode_t* node = de->node;
			filestats_t stats;
			xerr_t err = node->fs->stat(auth, node, &stats);
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
	}
	else
	{
		// Reading outside of the file?
		if (fio->offset >= vfd->len)
		{
			fio->result = 0;
			return 0;
		}

		size_t sz = fio->offset + fio->count;

		// Cap the size off
		if (sz > vfd->len) sz = vfd->len;
		sz -= fio->offset;

		// File -> FIO 
		memcpy(fio->data, vfd->buf->data() + fio->offset, sz);
		fio->result = sz;
	}
	return 0;
}

xerr_t XRemoteFileSystem::write(fauth_t* auth, filenode_t* fnode, fileio_t* fio)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	if (fnode->type == X9P_FT_DIRECTORY)
	{
		return "NOT IMPLEMENTED";
	}
	else
	{
		size_t sz = fio->offset + fio->count;

		if (sz > vfd->len)
			vfd->len = sz;
		if (sz > vfd->buf->size())
			vfd->buf->resize(sz);


		// Cap the size off
		//if (sz > vfd->len) sz = vfd->len;
		sz -= fio->offset;

		// FIO -> File
		memcpy(vfd->buf->data() + fio->offset, fio->data, sz);
		fio->result = sz;

	}
	return 0;
}

xerr_t XRemoteFileSystem::remove(fauth_t* auth, direntry_t* dentry)
{
	fid_t fid = reinterpret_cast<fid_t>(dentry->parent->data);

	m_client->Tremove(fid, [&](xerr_t err) {

	});


	return 0;
}

xerr_t XRemoteFileSystem::stat(fauth_t* auth, filenode_t* fnode, filestats_t* stats)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;

	stats->uid = "user";
	stats->gid = "group";
	stats->muid = "otheruser";

	stats->atime = fnode->atime;
	stats->mtime = fnode->mtime;

	dirmode_t mode = X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC;
	if (fnode->type == X9P_FT_DIRECTORY)
		mode |= X9P_DM_DIR;

	stats->mode = mode;
	stats->size = vfd->len;

	return 0;
}

xerr_t XRemoteFileSystem::wstat(fauth_t* auth, filenode_t* fnode, filestats_t* stats)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	if (stats->atime != UINT32_MAX)
		fnode->atime = stats->atime;
	if (stats->mtime != UINT32_MAX)
		fnode->mtime = stats->mtime;
	//if (stats->size != UINT64_MAX)
	//	fnode->size = stats->size;


	return 0;
}

uint32_t XRemoteFileSystem::fileversion(filenode_t* fnode)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	return vfd->ver;
}
