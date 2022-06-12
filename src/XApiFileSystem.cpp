#include "XApiFileSystem.h"
#include "X9PFileSystem.h"
#include "X9PClient.h"
#include "XLogging.h"
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <time.h>
#include <assert.h>



XApiFSHandle* fsapisetcaller::s_caller = nullptr;
//thread_local XApiFSHandle* XApiFSNode::s_caller = nullptr;


size_t fsapistat(XApiFSHandle* de, stat_t* stats, uint32_t max)
{
	fsapisetcaller sc(de);

	XApiFSNode* fnode = de->node;


	xstr_t user     = fnode->OwnerUser();
	xstr_t group    = fnode->OwnerGroup();
	xstr_t modifier = fnode->LastUserToModify();
	
	
	int namelen = fnode->Name(0, 0);

	size_t sz = sizeof(stat_t) + namelen + user->size() + group->size() + modifier->size();

	// If we're being asked to write to a null, just stop and hand back how much we would write if we could
	if (!stats) return sz;
	assert(sz <= max);

	stats->size = sz;

	stats->qid = fnode->QID();

	dirmode_t mode = X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC;
	mode |= ((unsigned int)fnode->Type()) << 24;
	stats->mode = mode;

	stats->atime = fnode->AccessTime();
	stats->mtime = fnode->ModifyTime();

	stats->length = fnode->Length();

	fnode->Name(stats->name(), namelen);
	memcpy(stats->uid(), user, user->size());
	memcpy(stats->gid(), group, group->size());
	memcpy(stats->muid(), modifier, modifier->size());

	return sz;
}


// TODO: Make this all actually work and change users to not be strings eventually 
void XApiFSNode::Open(openmode_t mode)
{
}
bool XApiFSNode::CanOpen(openmode_t mode)
{
	XAuth* auth = GetCaller()->hnd->auth;

	dirmode_t request = X9POpenModeToDirMode(mode);
	if (xstrcmp(auth->uname, OwnerUser()) == 0)
	{
		// We own this node
		// Maintain "owner", "group", and "other"
		request = request & (X9P_DM_MASK_OWNER | X9P_DM_MASK_GROUP | X9P_DM_MASK_OTHER);
	}
	else if (auth->HasGroup(OwnerGroup()))
	{
		// We're in the same group
		// Maintain "group" and "other"
		request = request & (X9P_DM_MASK_GROUP | X9P_DM_MASK_OTHER);
	}
	else
	{
		// We're not in the group and not the owner
		// Maintain only "other"
		request = request & (X9P_DM_MASK_OTHER);
	}

	dirmode_t actual = Permissions();
	dirmode_t allowed = request & actual;
	
	// Flatten allowed permissions to the lower 3 bits, where "other" is located
	allowed = ((allowed >> 6) | (allowed >> 3) | (allowed >> 0));

	// Did we get our request?
	return (allowed & request & X9P_DM_MASK_OTHER) == request;
}

void XApiFSNode::Clunk()
{

}


XApiFSHandle* XApiFileSystem::GetHnd(xhnd hnd)
{
	// Is our hid valid?
	if (hnd->id == XHID_UNINITIALIZED)
		return 0;

	// Has this user attached yet?
	auto f = m_userhandles.find(hnd->auth);
	if (f == m_userhandles.end())
		return 0;
	auto& ad = f->second;

	// Are we tracking this hid?
	auto g = ad.handles.find(hnd->id);
	if (g == ad.handles.end())
		return 0;

	// Got it!
	return g->second;
}

XApiFSHandle* XApiFileSystem::TagHnd(xhnd hnd)
{
	// We only want uninitialized hnds
	if (hnd->id != XHID_UNINITIALIZED)
		return 0;

	// Has this user attached yet?
	auto f = m_userhandles.find(hnd->auth);
	if (f == m_userhandles.end())
		return 0;
	auto& ad = f->second;

	// TODO: This is really unlikely to matter. Is there a nicer way to do this?
	// Is this hid already in use?
	xhid hid;
	do {
		hid = ad.hidserial++;
	} while (ad.handles.find(hid) != ad.handles.end());

	// Got an open hid, track it
	XApiFSHandle* h = new XApiFSHandle{ hnd , X9P_OPEN_EX_INVALID, 0, 0, 0 };
	hnd->id = hid;
	ad.handles.insert({ hid, h });

	return h;
}
void XApiFileSystem::UntagHnd(xhnd hnd)
{
	// Has this user attached yet?
	auto f = m_userhandles.find(hnd->auth);
	if (f == m_userhandles.end())
		return;
	auto& ad = f->second;

	// Are we tracking this hid?
	auto g = ad.handles.find(hnd->id);
	if (g == ad.handles.end())
		return;

	// Bye, bye!
	delete g->second;
	f->second.handles.erase(g);
}


