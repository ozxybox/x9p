#include "X9PClient.h"
#include "XLogging.h"
#include <assert.h>



xfiddata_t::xfiddata_t()
	: refcount(0)
{

}

xfiddata_t::~xfiddata_t()
{
	for (Twrite_fn* hook : hooks)
		delete hook;
}

void bigbad()
{
	printf("oh no! oh no!\n");
}

void xsentmsg_clear(xreqmsg_t* qmsg)
{
	// We need to call the correct deconstructor
	if(qmsg->callback.ptr)
	switch (qmsg->type)
	{
	case X9P_TVERSION: delete qmsg->callback.version; break;
  //case X9P_TAUTH:    delete qmsg->callback.auth;    break;
	case X9P_TATTACH:  delete qmsg->callback.attach;  break;
  //case X9P_TERROR:   delete qmsg->callback.error;   break;
  //case X9P_TFLUSH:   delete qmsg->callback.flush;   break;
	case X9P_TWALK:    delete qmsg->callback.walk;    break;
	case X9P_TOPEN:    delete qmsg->callback.open;    break;
	case X9P_TCREATE:  delete qmsg->callback.create;  break;
	case X9P_TREAD:    delete qmsg->callback.read;    break;
	case X9P_TWRITE:   delete qmsg->callback.write;   break;
	case X9P_TCLUNK:   delete qmsg->callback.clunk;   break;
	case X9P_TREMOVE:  delete qmsg->callback.remove;  break;
	case X9P_TSTAT:    delete qmsg->callback.stat;    break;
	case X9P_TWSTAT:   delete qmsg->callback.wstat;   break;
	}
}




X9PClient::X9PClient()
{
	// Start with a default buf with just enough space for a Tversion
	m_sendbuffer = (char*)malloc(32);
	m_recvbuffer = (char*)malloc(32);
	m_maxmessagesize = 32;
	m_authserial = 0;
	m_partialrecvoffset = 0;
	m_partialrecvsize = 0;
}

X9PClient::X9PClient(TCPClientSocket sock) : X9PClient()
{
	m_socket = sock;
}

X9PClient::~X9PClient()
{
	//free(m_sendbuffer);

}

void X9PClient::End()
{
	m_socket.Close();
	free(m_sendbuffer);
	free(m_recvbuffer);
	for (auto p : m_requestmessages)
	{
		xsentmsg_clear(&p.second);
	}
}

xerr_t X9PClient::Begin(const char* ip, const char* port)
{
	ISOCKET_RESULT r = m_socket.Connect(ip, port);
	if (r != ISOCKET_RESULT::OK)
		return "Begin: Failed to connect to remote host";

	m_socket.SetNonblocking(true);

	printf("Connected!\n");

	m_currentTag = 0;


	// Ask the server about our protocol and negotiate a max msg size
	uint32_t msize = 0;
	xstr_t serverver = 0;

	xstr_t ver = xstrndup(X9P_VERSION, X9P_VERSION_LEN);

	xerr_t errv = 0;
	bool responded = false;

	Tversion(X9P_MSIZE_DEFAULT, ver, [&](xerr_t err, uint32_t msize, xstr_t version) {
	
		responded = true;

		if (err)
			errv = "Begin: Server responded with error?";

		// Check reponses
		if (msize > X9P_MSIZE_DEFAULT)
		{
			errv = "Begin: Server cannot negotiate a packet size larger than the client's";
			return;
		}

		// We only support one version of 9P for now
		if (xstrcmp(version, ver) != 0)
		{
  			errv = "Begin: Server protocol unsupported";
			return;
		}

		// We've negotiated a max message size with the server, so one big buffer can get used for all messages now
		m_maxmessagesize = msize;
		free(m_sendbuffer);
		free(m_recvbuffer);
		m_sendbuffer = (char*)malloc(m_maxmessagesize);
		m_recvbuffer = (char*)malloc(m_maxmessagesize);
		
	});

	
	while (!responded)
		ProcessPackets();
	
	free(ver);


	// NOTE: The msize should *never* be smaller than our largest message!
	return errv;
}


