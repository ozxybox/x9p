#include "X9PServer.h"
#include "XLogging.h"
#include <time.h>
#include <stdio.h>
#include <vector>
#include <unordered_map>


void X9PServer::Begin(const char* ip, const char* port, X9PFileSystem* fs) 
{
	ISOCKET_RESULT res = m_listener.Listen(ip, port);
	if (res != ISOCKET_RESULT::OK)
	{
		printf("Listen error: %d\n", res);
	}

	m_listener.SetNonblocking(true);

	m_maxmessagesize = X9P_MSIZE_DEFAULT;
	m_respbuffer = (char*)malloc(m_maxmessagesize);
	
	m_rootfilesystem = fs;


	s_lastRecvTime = 0;
}

void X9PServer::End()
{
	free(m_respbuffer);
}

int SendError(X9PClient& cl, xerr_t err, mtag_t tag)
{
	if (!err) return 0;

	// FIXME: size checking much?
	printf("\n---! Reporting error \"%s\"\n", err);

	Rerror_t* send = (Rerror_t*)cl.m_sendbuffer;
	send->tag = tag;
	send->type = X9P_RERROR;
	
	send->ename()->len = strlen(err);
	memcpy(send->ename()->str(), err, send->ename()->len);
	
	send->size = sizeof(Rerror_t) + send->ename()->size();

	cl.QueueResponse(send);

	return 1;
}



int GetXHND(X9PClient& cl, fid_t fid, mtag_t tag, xhnd& out)
{
	auto p = cl.m_fids.find(fid);
	if (p == cl.m_fids.end())
	{
		SendError(cl, XERR_FID_DNE, tag);
		return 0;
	}

	out = p->second;
	return 1;
}

void X9PServer::AcceptConnections()
{
	for(;;)
	{
		TCPClientSocket client;
		ISOCKET_RESULT r = m_listener.Accept(client);
		if (r == ISOCKET_RESULT::OK)
		{
			printf("new connection\n");
			client.SetNonblocking(true);
			m_clients.push_back(new X9PClient{ client });
		}
		else
			break;
	} 
}


// TODO: Size of an iounit should be msize - sizeof(Rread_t) or msize - sizeof(Twrite_t)

