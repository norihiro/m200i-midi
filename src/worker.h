#pragma once

#include <sys/select.h>

class worker
{
	public:
		virtual int get_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) = 0; // called before select
		virtual void available_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds) = 0; // called after select
};