xerr_t X9PClient::ProcessPackets()
{
	// Send off all outstanding requests before checking for responses
	FlushRequests();

	// What'd the server say?
	message_t* recvmsg = (message_t*)m_recvbuffer;
	do
	{
		bool wouldblock = false;
		xerr_t err = RecvResponse(wouldblock);
		if (wouldblock) return 0;
		if (err) return err;

		if (recvmsg->tag == NOTAG && recvmsg->type == X9P_TWRITE)
		{

			Twrite_t* wm = (Twrite_t*)recvmsg;
			xfiddata_t* fd = m_fiddata[wm->fid];
			printf("Got the weird msg! %d\n", wm->fid);

			// Call all attached hooks
			for (auto& c : fd->hooks)
			{
				c->operator()(0, wm->offset, wm->count, wm->data());
			}

			continue;
		}

		xreqmsg_t* qmsg = &m_requestmessages[recvmsg->tag];

		
		if(qmsg->callback.ptr)
		{
			switch (qmsg->type)
			{
			case X9P_TVERSION: Rversion(recvmsg, qmsg); delete qmsg->callback.version; break;
		  //case X9P_TAUTH:    Rauth   (recvmsg, qmsg); delete qmsg->callback.auth;    break;
			case X9P_TATTACH:  Rattach (recvmsg, qmsg); delete qmsg->callback.attach;  break;
		  //case X9P_TERROR:   Rerror  (recvmsg, qmsg); delete qmsg->callback.error;   break;
		  //case X9P_TFLUSH:   Rflush  (recvmsg, qmsg); delete qmsg->callback.flush;   break;
			case X9P_TWALK:    Rwalk   (recvmsg, qmsg); delete qmsg->callback.walk;    break;
			case X9P_TOPEN:    Ropen   (recvmsg, qmsg); delete qmsg->callback.open;    break;
			case X9P_TCREATE:  Rcreate (recvmsg, qmsg); delete qmsg->callback.create;  break;
			case X9P_TREAD:    Rread   (recvmsg, qmsg); delete qmsg->callback.read;    break;
			case X9P_TWRITE:   Rwrite  (recvmsg, qmsg); delete qmsg->callback.write;   break;
			case X9P_TCLUNK:   Rclunk  (recvmsg, qmsg); delete qmsg->callback.clunk;   break;
			case X9P_TREMOVE:  Rremove (recvmsg, qmsg); delete qmsg->callback.remove;  break;
			case X9P_TSTAT:    Rstat   (recvmsg, qmsg); delete qmsg->callback.stat;    break;
			case X9P_TWSTAT:   Rwstat  (recvmsg, qmsg); delete qmsg->callback.wstat;   break;
			}
		}

		m_requestmessages.erase(qmsg->tag);

	} while (1);


}


xerr_t X9PClient::SendMessage(message_t* m)
{
	ISOCKET_RESULT r;
	r = m_socket.Send((char*)m, m->size);
	XPRINTF("\x1b[36mSent %u %s %u\x1b[39m\n", m->size, X9PMessageTypeName(m->type), m->tag);
	if (r != ISOCKET_RESULT::OK) return "SendMessage: Failed";
	return 0;
}


void X9PClient::FlushRequests()
{
	std::vector<xqueuedmsg_t> unsent;

	// Ship off whatever's in the queue and then clear it
	for (int i = 0; i < m_queue.size(); i++)
	{
		xqueuedmsg_t* q = m_queue.data() + i;
		xerr_t err = SendMessage(q->message);

		if (err)
		{
			// If we fail to send the message, hold on to it for later
			printf("Send failure??!\n\t");
			puts(err);
			unsent.push_back(*q);
		}
		else
		{
			// Remember that we sent this message
			xreqmsg_t s = { q->message->type, q->message->tag, q->callback };
			m_requestmessages.emplace(q->message->tag, s);

			delete q->message;
		}

	}

	m_queue = unsent;
	//m_queue.clear();
}

