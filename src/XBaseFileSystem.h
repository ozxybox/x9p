#include "X9PFileSystem.h"
#include <unordered_map>
#include <assert.h>



template<typename T>
class XBaseFileSystem : public X9PFileSystem
{
public:

	class XAuthEntry
	{
	public:
		XAuth* m_auth;
		T m_value;
	};


	XBaseFileSystem()
	{
		m_hndserial = 0;

	}

	// These are used instead of FIDs, as FIDs are per connection

	virtual T NullEntry() = 0;

	virtual xhnd NewFileHandle(XAuth* user)
	{
		xhnd hnd = m_hndserial++;
		XAuthEntry e{ user, NullEntry() };
		m_handles.emplace(hnd, e);
		return hnd;
	}

	virtual xhnd DeriveFileHandle(xhnd hnd)
	{
		auto f = m_handles.find(hnd);
		if (f == m_handles.end())
		{
			assert(0);
			return 0xFFFFFFFF;
		}

		xhnd newhnd = m_hndserial++;
		XAuthEntry e{f->second.m_auth, NullEntry() };
		m_handles.emplace(newhnd, e);
		return newhnd;
	}

	virtual void ReleaseFileHandle(XAuth* user, xhnd hnd)
	{
		// FIXME: Implement
	}

	virtual void Tclunk(xhnd hnd, Rclunk_fn callback)
	{
		m_handles.erase(hnd);
		callback(0);
	}

	// FIXME: Rename this to GetHNDValue
	bool isValidXHND(xhnd hnd, T& out)
	{
		auto f = m_handles.find(hnd);
		if (f == m_handles.end())
		{
			out = 0;
			return false;
		}
		out = f->second.m_value;
		return true;
	}


	bool GetHND(xhnd hnd, XAuthEntry*& out)
	{
		auto f = m_handles.find(hnd);
		if (f == m_handles.end())
		{
			out = 0;
			return false;
		}
		out = &f->second;
		return true;
	}



protected:
	std::unordered_map<xhnd, XAuthEntry> m_handles;
	xhnd m_hndserial;
};