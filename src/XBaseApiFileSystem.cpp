#include "XBaseApiFileSystem.h"

#define XAPI_HND_INVALID ((xhnd)0xFFFFFFFF)


xhnd XBaseApiFileSystem::NullEntry()
{
	return XAPI_HND_INVALID;
}

xhnd XBaseApiFileSystem::NewFileHandle(XAuth* user)
{
	xhnd hnd = XBaseFileSystem<xhnd>::NewFileHandle(user);

	auto f = m_userfilesystems.find(user);
	if (f == m_userfilesystems.end())
	{
		m_handles[hnd].m_value = XAPI_HND_INVALID;
	}
	else
	{
		m_handles[hnd].m_value = f->second->NewFileHandle(user);
	}

	return hnd;
}

xhnd XBaseApiFileSystem::DeriveFileHandle(xhnd hnd)
{
	// TODO: Optimize this!

	// Get the old handle
	XAuthEntry* oe = 0;
	GetHND(hnd, oe);

	// Create the new handle
	xhnd nh = XBaseFileSystem<xhnd>::NewFileHandle(oe->m_auth);
	XAuthEntry* entry = 0;
	GetHND(nh, entry);

	// See if we have a fs that can populate it 
	auto f = m_userfilesystems.find(entry->m_auth);
	if (f == m_userfilesystems.end())
		entry->m_value = XAPI_HND_INVALID;
	else
		entry->m_value = f->second->NewFileHandle(entry->m_auth);

	return nh;
}

void XBaseApiFileSystem::ReleaseFileHandle(XAuth* user, xhnd hnd)
{
	// TODO: Ref counting!

	// fs->ReleaseFileHandle
}





bool XBaseApiFileSystem::GetTrueHND(xhnd hnd, X9PFileSystem*& fsOut, xhnd& hndOut)
{
	XAuthEntry* ae = 0;

	if (!GetHND(hnd, ae))
	{
		fsOut = 0;
		hndOut = 0;
		return false;
	}

	auto f = m_userfilesystems.find(ae->m_auth);
	if (f == m_userfilesystems.end())
	{
		fsOut = 0;
		hndOut = 0;
		return false;
	}

	fsOut = f->second;
	hndOut = ae->m_value;
	return true;
}


void XBaseApiFileSystem::Tattach(xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback)
{
	XAuthEntry* entry = 0;
	if (!GetHND(hnd, entry))
	{
		callback("What??", 0);
		assert(0);
		return;
	}

	if (entry->m_value != XAPI_HND_INVALID)
	{
		callback("FID already in use?", 0);
		return;
	}

	XAuth* user = entry->m_auth;

	X9PFileSystem* fs = CreateUserFS(user);
	m_userfilesystems.emplace(user, fs);

	// Get a new xhnd from the new fs
	entry->m_value = fs->NewFileHandle(user);

	fs->Tattach(entry->m_value, 0, username, accesstree, callback);

}




// Abstractions to the user fs

void XBaseApiFileSystem::Twalk(xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	// Second hnd
	X9PFileSystem* nfs = 0;
	xhnd noh = 0;
	if (!GetTrueHND(newhnd, nfs, noh) || nfs != fs)
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}


	fs->Twalk(oh, noh, nwname, wname, callback);
}

void XBaseApiFileSystem::Topen(xhnd hnd, openmode_t mode, Ropen_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	fs->Topen(oh, mode, callback);
}

void XBaseApiFileSystem::Tcreate(xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	fs->Tcreate(oh, name, perm, mode, callback);
}

void XBaseApiFileSystem::Tread(xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID, 0, 0);
		return;
	}

	fs->Tread(oh, offset, count, callback);
}

void XBaseApiFileSystem::Twrite(xhnd hnd, uint64_t offset, uint32_t count, uint8_t* data, Rwrite_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID, 0);
		return;
	}

	fs->Twrite(oh, offset, count, data, callback);
}


void XBaseApiFileSystem::Tclunk(xhnd hnd, Rclunk_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (GetTrueHND(hnd, fs, oh))
		fs->Tclunk(oh, callback);
	else
		callback(XERR_XHND_INVALID);

	// Remove from us too!
	m_handles.erase(hnd);
}

void XBaseApiFileSystem::Tremove(xhnd hnd, Rremove_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID);
		return;
	}

	fs->Tremove(oh, callback);
}

void XBaseApiFileSystem::Tstat(xhnd hnd, Rstat_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID, 0);
		return;
	}

	fs->Tstat(oh, callback);
}

void XBaseApiFileSystem::Twstat(xhnd hnd, stat_t* stat, Rwstat_fn callback)
{
	X9PFileSystem* fs = 0;
	xhnd oh = 0;
	if (!GetTrueHND(hnd, fs, oh))
	{
		callback(XERR_XHND_INVALID);
		return;
	}

	fs->Twstat(oh, stat, callback);
}