XApiFileSystem::XApiFileSystem(XApiFSDir* root) : m_rootdir(root)
{

}


void XApiFileSystem::Disconnect(XAuth* auth)
{
	auto f = m_userhandles.find(auth);
	if (f == m_userhandles.end())
		return;
	auto& hnds = f->second;

	// Delete all handles
	for (auto& p : hnds.handles)
		delete p.second;

	// Clear out their data
	m_userhandles.erase(f);
}



void XApiFileSystem::Tattach(xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	// Make sure we've made space for this user
	auto f = m_userhandles.find(hnd->auth);
	if (f == m_userhandles.end())
		m_userhandles.insert({ hnd->auth, {} });

	// Get a new handle for them to attach to
	XApiFSHandle* ndh = TagHnd(hnd);
	if (ndh == 0)
	{
		callback(XERR_FID_USED, 0);
		return;
	}


	// Fill in the new handle with info about our root dir
	ndh->hnd = hnd;
	ndh->node = m_rootdir;
	ndh->parent = m_rootdir;
	ndh->openmode = X9P_OPEN_EX_INVALID;

	fsapisetcaller sc(ndh, m_rootdir);

	ndh->qidpath = m_rootdir->Path();
	
	qid_t q = m_rootdir->QID();
	callback(0, &q);

	// Let any overriding classes know we got a new connection
	NewConnection(hnd->auth);
}

void XApiFileSystem::Twalk(xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);

	if (nwname > X9P_TWALK_MAXELEM)
	{
		callback(XERR_WALK_TOO_FAR, 0, 0);
		return;
	}

	// Does the starting handle exist?
	XApiFSHandle* starthnd = GetHnd(hnd);
	if (!starthnd)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	// Check if we can use the second handle
	if (hnd->id == newhnd->id)
	{
		// IDs are the same, we're outputting to the start hnd
	}
	else
	{
		// Make sure the requested id is open for use
		XApiFSHandle* whnd = GetHnd(newhnd);
		if (whnd != 0)
		{
			// Requested hnd is already in use!
			callback(XERR_FID_USED, 0, 0);
			return;
		}
	}

	// Resulting walked hnd
	XApiFSHandle walkinghnd = { hnd, X9P_OPEN_EX_INVALID };// , starthnd->qidpath, starthnd->node, starthnd->parent
	walkinghnd.copyfrom(starthnd);

	// qid walk result
	qid_t wqid[X9P_TWALK_MAXELEM];

	int nwalked = 0;
	for (xstr_t name = wname; nwalked < nwname; name = xstrnext(name))
	{
#if XLOG_VERBOSE
		xstrprint(name);
		putchar('\n');
#endif

		// Let the node know that we're the caller
		fsapisetcaller sc(&walkinghnd);

		// Is this a directory or a file?
		if (walkinghnd.node->Type() == X9P_QT_DIR)
		{
			XApiFSDir* dn = static_cast<XApiFSDir*>(walkinghnd.node);
			if (name->len == 1 && *name->str() == '.')
			{
				// Accessing . is against the protocol, but we'll handle it just in case.
			}
			// else if name is "..", handle it somehow lol
			else
			{
				XApiFSHandle nh;
				xerr_t ferr = dn->FindChild(name, &nh);
				if (ferr)
				{
#if XLOG_VERBOSE
					printf("Directory does not exist!");
#endif
					// Does not exist!
					break;
				}

				// Update our pos
				walkinghnd.copyfrom(&nh);
			}

			// Add the qid to the arr
			wqid[nwalked] = walkinghnd.node->QID();
		}
		else
		{
			// Cant walk into a file! We're done now
			break;
		}

		nwalked++;
	}

	// FIXME: According to the 9P man documents, this *should* be an error
	//        v9fs REFUSES to call Tcreate if we don't return Rwalk though
	//        So instead, we just return that we walked 0 directories
	// 
#if !X9P_V9FS_COMPAT
	if (nwalked == 0 && nwname != 0)
	{
		callback(XERR_FILE_DNE, 0, 0);
		return;
	}
#else
	if (nwalked == 0 && nwname != 0)
	{
		printf("HHHHHHHH");
	}
#endif
	

	if (newhnd->id == hnd->id)
	{
		// Overwrite original
		*starthnd = walkinghnd;
	}
	else
	{
		// Tag it as a new hnd
		TagHnd(newhnd)->copyfrom(&walkinghnd);
	}

	// Respond
	callback(0, nwalked, &wqid[0]);
}