xerr_t X9PClient::RecvResponse(bool& wouldblock)
{
	// TODO: Up the recv buffer to something like 8x the size of of max msg
	//       Recv as much as possible, and process each packet until no bytes remain
	//       Only when a partial packet appears, or when the buffer is empty, read more data
	//       A malicious client could load up something like a hundred clunks into a buffer, so we should only 
	//       process a set number of msgs at a time before returning to check on our other clients
	
	// What'd we get back from the server?
	size_t written;
	ISOCKET_RESULT r = ISOCKET_RESULT::OK;
	
	// Check for a partial
	if (m_partialrecvsize > 0)
	{
		// Push it forward
		memmove(m_recvbuffer, m_recvbuffer + m_partialrecvoffset, m_partialrecvsize);
		written = m_partialrecvsize;
	
		// Do we have enough for at least 1 msg?
		if (written < sizeof(message_t))
		{
			// We don't. Pull some more data
			size_t more = 0;
			r = m_socket.Recv((char*)m_recvbuffer + written, m_maxmessagesize - written, more);

			written += more;
		}

	}
	else
	{
		// Full buf recv
		r = m_socket.Recv((char*)m_recvbuffer, m_maxmessagesize, written);
	}


	message_t* msg = (message_t*)m_recvbuffer;

	// If it'll block, just return back. Nothing we can do here
	if (r == ISOCKET_RESULT::WOULDBLOCK)
	{
		wouldblock = true;
		return 0;
	}
	wouldblock = false;

	// Did we receive ok?
	if (r != ISOCKET_RESULT::OK)
		return "RecvResponse: Failed to receive from socket";

	// Are we still connected?
	if (written == 0)
		return "RecvResponse: Disconnected from the other end";

	// Is it at least big enough for us to read?
	if (written < sizeof(message_t))
		return "RecvResponse: Packet smaller than a message_t";

	// Is this message trying to lie about its size?
	if (msg->size > m_maxmessagesize || msg->size < sizeof(message_t))
		return "RecvResponse: Incorrect reported message size";


	XPRINTF("\x1b[36mReceived %u %s %u\x1b[39m\n", msg->size, X9PMessageTypeName(msg->type), msg->tag);
	
	if (msg->size < written)
	{
		// We got part of another message
		// Store the offset and size
		m_partialrecvoffset = msg->size;
		m_partialrecvsize = written - m_partialrecvoffset;
	}
	else
	{
		m_partialrecvoffset = 0;
		m_partialrecvsize = 0;
	}
	
	return 0;
}




void X9PClient::FlushResponses()
{
	std::vector<xqueuedmsg_t> unsent;

	// Ship off whatever's in the queue and then clear it
	for (int i = 0; i < m_queue.size(); i++)
	{
		xqueuedmsg_t* q = m_queue.data() + i;
		xerr_t err = SendMessage(q->message);

		if (err)
		{
			// If we fail to send the message, hold on to it for later
			printf("Send failure??!\n\t");
			puts(err);
			unsent.push_back(*q);
		}
		else
		{
			delete q->message;
		}

	}

	m_queue = unsent;
	//m_queue.clear();
}

void X9PClient::QueueRequest(message_t* msg, xmsgcallback_t callback)
{
	assert(msg->size < m_maxmessagesize);

	// TODO: Instead of allocating small blocks, have our queue be just a big buffer we can write to and send
	message_t* nm = (message_t*)memcpy(malloc(msg->size), msg, msg->size);
	xqueuedmsg_t q = { nm, callback };
	m_queue.push_back(q);
}


bool checkqueuefortag_deletethis(std::vector<xqueuedmsg_t>& m_queue, mtag_t tag)
{
	for (auto& q : m_queue)
		if (q.message->tag == tag)
			return true;
	return false;
}

mtag_t X9PClient::NewTag()
{
	// Find a new tag and make sure it's not already in use.
	m_currentTag = 0;
	mtag_t tag;
	do
	{
		tag = m_currentTag++;
	} while (m_requestmessages.find(tag) != m_requestmessages.end() || checkqueuefortag_deletethis(m_queue, tag));

	return tag;
}


fid_t X9PClient::NewFID()
{
	// Find a new fid and make sure it's not already in use.
	fid_t fid = m_fiddata.size();
	while (m_fiddata.find(fid) != m_fiddata.end())
		fid++;

	m_fiddata.insert({ fid, new xfiddata_t()});

	return fid;
}


// Server
void X9PClient::QueueResponse(message_t* msg)
{
	// TODO: Instead of allocating small blocks, have our queue be just a big buffer we can write to and send
	message_t* nm = (message_t*)memcpy(malloc(msg->size), msg, msg->size);

	// TODO: split this up better so we aren't wasting memory with callback pointers
	xqueuedmsg_t q = { nm, 0 };
	m_queue.push_back(q);

}
xerr_t X9PClient::RecvRequest(bool& wouldblock)
{
	xerr_t err = RecvResponse(wouldblock);
	if (wouldblock) return 0;
	//if (err) 
	return err;
	

	//message_t* msg = (message_t*)m_recvbuffer;
	//m_requestmessages.insert(m_recvbuffer)

}



