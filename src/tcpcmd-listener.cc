#include "m200i-midi-config.h"
#include "tcpcmd.h"
#include "core.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <list>

#include <netinet/in.h>

struct tcpcmd_listener_data
{
	int port;
	int fd;
	std::list <tcpcmd*> tt;
};

static int init_listener(int port)
{
	int soc = socket(AF_INET, SOCK_STREAM, 0);

	int opt = 1;
	setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	struct sockaddr_in me = {0};
	me.sin_family = AF_INET;
	me.sin_port = htons(port);
	if(bind(soc, (const sockaddr*)&me, sizeof(me))!=0) {
		close(soc);
		return -1;
	}

	listen(soc, 64);

	return soc;
}

tcpcmd_listener::tcpcmd_listener(int port)
{
	data = new tcpcmd_listener_data;
	data->port = port;
	data->fd = init_listener(port);

	core_add_worker(this);
}

tcpcmd_listener::~tcpcmd_listener()
{
	if (data->fd>=0)
		close(data->fd);
}

int tcpcmd_listener::get_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	if (data->fd>=0) {
		FD_SET(data->fd, read_fds);
		return data->fd;
	}
	return 0;
}

void tcpcmd_listener::available_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds)
{
	if (data->fd>=0 && FD_ISSET(data->fd, read_fds)) {
		struct sockaddr_in server;
		socklen_t len_server = sizeof(server);
		int sx = accept(data->fd, (sockaddr*)&server, &len_server);
		if (sx<0) {
			perror("accept");
			return;
		}

		tcpcmd *t = new tcpcmd(sx);
		data->tt.push_back(t);
	}

	for (auto it=data->tt.begin(); it!=data->tt.end(); it++) {
		tcpcmd *t = *it;
		if (t->is_closed()) {
			data->tt.erase(it);
			delete t;
			break;
		}
	}
}