void XApiFileSystem::Topen(xhnd hnd, openmode_t mode, Ropen_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	XApiFSHandle* ndh = GetHnd(hnd);
	if (!ndh)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	// We cannot open a node twice!
	if (!(ndh->openmode & X9P_OPEN_EX_INVALID))
	{
		callback(XERR_ALREADY_OPEN, 0, 0);
		return;
	}

	// Check if we're opening root with remove
	if (ndh->node == m_rootdir && ndh->parent == m_rootdir && (mode & X9P_OPEN_CLOSE))
	{
		callback(XERR_NO_PERM, 0, 0);
		return;
	}

	// We have to mark who we are before we make calls
	fsapisetcaller sc(ndh);


	// Check permissions
	if (!ndh->node->CanOpen(mode))
	{
		callback(XERR_NO_PERM, 0, 0);
		return;
	}

	// Can we hook this file?
	if (ndh->node->Type() & X9P_QT_MOUNT)
	{
		if (ndh->node->CanHook())
		{
			xerr_t hkerr = ndh->node->Hook();
			if (hkerr)
			{
				callback(hkerr, 0, 0);
				return;
			}
		}
	}

	// Open the hnd with the requested mode
	ndh->openmode = mode;
	ndh->node->Open(mode);

	qid_t q = ndh->node->QID();
	callback(0, &q, X9PDefaultIOUnit(hnd));
}



void XApiFileSystem::Tcreate(xhnd hnd, xstr_t name, dirmode_t perm, openmode_t mode, Rcreate_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	// TODO: Add filtering for invalid names, such as "." ".." "a/b" "a\\b"

	XApiFSHandle* ndh = GetHnd(hnd);
	if (!ndh)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	// We have to mark who we are before we make calls
	fsapisetcaller sc(ndh);

	// Can only call Tcreate on a directory!
	if (ndh->node->Type() != X9P_QT_DIR)
	{
		callback(XERR_NOT_DIR, 0, 0);
		return;
	}
	XApiFSDir* dir = static_cast<XApiFSDir*>(ndh->node);

	// Check permissions
	if (!dir->CanOpen(X9P_OPEN_WRITE))
	{
		callback(XERR_NO_PERM, 0, 0);
		return;
	}

	// Create the new file
	XApiFSHandle out = { 0, X9P_OPEN_EX_INVALID, 0, 0, 0 };
	xerr_t err = dir->Create(name, perm, &out);
	if (err)
	{
		callback(err, 0, 0);
		return;
	}
	if (!out.node)
	{
		callback(XERR_CANT_CREATE, 0, 0);
		return;
	}

	// Walk to and open the newly created file
	ndh->copyfrom(&out);
	ndh->openmode = mode;
	dir->Open(mode);

	fsapisetcaller sc2(ndh);
	qid_t q = ndh->node->QID();
	callback(0, &q, X9PDefaultIOUnit(hnd));
}

void XApiFileSystem::Tread(xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	XApiFSHandle* ndh = GetHnd(hnd);
	if (!ndh)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	// Check the handle's open mode
	openmode_t curmode = ndh->openmode;
	if ((curmode & X9P_OPEN_EX_INVALID) || (curmode & X9P_OPEN_STATE_MASK == X9P_OPEN_WRITE))
	{
		callback(XERR_OPEN_MODE, 0, 0);
		return;
	}

	// Set the caller
	fsapisetcaller sc(ndh);


	if (ndh->node->Type() == X9P_QT_DIR)
	{
		// Reading a directory. Give a stat list of the children
		XApiFSDir* dir = static_cast<XApiFSDir*>(ndh->node);

		XApiFSHandle child = {0,0,0,0,0};

		// Trek forward until our offset matches the direntry
		uint64_t trekoffset = 0;
		size_t sz = 0;
		int childcount = dir->ChildCount();
		unsigned int i = 0;
		for (; i < childcount; i++)
		{
			// Stop once we're IN our starting point
			if (trekoffset > offset)
				break;

			xerr_t err = dir->GetChild(i, &child);
			if (err)
				continue;

			sz = fsapistat(&child, 0, 0);
			trekoffset += sz;
		}


		// Do we still have anything more to respond with?
		if (offset >= trekoffset)
		{
			// No more!
			callback(0, 0, 0);
			return;
		}

		assert(trekoffset - sz == offset);

		// How many entries can we report?
		int start = i - 1;
		for (; i < childcount; i++)
		{
			xerr_t err = dir->GetChild(i, &child);
			if (err)
				continue;

			size_t nz = fsapistat(&child, 0, 0);

			if (sz + nz > count)
				break;

			sz += nz;
		}
		int endi = i;


		char* statbuf = (char*)calloc(sz, 1);
		char* stats = statbuf;
		char* end = statbuf + sz;

		trekoffset = 0;
		for (i = start; i < endi; i++)
		{
			xerr_t err = dir->GetChild(i, &child);
			if (err)
				continue;

			size_t x = end - stats;

			if (x == 0)
			{
				puts("end???");
				break;
			}

			size_t qz = fsapistat(&child, 0, 0);

			if (qz > x)
			{
				puts("what????");
//				break;
			}

			stats += fsapistat(&child, (stat_t*)stats, end - stats);
		}

		callback(0, sz, (void*)statbuf);
		
//#if XLOG_VERBOSE
		//XLogDumpStat(stats);
//#endif

		free(statbuf);
	}
	else
	{
		XApiFSFile* file = static_cast<XApiFSFile*>(ndh->node);

		// Read with no buffer to see how much can be read
		// Allocate with that rather than count
		// Otherwise, someone could just say "READ 0xFFFFFFFF BYTES!" and we'd allocate way too much
		uint32_t result = 0;
		xerr_t err = file->Read(offset, count, 0, result);
		if (err)
		{
			callback(err, 0, 0);
			return;
		}

		uint8_t* data = (uint8_t*)calloc(result, 1);
		err = file->Read(offset, count, data, result);
	
		if (err)
			callback(err, 0, 0);
		else
			callback(0, result, data);
		free(data);
	}
}


