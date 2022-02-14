#pragma once
#include "X9P.h"
#include "X9PFileSystem.h"
#include "socket/TCPSocket.h"
#include "fs.h"

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



struct xqueuedmsg_t
{
	uint8_t  type;
	mtag_t   tag;

	xmsgcallback_t callback;
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


	virtual xhnd NewFileHandle(int connection);

	void Tversion(uint32_t messagesize, xstr_t version, Rversion_fn callback);
	// Tauth
	// Tflush
	virtual void Tattach (xhnd hnd, xhnd ahnd, xstr_t username, xstr_t accesstree, Rattach_fn callback);
	virtual void Twalk   (xhnd hnd, xhnd newhnd, uint16_t nwname, xstr_t wname, Rwalk_fn callback);
	virtual void Topen   (xhnd hnd, openmode_t mode, Ropen_fn callback);
	virtual void Tcreate (xhnd hnd, xstr_t name, uint32_t perm, openmode_t mode, Rcreate_fn callback);
	virtual void Tread   (xhnd hnd, uint64_t offset, uint32_t count, Rread_fn callback);
	virtual void Twrite  (xhnd hnd, uint64_t offset, uint32_t count, uint8_t* data, Rwrite_fn callback);
	virtual void Tclunk  (xhnd hnd, Rclunk_fn callback);
	virtual void Tremove (xhnd hnd, Rremove_fn callback);
	virtual void Tstat   (xhnd hnd, Rstat_fn callback);
	virtual void Twstat  (xhnd hnd, stat_t* stat, Rwstat_fn callback);

private:
	void Rversion(message_t* respmsg, xqueuedmsg_t* queued);
  //void Rauth   (message_t* respmsg, xqueuedmsg_t* queued);
  //void Rerror  (message_t* respmsg, xqueuedmsg_t* queued);
  //void Rflush  (message_t* respmsg, xqueuedmsg_t* queued);
	void Rattach (message_t* respmsg, xqueuedmsg_t* queued);
	void Rwalk   (message_t* respmsg, xqueuedmsg_t* queued);
	void Ropen   (message_t* respmsg, xqueuedmsg_t* queued);
	void Rcreate (message_t* respmsg, xqueuedmsg_t* queued);
	void Rread   (message_t* respmsg, xqueuedmsg_t* queued);
	void Rwrite  (message_t* respmsg, xqueuedmsg_t* queued);
	void Rclunk  (message_t* respmsg, xqueuedmsg_t* queued);
	void Rremove (message_t* respmsg, xqueuedmsg_t* queued);
	void Rstat   (message_t* respmsg, xqueuedmsg_t* queued);
	void Rwstat  (message_t* respmsg, xqueuedmsg_t* queued);
public:


	//void Clunk(fid_t fid);
	
	void QueueMessage(message_t* msg, xmsgcallback_t callback);
	
	xerr_t RecvMessage(bool& wouldblock);

	xerr_t SendMessage(message_t* msg);

	fid_t NewFID();

	mtag_t NewTag();
	
	std::unordered_map<fid_t, xhnd> m_fids;
	std::unordered_map<mtag_t, xqueuedmsg_t> m_messages;

	TCPClientSocket m_socket;
	uint16_t m_currentTag;

	size_t m_maxmessagesize;

	char* m_sendbuffer;
	char* m_recvbuffer;
};

/*

should be able to fire off like 6 reads at once or something

req = 
newrequest(callback)
.walk("files/documents")
.open("rad.txt", READ)
.read(buf, 0, 512)
.remove();

req.send();

Send off all requests at once 

should this be part of the filesystem?



Main Thread ------ FS Thread ----- 9P Thread


xopen -> vop_open -> Topen --,
file* <-      qid <- Ropen <-'



.open.read.remove -> vop_open vop_read vop_remove -> Topen Tread Tremove

func stack  -> vop stack  -> 9p Tmsgs --,
func result <- vop result <- 9p Rmsgs <-'


im designing an RPC api for a nonexistant system!

locking
0: unlocked
any: locked
lock using thread id
compare thread id against locked state
wait until 0

*/

