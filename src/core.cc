#include "m200i-midi-config.h"
#include "core.h"
#include "core-internal.h"
#include "worker.h"

#include <stdio.h>
#include <sys/select.h>

static struct core_internal *c;

struct core_internal *get_core()
{
	return c;
}

void core_add_worker(class worker *w)
{
	c->workers.push_back(w);
}

void core_del_worker(class worker *w)
{
	for (auto it=c->workers.begin(); it!=c->workers.end(); it++) if (*it == w) {
		c->workers.erase(it);
		break;
	}
}

void core_init()
{
	c = new core_internal;
}

int core_loop()
{
	int nfds;
	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;
	struct timeval timeout;

	while (1) {
		nfds = 0;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		for (auto it=c->workers.begin(); it!=c->workers.end(); it++) {
			class worker *w = *it;
			int r = w->get_fds(&readfds, &writefds, &exceptfds, &timeout) + 1;
			if (r > nfds)
				nfds = r;
		}

		select(nfds, &readfds, &writefds, &exceptfds, &timeout);

		for (auto it=c->workers.begin(); it!=c->workers.end(); it++) {
			class worker *w = *it;
			w->available_fds(&readfds, &writefds, &exceptfds);
		}
	}

	delete c;
	c = 0;

	return 0;
}