void X9PServer::ProcessPackets()
{
	for (int idx = 0; idx < m_clients.size(); idx++)
	{
		X9PClient& cl = *m_clients[idx];

		// Send off whatever we need to send
		cl.FlushResponses();

		
		// Proccess N requests from this client
		for (int i = 0; i < 5; i++)
		{

			// Anything new from this client?
			bool wouldblock = false;
			message_t* recvmsg = (message_t*)cl.m_recvbuffer;
			xerr_t r = cl.RecvRequest(wouldblock);

			mtag_t tag = recvmsg->tag;

			// Nothing new... Next client
			if (wouldblock)
				break;

			// Disconnected or errored?
			if (r)
			{
				cl.m_socket.Close();
				m_clients.erase(m_clients.begin() + idx);
				idx--;
				if(r) printf(r);
				printf("disconnected\n");
				break;
			}


			// TODO: Once we've got everything threaded and etc, add instrumentation
			//       On msg recv, record the time; on resp, record the time again
			s_lastRecvTime = clock() / (float)CLOCKS_PER_SEC;


			switch (recvmsg->type)
			{
			case X9P_TVERSION:
			{
				// TODO: Clunk all outstanding fids and begin a new session when Tversion is requested

				Tversion_t* recv = (Tversion_t*)recvmsg;

				// Negotiate a nice size
				uint32_t msize;
				if (recv->msize > m_maxmessagesize)
					msize = m_maxmessagesize;
				else
					msize = recv->msize;

				mtag_t tag = recv->tag;
				uint8_t type = recv->type;

				// TODO: maybe move this to when we get another response? The client could disconnect at this point
				// Bump up our buffer now that we know how much we need
				cl.m_maxmessagesize = msize;
				free(cl.m_sendbuffer);
				free(cl.m_recvbuffer);
				cl.m_sendbuffer = (char*)malloc(cl.m_maxmessagesize);
				cl.m_recvbuffer = (char*)malloc(cl.m_maxmessagesize);
				message_t* sendmsg = (message_t*)cl.m_sendbuffer;
				recvmsg = (message_t*)cl.m_recvbuffer;
			

				// At this point "recv" is invalid


				// Compose a response
				const int size = sizeof(Tversion_t) + 2 + X9P_VERSION_LEN;
				Rversion_t* send = (Rversion_t*)sendmsg;
				// Normally these first two are autofilled, but we've gotta fill em in since we deleted the buffer
				send->type = type + 1;
				send->tag = tag;
				send->size = size;
				send->msize = msize;

				// We only support 1 version of 9P for now
				xstr_t version = send->version();
				version->len = X9P_VERSION_LEN;
				memcpy(version->str(), X9P_VERSION, X9P_VERSION_LEN);


				XPRINTF("msize: %u\n", msize);
				cl.QueueResponse(send);

				break;
			}
			case X9P_TAUTH:
			{
				SendError(cl, "Tauth not supported", tag);
				break;
			}
			case X9P_TATTACH:
			{
				Tattach_t* recv = (Tattach_t*)recvmsg;

				// Create a new identity for this attach
				XAuth* auth = new XAuth;
				auth->client = &cl;
				auth->id = cl.m_authserial++;
				auth->uname = xstrdup(recv->uname());
				cl.m_connections.push_back(auth);

				// New Node Handle 
				xhnd hnd = new xhnd_data{ recv->fid, auth, m_rootfilesystem, XHID_UNINITIALIZED };
				cl.m_fids.emplace(recv->fid, hnd);


				hnd->fs->Tattach(hnd, 0, auth->uname, recv->aname(), [&](xerr_t err, qid_t* qid)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rattach_t* send = (Rattach_t*)cl.m_sendbuffer;
					send->type = X9P_RATTACH;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rattach_t);
					send->qid = *qid;
					cl.QueueResponse(send);
				});
				break;
			}
			case X9P_TFLUSH:
			{
				SendError(cl, "Tflush not supported", tag);
				break;
			}
			case X9P_TCLUNK:
			{
				Tclunk_t* recv = (Tclunk_t*)recvmsg;
				auto p = cl.m_fids.find(recv->fid);
				if (p == cl.m_fids.end())
				{
					SendError(cl, XERR_FID_DNE, recv->tag);
					break;
				}
				xhnd hnd = p->second;
				cl.m_fids.erase(p);
				hnd->fs->Tclunk(hnd, [&](xerr_t err)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rclunk_t* send = (Rclunk_t*)cl.m_sendbuffer;
					send->type = X9P_RCLUNK;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rclunk_t);
					cl.QueueResponse(send);
				});
				delete hnd;
				break;
			}
			case X9P_TWALK:
			{
				Twalk_t* recv = (Twalk_t*)recvmsg;
				if (recv->nwname > X9P_TWALK_MAXELEM)
				{
					SendError(cl, XERR_WALK_TOO_FAR, recv->tag);
					break;
				}

				// Old Node Handle
				xhnd hnd = 0;
				if (!GetXHND(cl, recv->fid, recv->tag, hnd)) break;

				// New Node Handle 
				xhnd newhnd = 0;
				if (recv->newfid == recv->fid)
				{
					// Use the same handle for the walk
					newhnd = hnd;
				}
				else
				{
					auto p = cl.m_fids.find(recv->newfid);
					if (p == cl.m_fids.end())
					{
						// FID not in use, create a new handle
						newhnd = new xhnd_data{ recv->newfid, hnd->auth, hnd->fs, XHID_UNINITIALIZED };
						cl.m_fids.emplace(recv->newfid, newhnd);
					}
					else
					{
						// Cannot use an already in use handle!
						SendError(cl, XERR_FID_USED, tag);
						break;
					}
				}

				// FIXME: Walk the entire wname before passing it to sanitize the packet

				hnd->fs->Twalk(hnd, newhnd, recv->nwname, recv->wname(), [&](xerr_t err, uint16_t nwqid, qid_t* wqid)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rwalk_t* send = (Rwalk_t*)cl.m_sendbuffer;
					send->type = X9P_RWALK;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rwalk_t) + sizeof(qid_t) * nwqid;
					send->nwqid = nwqid;
					memcpy(send->wqid(), wqid, nwqid * sizeof(qid_t));
					cl.QueueResponse(send);
				});
				break;
			}
			case X9P_TOPEN:
			{
				Topen_t* recv = (Topen_t*)recvmsg;
				xhnd hnd = 0;
				if (!GetXHND(cl, recv->fid, recv->tag, hnd)) break;

				// Sanitize
				openmode_t mode = recv->mode & X9P_OPEN_PROTOCOL_MASK;

				hnd->fs->Topen(hnd, mode, [&](xerr_t err, qid_t* qid, uint32_t iounit)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Ropen_t* send = (Ropen_t*)cl.m_sendbuffer;
					send->type = X9P_ROPEN;
					send->tag = recvmsg->tag;
					send->size = sizeof(Ropen_t);
					send->qid = *qid;
					send->iounit = iounit;
					cl.QueueResponse(send);
				});
				break;
			}
			case X9P_TCREATE:
			{
				Tcreate_t* recv = (Tcreate_t*)recvmsg;
				xhnd hnd = 0;
				if (!GetXHND(cl, recv->fid, recv->tag, hnd)) break;
			
				// Sanitize
				dirmode_t perm = *recv->perm() & X9P_DM_PROTOCOL_MASK;
				openmode_t mode = *recv->mode() & X9P_OPEN_PROTOCOL_MASK;
				if (xstrcmp(recv->name(), "..") == 0)
				{
					SendError(cl, XERR_FILENAME_DOTDOT, tag);
					break;
				}
				if (xstrcmp(recv->name(), ".") == 0)
				{
					SendError(cl, XERR_FILENAME_DOT, tag);
					break;
				}
				// TODO: Santize the name for invalid characters

			
				hnd->fs->Tcreate(hnd, recv->name(), perm, mode, [&](xerr_t err, qid_t* qid, uint32_t iounit)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rcreate_t* send = (Rcreate_t*)cl.m_sendbuffer;
					send->type = X9P_RCREATE;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rcreate_t);
					send->qid = *qid;
					send->iounit = iounit;
					cl.QueueResponse(send);
				});
				break;
			}
			case X9P_TREAD:
			{
				Tread_t* recv = (Tread_t*)recvmsg;
				xhnd hnd = 0;
				if (!GetXHND(cl, recv->fid, recv->tag, hnd)) break;

				hnd->fs->Tread(hnd, recv->offset, recv->count, [&](xerr_t err, uint32_t count, void* data)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rread_t* send = (Rread_t*)cl.m_sendbuffer;
					send->type = X9P_RREAD;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rread_t) + count;
					send->count = count;
					memcpy(send->data(), data, count);
					cl.QueueResponse(send);

				});
				break;
			}
			case X9P_TWRITE:
			{
				Twrite_t* recv = (Twrite_t*)recvmsg;
				xhnd hnd = 0;
				if (!GetXHND(cl, recv->fid, recv->tag, hnd)) break;
				hnd->fs->Twrite(hnd, recv->offset, recv->count, recv->data(), [&](xerr_t err, uint32_t count)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rwrite_t* send = (Rwrite_t*)cl.m_sendbuffer;
					send->type = X9P_RWRITE;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rwrite_t);
					send->count = count;
					cl.QueueResponse(send);
				});
				break;
			}
			case X9P_TREMOVE:
			{
				Tremove_t* recv = (Tremove_t*)recvmsg;
			
				auto p = cl.m_fids.find(recv->fid);
				if (p == cl.m_fids.end())
				{
					SendError(cl, XERR_FID_DNE, recv->tag);
					break;
				}
				xhnd hnd = p->second;
				cl.m_fids.erase(p);

				hnd->fs->Tremove(hnd, [&](xerr_t err)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rremove_t* send = (Rremove_t*)cl.m_sendbuffer;
					send->type = X9P_RREMOVE;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rremove_t);
					cl.QueueResponse(send);
				});

				delete hnd;
				break;
			}
			case X9P_TSTAT:
			{
				Tstat_t* recv = (Tstat_t*)recvmsg;
				xhnd hnd = 0;
				if (!GetXHND(cl, recv->fid, recv->tag, hnd)) break;
				hnd->fs->Tstat(hnd, [&](xerr_t err, stat_t* stat)
				{
					if (SendError(cl, err, tag)) return;

					uint64_t sz = sizeof(Rstat_t) + stat->size;
					if (sz > m_maxmessagesize)
					{
						SendError(cl, "Max message size was too small???", tag);
						printf("_________FIXME! TSTAT\n");
						return;
					}

					// Compose a response
					Rstat_t* send = (Rstat_t*)cl.m_sendbuffer;
					send->type = X9P_RSTAT;
					send->tag = recvmsg->tag;
					send->size = sz;
					send->n = 1;
					memcpy(&send->stat, stat, stat->size);
					cl.QueueResponse(send);
				});
				break;
			}
			case X9P_TWSTAT:
			{
				Twstat_t* recv = (Twstat_t*)recvmsg;
				xhnd hnd = 0;
				if (!GetXHND(cl, recv->fid, recv->tag, hnd)) break;

				// Sanitize
				recv->stat.mode &= X9P_DM_PROTOCOL_MASK;

				hnd->fs->Twstat(hnd, &recv->stat, [&](xerr_t err)
				{
					if (SendError(cl, err, tag)) return;

					// Compose a response
					Rwstat_t* send = (Rwstat_t*)cl.m_sendbuffer;
					send->type = X9P_RWSTAT;
					send->tag = recvmsg->tag;
					send->size = sizeof(Rwstat_t);
					cl.QueueResponse(send);
				});
				break;
			}
			default:
	#if XLOG_VERBOSE
				for (uint32_t k = 0; k < recvmsg->size; k++)
				{
					printf("%u|", (unsigned int)((unsigned char*)recvmsg)[k]);
				}
				printf("\n");
	#endif
				SendError(cl, "Invalid request", tag);
				break;
			}
		}
		
	}
}
