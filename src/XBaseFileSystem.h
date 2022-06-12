#include "X9PFileSystem.h"
#include <unordered_map>
#include <assert.h>
#include "XLogging.h"


template<typename T>
class XBaseFileSystem : public X9PFileSystem
{
public:


	XBaseFileSystem()
	{
		m_hidserial = 0;
	}



	virtual void Tclunk(xhnd hnd, Rclunk_fn callback)
	{
		m_handles.erase(hnd->id);
		hnd->id = XHID_UNINITIALIZED;
		callback(0);
	}


	bool GetHND(xhnd hnd, T*& out)
	{
		if (hnd->id == XHID_UNINITIALIZED)
		{
			out = 0;
			return false;
		}

		auto f = m_handles.find(hnd->id);
		if (f == m_handles.end())
		{
			out = 0;
			return false;
		}
		out = &f->second;
		return true;
	}


	void TagHND(xhnd hnd, T&& value) { TagHND(hnd, value); }
	void TagHND(xhnd hnd, T& value)
	{
		assert(hnd->id == XHID_UNINITIALIZED);

		// Find a free id
		auto f = m_handles.find(m_hidserial);
		while(f != m_handles.end())
		{
			m_hidserial++;
		}

		// Assign the id
		xhid id = m_hidserial;
		hnd->id = id;

		XPRINTF("Tagged hnd %llu\n", m_hidserial);

		// Track it
		m_handles.insert({ id, value });
		m_hidserial++;

	}

	void UntagHND(xhnd hnd)
	{
		XPRINTF("Untagged hnd %llu\n", m_hidserial);

		m_handles.erase(hnd->id);
		hnd->id = XHID_UNINITIALIZED;
	}

protected:

	// FIXME: Move this to be something like per auth
	std::unordered_map<xhid, T> m_handles;
	xhid m_hidserial;
};