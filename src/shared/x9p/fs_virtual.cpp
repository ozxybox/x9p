#include "fs.h"

#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>


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

foperr_t virt_walk(fauth_t* auth, filenode_t* fnode, walkparam_t* param, walkret_t* ret);
foperr_t virt_open(fauth_t* auth, filenode_t* fnode, openmode_t mode, uint64_t* size);
foperr_t virt_create(fauth_t* auth, filenode_t* dir, xstr_t name, dirmode_t perm, openmode_t mode, direntry_t** ret);
foperr_t virt_read(fauth_t* auth, filenode_t* fnode, fileio_t* fio);
foperr_t virt_write(fauth_t* auth, filenode_t* fnode, fileio_t* fio);
foperr_t virt_remove(fauth_t* auth, direntry_t* dentry);// filenode_t* dir, filenode_t* fnode);
foperr_t virt_stat(fauth_t* auth, filenode_t* fnode, filestats_t* stats);
foperr_t virt_wstat(fauth_t* auth, filenode_t* fnode, filestats_t* stats);
uint32_t virt_fileversion(filenode_t* fnode);

static fileops_t s_vfops = {
	virt_walk,
	virt_open,
	virt_create,
	virt_read,
	virt_write,
	virt_remove,
	virt_stat,
	virt_wstat,
	virt_fileversion
};

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
	node->ops = &s_vfops;

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

void virt_init()
{
	// Root
	s_rootnode = createNode(true);
	virt_filedata_t* s_rootfile = (virt_filedata_t*)s_rootnode->data;

	s_rootdirentry.name = xstrdup("/");
	s_rootdirentry.parent = s_rootnode;
	s_rootdirentry.node = s_rootnode;

	
	// Text file
	filenode_t* txt = createNode(false);
	virt_filedata_t* txtdat = (virt_filedata_t*)txt->data;

	txtdat->buf = new std::vector<char>();
	txtdat->len = strlen("Hello world!\n");
	txtdat->ver = 0;
	txtdat->buf->resize(1024);
	memcpy(txtdat->buf->data(), "Hello world!\n", txtdat->len);

	adddentry(s_rootnode, txt, "hello.txt");

	// Directory
	filenode_t* folder = createNode(true);
	virt_filedata_t* fvfd = (virt_filedata_t*)txt->data;
	direntry_t* files = adddentry(s_rootnode, folder, "files");
	adddentry(folder, txt, "hoopla.txt");




	// Extras
	adddentry(s_rootnode, s_rootnode, ".");
	adddentry(s_rootnode, s_rootnode, "..");

}


foperr_t virt_walk(fauth_t* auth, filenode_t* fnode, walkparam_t* param, walkret_t* ret)
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

foperr_t virt_open(fauth_t* auth, filenode_t* fnode, openmode_t mode, uint64_t* size)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	*size = vfd->len;

	return 0;
}

foperr_t virt_create(fauth_t* auth, filenode_t* dir, xstr_t name, dirmode_t perm, openmode_t mode, direntry_t** ret)
{
	filenode_t* node;
	if (perm & X9P_DM_DIR)
	{
		// Directory
		node = createNode(true);
		virt_filedata_t* vfd = (virt_filedata_t*)node->data;

		// Extras
		adddentry(node, node, (const char*)".");
		adddentry(node, dir,  (const char*)"..");
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

foperr_t virt_read(fauth_t* auth, filenode_t* fnode, fileio_t* fio)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	if (fnode->type == X9P_FT_DIRECTORY)
	{

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
			foperr_t err = node->ops->stat(auth, node, &stats);
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

foperr_t virt_write(fauth_t* auth, filenode_t* fnode, fileio_t* fio)
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

foperr_t virt_remove(fauth_t* auth, direntry_t* dentry)//filenode_t* dir, filenode_t* fnode)
{
	virt_filedata_t* vfd = (virt_filedata_t*)dentry->parent->data;

	vfd->children->erase(vfd->children->find(dentry->name));

	return 0;
}

foperr_t virt_stat(fauth_t* auth, filenode_t* fnode, filestats_t* stats)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;

	stats->uid  = "user";
	stats->gid  = "group";
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

foperr_t virt_wstat(fauth_t* auth, filenode_t* fnode, filestats_t* stats)
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


uint32_t virt_fileversion(filenode_t* fnode)
{
	virt_filedata_t* vfd = (virt_filedata_t*)fnode->data;
	return vfd->ver;
}