// RPC calls
void X9PClient::Tversion(uint32_t messagesize, xstr_t version, Rversion_fn callback)
{
	uint32_t sz = sizeof(Tversion_t) + version->size();
	if (sz > m_maxmessagesize)
	{
		callback("Version: Request is too large", 0, 0);
		return;
	}

	// Compose the request
	Tversion_t* req = (Tversion_t*)m_sendbuffer;
	req->size = sz;
	req->type = X9P_TVERSION;
	req->tag = NOTAG;
	req->msize = messagesize;
	xstrcpy(req->version(), version);


	xmsgcallback_t c;
	c.version = new Rversion_fn(callback);
	QueueRequest(req, c);
}
// Tauth
// Tflush
void X9PClient::Tattach(xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback)
{
	uint32_t sz = sizeof(Tattach_t) + username->size() + accesstree->size();
	if (sz > m_maxmessagesize)
	{
		callback("Attach: Request is too large", 0);
		return;
	}

	if (hnd->id == XHID_UNINITIALIZED)
		hnd->id = NewFID();

	// Compose the request
	Tattach_t* req = (Tattach_t*)m_sendbuffer;
	req->size = sz;
	req->type = X9P_TATTACH;
	req->tag = NewTag();

	req->fid = hnd->id;
	req->afid = ahnd ? ahnd->id : NOFID;

	xstrcpy(req->uname(), username);
	xstrcpy(req->aname(), accesstree);


	xmsgcallback_t c;
	c.attach = new Rattach_fn(callback);
	QueueRequest(req, c);
}

void X9PClient::Twalk(xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback)
{
	// TODO: Ensure we aren't sending any '.'s


	if (hnd->id == XHID_UNINITIALIZED)
	{
		callback(XERR_FID_DNE, 0, 0);
		return;
	}

	if (nwname > 16)
	{
		callback("Walk: Number of walk requests too large! Cannot be more than 16!", 0, 0);
		return;
	}

	uint32_t wnsz = 0;
	int wi = 0;
	for (xstr_t w = wname; wi < nwname; w = xstrnext(w))
	{
		wnsz += w->size();
		wi++;
	}

	uint32_t sz = sizeof(Twalk_t) + wnsz;
	if (sz > m_maxmessagesize)
	{
		callback("Walk: Request is too large", 0, 0);
		return;
	}

	if (newhnd->id == XHID_UNINITIALIZED)
		newhnd->id = NewFID();

	// Compose the request
	Twalk_t* req = (Twalk_t*)m_sendbuffer;
	req->size = sz;
	req->type = X9P_TWALK;
	req->tag = NewTag();
	req->fid = hnd->id;
	req->newfid = newhnd->id;
	req->nwname = nwname;

	wi = 0;
	xstr_t d = req->wname();
	for (xstr_t w = wname; wi < nwname; w = xstrnext(w))
	{
		d->len = UINT16_MAX;
		xstrcpy(d, w);
		d = xstrnext(d);
		
		wnsz += w->len;
		wi++;
	}

	xmsgcallback_t c;
	c.walk = new Rwalk_fn(callback);
	QueueRequest(req, c);
}

void X9PClient::Topen(xhnd hnd, openmode_t mode, Ropen_fn callback)
{
	// Compose the request
	Topen_t* req = (Topen_t*)m_sendbuffer;
	req->size = sizeof(Topen_t);
	req->type = X9P_TOPEN;
	req->tag = NewTag();
	req->fid = hnd->id;
	req->mode = mode;


	xmsgcallback_t c;
	c.open = new Ropen_fn(callback);
	QueueRequest(req, c);
}

