#pragma once

#include "X9PProtocol.h"
#include "X9PFileSystem.h"
#include "XAuth.h"
#include "socket/TCPSocket.h"

#include <unordered_map>
#include <functional>


// I refuse to spend another 6 hours writing template code or a thousand reinterpret_casts
union xmsgcallback_t
{
	void        *ptr;
	Rversion_fn *version;
	Rauth_fn    *auth;
  //Rerror_fn   *error;
	Rflush_fn   *flush;
	Rattach_fn  *attach;
	Rwalk_fn    *walk;
	Ropen_fn    *open;
	Rcreate_fn  *create;
	Rread_fn    *read;
	Rwrite_fn   *write;
	Rclunk_fn   *clunk;
	Rremove_fn  *remove;
	Rstat_fn    *stat;
	Rwstat_fn   *wstat;
};

class X9PUser
{
public:
	fid_t m_rootdirectory;
	xstr_t m_username;
	xstr_t m_accessedtree;
};

// This'll make threading easier later
struct xmsg_t
{
	message_t* msg;
	uint32_t   size;
};



struct xreqmsg_t
{
	uint8_t  type;
	mtag_t   tag;

	xmsgcallback_t callback;
};

struct xqueuedmsg_t
{
	message_t* message;
	xmsgcallback_t callback;
};


struct xfiddata_t
{
	xfiddata_t();
	~xfiddata_t();

	int refcount;
	std::vector<Twrite_fn*> hooks;

};


class X9PClient : public X9PFileSystem
{
public:
	X9PClient();
	X9PClient(TCPClientSocket sock);
	~X9PClient();

	xerr_t Begin(const char* ip, const char* port);
	void End();

	xerr_t ProcessPackets();


	void Tversion(uint32_t messagesize, xstr_t version, Rversion_fn callback);
	// Tauth
	// Tflush
	virtual void Tattach (xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback);
	virtual void Twalk   (xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback);
	virtual void Topen   (xhnd hnd, openmode_t mode, Ropen_fn callback);
	virtual void Tcreate (xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback);
	virtual void Tread   (xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback);
	virtual void Twrite  (xhnd hnd, uint64_t offset, uint32_t count, void* data, Rwrite_fn callback);
	virtual void Tclunk  (xhnd hnd, Rclunk_fn callback);
	virtual void Tremove (xhnd hnd, Rremove_fn callback);
	virtual void Tstat   (xhnd hnd, Rstat_fn callback);
	virtual void Twstat  (xhnd hnd, stat_t* stat, Rwstat_fn callback);

private:
	void Rversion(message_t* respmsg, xreqmsg_t* queued);
  //void Rauth   (message_t* respmsg, xsentmsg_t* queued);
  //void Rerror  (message_t* respmsg, xsentmsg_t* queued);
  //void Rflush  (message_t* respmsg, xsentmsg_t* queued);
	void Rattach (message_t* respmsg, xreqmsg_t* queued);
	void Rwalk   (message_t* respmsg, xreqmsg_t* queued);
	void Ropen   (message_t* respmsg, xreqmsg_t* queued);
	void Rcreate (message_t* respmsg, xreqmsg_t* queued);
	void Rread   (message_t* respmsg, xreqmsg_t* queued);
	void Rwrite  (message_t* respmsg, xreqmsg_t* queued);
	void Rclunk  (message_t* respmsg, xreqmsg_t* queued);
	void Rremove (message_t* respmsg, xreqmsg_t* queued);
	void Rstat   (message_t* respmsg, xreqmsg_t* queued);
	void Rwstat  (message_t* respmsg, xreqmsg_t* queued);
public:

	// Client
	void FlushRequests();
	void QueueRequest(message_t* msg, xmsgcallback_t callback = {nullptr});
	xerr_t RecvResponse(bool& wouldblock);
	
	// Server
	void FlushResponses();
	void QueueResponse(message_t* msg);
	xerr_t RecvRequest(bool& wouldblock);

	xerr_t SendMessage(message_t* m);

	mtag_t NewTag();
	fid_t NewFID();


	std::unordered_map<fid_t, xhnd> m_fids;

	std::vector<xqueuedmsg_t> m_queue;
	std::unordered_map<mtag_t, xreqmsg_t> m_requestmessages;

	std::unordered_map<fid_t, xfiddata_t*> m_fiddata;


	TCPClientSocket m_socket;
	uint16_t m_currentTag;

	size_t m_maxmessagesize;

	char* m_sendbuffer;
	char* m_recvbuffer;
	size_t m_partialrecvoffset;
	size_t m_partialrecvsize;


	std::vector<XAuth*> m_connections;
	int m_authserial;
};



// Need something where we can queue up N messages into a big buffer that can be set at once
// if it fills up we need to alloc space and tack it onto another buffer