void XApiFileSystem::Twrite(xhnd hnd, uint64_t offset, uint32_t count, void* data, Rwrite_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	XApiFSHandle* ndh = GetHnd(hnd);
	if (!ndh)
	{
		callback(XERR_XHND_INVALID, 0);
		return;
	}

	// Check the handle's open mode
	openmode_t curmode = ndh->openmode;
	if ((curmode & X9P_OPEN_EX_INVALID) || (curmode & X9P_OPEN_STATE_MASK == X9P_OPEN_READ))
	{
		callback(XERR_OPEN_MODE, 0);
		return;
	}

	// Let it know we're the caller
	fsapisetcaller sc(ndh);


	if (ndh->node->Type() == X9P_QT_DIR)
	{
		// Can't write to a directory!
		callback(XERR_NOT_FILE, 0);
	}
	else
	{
		uint32_t result = 0;
		xerr_t err = static_cast<XApiFSFile*>(ndh->node)->Write(offset, count, data, result);

		if (err)
			callback(err, 0);
		else
			callback(0, result);
	}
}



void XApiFileSystem::Tremove(xhnd hnd, Rremove_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	XApiFSHandle* ndh = GetHnd(hnd);
	if (!ndh)
	{
		callback(XERR_XHND_INVALID);
		return;
	}

	// Check if we're removing root
	if (ndh->node == m_rootdir && ndh->parent == m_rootdir)
	{
		UntagHnd(hnd);
		callback(XERR_NO_PERM);
		return;
	}

	xerr_t err = 0;

	XApiFSDir* dir = ndh->parent;
	fsapisetcaller sc(ndh, dir);

	// Check permissions
	if (dir->CanOpen(X9P_OPEN_WRITE))
		err = dir->Remove(ndh);
	else 
		err = XERR_NO_PERM; // Not allowed to remove. Clunk the hnd anyways
	
	UntagHnd(hnd);
	callback(XERR_NO_PERM);
}


void XApiFileSystem::Tstat(xhnd hnd, Rstat_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	XApiFSHandle* ndh = GetHnd(hnd);
	if (!ndh)
	{
		callback(XERR_XHND_INVALID, 0);
		return;
	}

	uint32_t sz = fsapistat(ndh,0,0);
	stat_t* stats = (stat_t*)calloc(sz, 1);
	fsapistat(ndh, stats, sz);

	XLogDumpStat(stats);
	callback(0, stats);

	free(stats);
}

void XApiFileSystem::Twstat(xhnd hnd, stat_t* stat, Rwstat_fn callback)
{
	puts("______UNSUPPORTED CALL TO TWSTAT!!");
	callback(0);
}



void XApiFileSystem::Tclunk(xhnd hnd, Rclunk_fn callback)
{
	/* HHHHHHH */ assert(fsapisetcaller::s_caller == 0);
	XApiFSHandle* ndh = GetHnd(hnd);
	if (!ndh)
	{
		callback(XERR_XHND_INVALID);
		return;
	}

	fsapisetcaller sc(ndh);
	ndh->node->Clunk();

	// Did we have the file open for closing?
	xerr_t err = 0;
	if (ndh->openmode & X9P_OPEN_CLOSE)
	{
		// Delete on close
		XApiFSDir* dir = ndh->parent;

		// We should already have permission to delete if the file was opened with a close
		err = dir->Remove(ndh);
		
		XPRINTF("\x1b[31mRemove on clunk!\x1b[39m\n");
	}

	UntagHnd(hnd);
	hnd->id = XHID_UNINITIALIZED;
	callback(err);
}