void X9PClient::Tcreate(xhnd hnd, xstr_t name, dirmode_t perm, openmode_t mode, Rcreate_fn callback)
{
	// Compose the request
	Tcreate_t* req = (Tcreate_t*)m_sendbuffer;
	req->size = sizeof(Tcreate_t) + name->size() + sizeof(dirmode_t) + sizeof(openmode_t);
	req->type = X9P_TCREATE;
	req->tag = NewTag();
	req->fid = hnd->id;

	// FIXME: totally botches the name if the max size is bad
	char* end = (char*)(m_sendbuffer + m_maxmessagesize - sizeof(dirmode_t) + sizeof(openmode_t));
	xstrncpy_nocheck(req->name(), name, end - (char*)req->name() );
	
	*req->perm() = perm;
	*req->mode() = mode;

	xmsgcallback_t c;
	c.create = new Rcreate_fn(callback);
	QueueRequest(req, c);
}

void X9PClient::Tread(xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)
{
	// Compose the request
	Tread_t* req = (Tread_t*)m_sendbuffer;
	req->size = sizeof(Tread_t);
	req->type = X9P_TREAD;
	req->tag = NewTag();
	req->fid = hnd->id;
	req->offset = offset;
	req->count = count;


	xmsgcallback_t c;
	c.read = new Rread_fn(callback);
	QueueRequest(req, c);
}

void X9PClient::Twrite(xhnd hnd, uint64_t offset, uint32_t count, void* data, Rwrite_fn callback)
{
	uint32_t sz = sizeof(Twrite_t) + count;
	if (sz > m_maxmessagesize)
	{
		callback("Write: Request is too large", 0);
		return;
	}

	// Compose the request
	Twrite_t* req = (Twrite_t*)m_sendbuffer;
	req->size = sz;
	req->type = X9P_TWRITE;
	req->tag = NewTag();
	req->fid = hnd->id;
	req->offset = offset;
	req->count = count;
	memcpy(req->data(), data, count);


	xmsgcallback_t c;
	c.write = new Rwrite_fn(callback);
	QueueRequest(req, c);
}

void X9PClient::Tclunk(xhnd hnd, Rclunk_fn callback)
{
	// Compose the request
	Tclunk_t* req = (Tclunk_t*)m_sendbuffer;
	req->size = sizeof(Tclunk_t);
	req->type = X9P_TCLUNK;
	req->tag = NewTag();
	req->fid = hnd->id;

	xmsgcallback_t c;
	c.clunk = new Rclunk_fn(callback);
	QueueRequest(req, c);

	// Clear out the clunked fid
	auto f = m_fiddata.find(hnd->id);
	delete f->second;
	m_fiddata.erase(f);
}

void X9PClient::Tremove(xhnd hnd, Rremove_fn callback)
{
	// Compose the request
	Tremove_t* req = (Tremove_t*)m_sendbuffer;
	req->size = sizeof(Tremove_t);
	req->type = X9P_TREMOVE;
	req->tag = NewTag();
	req->fid = hnd->id;

	xmsgcallback_t c;
	c.remove = new Rremove_fn(callback);
	QueueRequest(req, c);

	// Clear out the clunked fid
	auto f = m_fiddata.find(hnd->id);
	delete f->second;
	m_fiddata.erase(f);
}

void X9PClient::Tstat(xhnd hnd, Rstat_fn callback)
{
	// Compose the request
	Tstat_t* req = (Tstat_t*)m_sendbuffer;
	req->size = sizeof(Tstat_t);
	req->type = X9P_TREMOVE;
	req->tag = NewTag();
	req->fid = hnd->id;

	xmsgcallback_t c;
	c.stat = new Rstat_fn(callback);
	QueueRequest(req, c);
}

void X9PClient::Twstat(xhnd hnd, stat_t* stat, Rwstat_fn callback)
{
	// Compose the request
	Twstat_t* req = (Twstat_t*)m_sendbuffer;
	req->size = sizeof(Twstat_t);
	req->type = X9P_TREMOVE;
	req->tag = NewTag();
	req->fid = hnd->id;
	memcpy(&req->stat, stat, sizeof(stat_t));

	xmsgcallback_t c;
	c.wstat = new Rwstat_fn(callback);
	QueueRequest(req, c);
}


char* GetRerror(message_t* msg)
{
	if (msg->type == X9P_RERROR)
	{
		void* end = reinterpret_cast<void*>(reinterpret_cast<char*>(msg) + msg->size);

		Rerror_t* resp = (Rerror_t*)msg;
		if (!xstrsafe(resp->ename(), end))
		{
			bigbad();
			return strdup("FAILED TO READ ERROR!");
		}

		char* err = xstrcstr(resp->ename());
		return err;
	}
	return 0;
}

