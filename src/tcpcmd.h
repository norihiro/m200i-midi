#pragma once

#include "worker.h"

class tcpcmd : public worker
{
	public:
		tcpcmd(int fd);
		virtual ~tcpcmd();
		virtual int get_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
		virtual void available_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds);

		bool is_closed();

	private:
		struct tcpcmd_data *data;
};

class tcpcmd_listener : public worker
{
	public:
		tcpcmd_listener (int port);
		virtual ~tcpcmd_listener ();
		virtual int get_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
		virtual void available_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds);
	private:
		struct tcpcmd_listener_data *data;
};
