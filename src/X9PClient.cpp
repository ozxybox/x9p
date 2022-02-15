#include "X9PClient.h"
#include "XLogging.h"

#define DEFAULT_MSIZE 8192


void bigbad()
{
	printf("oh no! oh no!\n");
}

void xqueuedmsg_clear(xqueuedmsg_t* qmsg)
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
	m_xhndserial = 0;
}

X9PClient::X9PClient(TCPClientSocket sock) : X9PClient()
{
	m_socket = sock;
	m_xhndserial = 0;
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
	for (auto p : m_messages)
	{
		xqueuedmsg_clear(&p.second);
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

	Tversion(DEFAULT_MSIZE, ver, [&](xerr_t err, uint32_t msize, xstr_t version) {
	
		responded = true;

		if (err)
			errv = "Begin: Server responded with error?";

		// Check reponses
		if (msize > DEFAULT_MSIZE)
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
	// What'd the server say?

	message_t* recvmsg = (message_t*)m_recvbuffer;
	do
	{
		bool wouldblock = false;
		xerr_t err = RecvMessage(wouldblock);
		if (wouldblock) return 0;
		if (err) return err;

		xqueuedmsg_t* qmsg = &m_messages[recvmsg->tag];

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

		m_messages.erase(qmsg->tag);
	} while (1);


}

xerr_t X9PClient::RecvMessage(bool& wouldblock)
{

	// What'd we get back from the server?
	size_t written;
	ISOCKET_RESULT r = m_socket.Recv((char*)m_recvbuffer, sizeof(message_t), written);

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
		return "RecvMessage: Failed to receive from socket";

	// Are we still connected?
	if (written == 0)
		return "RecvMessage: Disconnected from the other end";

	// Is it at least big enough for us to read?
	if (written < sizeof(message_t))
		return "RecvMessage: Packet smaller than a message_t";

	// Is this message trying to lie about its size?
	if (msg->size > m_maxmessagesize)
		return "RecvMessage: Incorrect reported message size";

	if (msg->size > sizeof(message_t))
	{
		// We need more
		while (written < msg->size)
		{
			XPRINTF("reading more\n");
			size_t w;
			r = m_socket.Recv((char*)m_recvbuffer + written, msg->size - written, w);
			written += w;
		}
	}

	XPRINTF("Received %u %s %u\n", msg->size, X9PMessageTypeName(msg->type), msg->tag);
	
	return 0;
}






void X9PClient::QueueMessage(message_t* msg, xmsgcallback_t callback)
{
	xqueuedmsg_t q = { msg->type, msg->tag, callback };
	m_messages.emplace(msg->tag, q);
	SendMessage(msg);
}


xerr_t X9PClient::SendMessage(message_t* msg)
{
	XPRINTF("Sent %u %s %u\n", msg->size, X9PMessageTypeName(msg->type), msg->tag);

	ISOCKET_RESULT r;
	r = m_socket.Send((char*)msg, msg->size);
	if (r != ISOCKET_RESULT::OK) return "SendMessage: Failed";
	return 0;
}


mtag_t X9PClient::NewTag()
{
	return m_currentTag++;
}
xhnd X9PClient::NewFileHandle(int connection)
{
	/*
	size_t sz = m_fids.size();
	if (sz >= UINT32_MAX)
	{
		// This hasn't happened yet, but if it does, I will cry
		printf("\n\n----! Completely out of FIDs???? HOW??? !----\n\n");
		return UINT32_MAX;
	}

	fid_t fid = (fid_t)sz + 1;

	// Is this actually in use?
	while (m_fids.find(fid) != m_fids.end())
		fid++;

	// Put it in to our used fids list
	m_fids.emplace(fid, m_xhndserial++);
	*/
	return m_xhndserial++;
}



#if 0

xerr_t X9PClient::Clunk(fid_t fid)
{
	// First check if it exists in our fid table
	auto f = m_fids.find(fid);

	// Doesn't exist? No error
	if (f == m_fids.end())
		return 0;
	
	// Not mapped yet? No error
	if (f->second == 0)
	{
		m_fids.erase(f);
		return 0;
	}

}
#endif


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
	QueueMessage(req, c);
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

	// Compose the request
	Tattach_t* req = (Tattach_t*)m_sendbuffer;
	req->size = sz;
	req->type = X9P_TATTACH;
	req->tag = NewTag();
	req->fid = hnd;
	req->afid = ahnd;

	xstrcpy(req->uname(), username);
	xstrcpy(req->aname(), accesstree);


	xmsgcallback_t c;
	c.attach = new Rattach_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Twalk(xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback)
{
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

	// Compose the request
	Twalk_t* req = (Twalk_t*)m_sendbuffer;
	req->size = sz;
	req->type = X9P_TWALK;
	req->tag = NewTag();
	req->fid = hnd;
	req->newfid = newhnd;
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
	QueueMessage(req, c);
}

void X9PClient::Topen(xhnd hnd, openmode_t mode, Ropen_fn callback)
{
	// Compose the request
	Topen_t* req = (Topen_t*)m_sendbuffer;
	req->size = sizeof(Topen_t);
	req->type = X9P_TOPEN;
	req->tag = NewTag();
	req->fid = hnd;
	req->mode = mode;


	xmsgcallback_t c;
	c.open = new Ropen_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Tcreate(xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback)
{
	// Compose the request
	Tcreate_t* req = (Tcreate_t*)m_sendbuffer;
	req->size = sizeof(Tcreate_t) + name->size() + sizeof(uint32_t) + sizeof(openmode_t);
	req->type = X9P_TCREATE;
	req->tag = NewTag();
	req->fid = hnd;
	xstrcpy(req->name(), name);
	*req->perm() = perm;
	*req->mode() = mode;

	xmsgcallback_t c;
	c.create = new Rcreate_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Tread(xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback)
{
	// Compose the request
	Tread_t* req = (Tread_t*)m_sendbuffer;
	req->size = sizeof(Tread_t);
	req->type = X9P_TREAD;
	req->tag = NewTag();
	req->fid = hnd;
	req->offset = offset;
	req->count = count;


	xmsgcallback_t c;
	c.read = new Rread_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Twrite(xhnd hnd, uint64_t offset, uint32_t count, uint8_t* data, Rwrite_fn callback)
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
	req->fid = hnd;
	req->offset = offset;
	req->count = count;
	memcpy(req->data(), data, count);


	xmsgcallback_t c;
	c.write = new Rwrite_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Tclunk(xhnd hnd, Rclunk_fn callback)
{
	// Compose the request
	Tclunk_t* req = (Tclunk_t*)m_sendbuffer;
	req->size = sizeof(Tclunk_t);
	req->type = X9P_TCLUNK;
	req->tag = NewTag();
	req->fid = hnd;

	xmsgcallback_t c;
	c.clunk = new Rclunk_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Tremove(xhnd hnd, Rremove_fn callback)
{
	// Compose the request
	Tremove_t* req = (Tremove_t*)m_sendbuffer;
	req->size = sizeof(Tremove_t);
	req->type = X9P_TREMOVE;
	req->tag = NewTag();
	req->fid = hnd;

	xmsgcallback_t c;
	c.remove = new Rremove_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Tstat(xhnd hnd, Rstat_fn callback)
{
	// Compose the request
	Tstat_t* req = (Tstat_t*)m_sendbuffer;
	req->size = sizeof(Tstat_t);
	req->type = X9P_TREMOVE;
	req->tag = NewTag();
	req->fid = hnd;

	xmsgcallback_t c;
	c.stat = new Rstat_fn(callback);
	QueueMessage(req, c);
}

void X9PClient::Twstat(xhnd hnd, stat_t* stat, Rwstat_fn callback)
{
	// Compose the request
	Twstat_t* req = (Twstat_t*)m_sendbuffer;
	req->size = sizeof(Twstat_t);
	req->type = X9P_TREMOVE;
	req->tag = NewTag();
	req->fid = hnd;
	memcpy(&req->stat, stat, sizeof(stat_t));

	xmsgcallback_t c;
	c.wstat = new Rwstat_fn(callback);
	QueueMessage(req, c);
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

void X9PClient::Rversion(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.version)(err, 0, 0);
		free(err);
		return;
	}

	Rversion_t* resp = (Rversion_t*)respmsg;
	void* end = reinterpret_cast<void*>(reinterpret_cast<char*>(resp) + resp->size);

	// Always perform size checks first
	if (resp->size < sizeof(Rversion_t) + sizeof(xstrlen_t))
		(*queued->callback.version)("Rversion: Response too small", 0, 0);
	else if (!xstrsafe(resp->version(), end))
		(*queued->callback.version)("Rversion: Received invalid string size", 0, 0);
	else // It's clean!
		(*queued->callback.version)(0, resp->msize, resp->version());
}
void X9PClient::Rattach(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.attach)(err, 0);
		free(err);
		return;
	}

	Rattach_t* resp = (Rattach_t*)respmsg;
	
	// Always perform size checks first
	if (resp->size < sizeof(Rattach_t))
		(*queued->callback.attach)("Rattach: Response too small", 0);
	else if (resp->qid.type & X9P_QT_AUTH)
		(*queued->callback.attach)("Rattach: Authentication not supported", 0);
	else 
		(*queued->callback.attach)(0, &resp->qid);
}
void X9PClient::Rwalk(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.walk)(err, 0, 0);
		free(err);
		return;
	}

	Rwalk_t* resp = (Rwalk_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rwalk_t))
		(*queued->callback.walk)("Rwalk: Response too small", 0, 0);
	else
		(*queued->callback.walk)(0, resp->nwqid, resp->wqid());
}
void X9PClient::Ropen(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.open)(err, 0, 0);
		free(err);
		return;
	}

	Ropen_t* resp = (Ropen_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Ropen_t))
		(*queued->callback.open)("Ropen: Response too small", 0, 0);
	else
		(*queued->callback.open)(0, &resp->qid, resp->iounit);
}
void X9PClient::Rcreate(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.create)(err, 0, 0);
		free(err);
		return;
	}

	Rcreate_t* resp = (Rcreate_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rcreate_t))
		(*queued->callback.create)("Ropen: Response too small", 0, 0);
	else
		(*queued->callback.create)(0, &resp->qid, resp->iounit);
}
void X9PClient::Rread(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.read)(err, 0, 0);
		free(err);
		return;
	}

	Rread_t* resp = (Rread_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rread_t))
		(*queued->callback.read)("Rread: Response too small", 0, 0);
	if (resp->count + sizeof(Rread_t) > resp->size)
		(*queued->callback.read)("Rread: Response count too large", 0, 0);
	else
		(*queued->callback.read)(0, resp->count, resp->data());
}
void X9PClient::Rwrite(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.write)(err, 0);
		free(err);
		return;
	}

	Rwrite_t* resp = (Rwrite_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rwrite_t))
		(*queued->callback.write)("Ropen: Response too small", 0);
	else
		(*queued->callback.write)(0, resp->count);
}
void X9PClient::Rclunk(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.clunk)(err);
		free(err);
		return;
	}

	Rclunk_t* resp = (Rclunk_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rclunk_t))
		(*queued->callback.clunk)("Rclunk: Response too small");
	else
		(*queued->callback.clunk)(0);
}
void X9PClient::Rremove(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.remove)(err);
		free(err);
		return;
	}

	Rremove_t* resp = (Rremove_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rremove_t))
		(*queued->callback.remove)("Rremove: Response too small");
	else
		(*queued->callback.remove)(0);
}
void X9PClient::Rstat(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.remove)(err);
		free(err);
		return;
	}

	Rstat_t* resp = (Rstat_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rstat_t))
		(*queued->callback.stat)("Rstat: Response too small", 0);
	else
		(*queued->callback.stat)(0, &resp->stat);
}
void X9PClient::Rwstat(message_t* respmsg, xqueuedmsg_t* queued)
{
	// Check if the response was an Rerror
	if (char* err = GetRerror(respmsg))
	{
		(*queued->callback.wstat)(err);
		free(err);
		return;
	}

	Rwstat_t* resp = (Rwstat_t*)respmsg;

	// Always perform size checks first
	if (resp->size < sizeof(Rwstat_t))
		(*queued->callback.wstat)("Rwstat: Response too small");
	else
		(*queued->callback.wstat)(0);
}