void X9PClient::Rversion(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.version)(err, 0, 0);
		free(err);
		return;
	}

	Rversion_t* resp = (Rversion_t*)respmsg;
	void* end = reinterpret_cast<void*>(reinterpret_cast<char*>(resp) + resp->size);

	// Always perform size checks first
	if (resp->size < sizeof(Rversion_t) + sizeof(xstrlen_t))
		(*requestmsg->callback.version)("Rversion: Response too small", 0, 0);
	else if (!xstrsafe(resp->version(), end))
		(*requestmsg->callback.version)("Rversion: Received invalid string size", 0, 0);
	else // It's clean!
		(*requestmsg->callback.version)(0, resp->msize, resp->version());
}
void X9PClient::Rattach(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.attach)(err, 0);
		free(err);
		return;
	}

	Rattach_t* resp = (Rattach_t*)respmsg;
	
	// Always perform size checks first
	if (resp->size < sizeof(Rattach_t))
		(*requestmsg->callback.attach)("Rattach: Response too small", 0);
	else if (resp->qid.type & X9P_QT_AUTH)
		(*requestmsg->callback.attach)("Rattach: Authentication not supported", 0);
	else 
		(*requestmsg->callback.attach)(0, &resp->qid);
}
void X9PClient::Rwalk(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.walk)(err, 0, 0);
		free(err);
		return;
	}

	Rwalk_t* resp = (Rwalk_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rwalk_t))
		(*requestmsg->callback.walk)("Rwalk: Response too small", 0, 0);
	else
		(*requestmsg->callback.walk)(0, resp->nwqid, resp->wqid());
}
void X9PClient::Ropen(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.open)(err, 0, 0);
		free(err);
		return;
	}

	Ropen_t* resp = (Ropen_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Ropen_t))
		(*requestmsg->callback.open)("Ropen: Response too small", 0, 0);
	else
		(*requestmsg->callback.open)(0, &resp->qid, resp->iounit);
}
void X9PClient::Rcreate(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.create)(err, 0, 0);
		free(err);
		return;
	}

	Rcreate_t* resp = (Rcreate_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rcreate_t))
		(*requestmsg->callback.create)("Ropen: Response too small", 0, 0);
	else
		(*requestmsg->callback.create)(0, &resp->qid, resp->iounit);
}
void X9PClient::Rread(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.read)(err, 0, 0);
		free(err);
		return;
	}
	
	Rread_t* resp = (Rread_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rread_t))
		(*requestmsg->callback.read)("Rread: Response too small", 0, 0);
	if (resp->count + sizeof(Rread_t) > resp->size)
		(*requestmsg->callback.read)("Rread: Response count too large", 0, 0);
	else
		(*requestmsg->callback.read)(0, resp->count, resp->data());
}
void X9PClient::Rwrite(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.write)(err, 0);
		free(err);
		return;
	}

	Rwrite_t* resp = (Rwrite_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rwrite_t))
		(*requestmsg->callback.write)("Ropen: Response too small", 0);
	else
		(*requestmsg->callback.write)(0, resp->count);
}
void X9PClient::Rclunk(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.clunk)(err);
		free(err);
		return;
	}

	Rclunk_t* resp = (Rclunk_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rclunk_t))
		(*requestmsg->callback.clunk)("Rclunk: Response too small");
	else
		(*requestmsg->callback.clunk)(0);
}
void X9PClient::Rremove(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.remove)(err);
		free(err);
		return;
	}

	Rremove_t* resp = (Rremove_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rremove_t))
		(*requestmsg->callback.remove)("Rremove: Response too small");
	else
		(*requestmsg->callback.remove)(0);
}
void X9PClient::Rstat(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.remove)(err);
		free(err);
		return;
	}

	Rstat_t* resp = (Rstat_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rstat_t))
		(*requestmsg->callback.stat)("Rstat: Response too small", 0);
	else
		(*requestmsg->callback.stat)(0, &resp->stat);
}
void X9PClient::Rwstat(message_t* respmsg, xreqmsg_t* requestmsg)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*requestmsg->callback.wstat)(err);
		free(err);
		return;
	}

	Rwstat_t* resp = (Rwstat_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rwstat_t))
		(*requestmsg->callback.wstat)("Rwstat: Response too small");
	else
		(*requestmsg->callback.wstat)(0);
}

