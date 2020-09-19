#pragma once

#include "worker.h"
#include <stdint.h>

class m200i_frontend: public worker
{
	public:
		m200i_frontend(class midibase *);
		~m200i_frontend();

		const char *get_name();

		// worker
		virtual int get_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
		virtual void available_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds);

		// command from TCP sockets
		void send_dt1(int addr, int size, const uint8_t *data);
		void send_rq1(int addr, int size);
		void read_dt1(int addr, int size, uint8_t *data);

	private:
		class midibase *base;
		struct m200i_frontend_int *internal;
};
