#include "m200i-midi-config.h"
#include "tcpcmd.h"
#include "core.h"
#include "core-internal.h"
#include "m200i-frontend.h"

#include <unistd.h>
#include <vector>
#include <cstring>
#include <cstdio>


struct tcpcmd_data
{
	int fd;
	std::vector<char> buf_recv;
};

tcpcmd::tcpcmd(int fd)
{
	data = new tcpcmd_data;
	data->fd = fd;

	core_add_worker(this);
}

tcpcmd::~tcpcmd()
{
	close(data->fd);
	delete data;
}

int tcpcmd::get_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	if (data->fd>=0) {
		FD_SET(data->fd, read_fds);
		return data->fd;
	}
	return 0;
}

int arg2addr(const char *a)
{
	int r = 0;
	int b = 4;
	while (char c=*a++) {
		if ('0'<=c && c<='9') {
			r = (r << b) | (c-'0');
			b = 4;
		}
		else if ('a'<=c && c<='f') {
			r = (r << b) | (c-'a'+10);
			b = 4;
		}
		else if ('A'<=c && c<='F') {
			r = (r << b) | (c-'A'+10);
			b = 4;
		}
		else if(c=='_') {
			b = 3;
		}
	}
	return r;
}

void arg2vec(const char *a, std::vector<uint8_t> &v)
{
	int r = 0;
	while (char c=*a++) {
		if ('0'<=c && c<='9')
			r = (r << 4) | (c-'0');
		else if ('a'<=c && c<='f')
			r = (r << 4) | (c-'a'+10);
		else if ('A'<=c && c<='F')
			r = (r << 4) | (c-'A'+10);
		else if(c=='_') {
			v.push_back(r & 0x7F);
			r = 0;
		}
	}
	v.push_back(r & 0x7F);
}

static void handle_line(tcpcmd_data *data, char *line)
{
	std::vector<char*> arg;
	bool found = 1;
	for (char *ptr=line; *ptr; ptr++) {
		if (*ptr==' ' || *ptr=='\t') {
			found = 1;
			*ptr = 0;
		}
		else if (found) {
			arg.push_back(ptr);
			found = 0;
		}
	}
	for (int i=0; i<(int)arg.size(); i++) fprintf(stderr, "Debug: arg[%d]=\"%s\"\n", i, arg[i]);
	if (arg.size()<1)
		return;

	char buf[1024] = {0};

	if (!strcmp(arg[0], "list")) {
		auto &ff = get_core()->frontends;
		char *ptr = buf;
		for (auto it=ff.begin(); it!=ff.end(); it++) {
			class m200i_frontend *f = *it;
			if (ptr!=buf) *ptr++ = ' ';
			strcpy(ptr, f->get_name());
			while (*ptr) ptr++;
		}
		*ptr++ = '\r';
		*ptr++ = '\n';

		write(data->fd, buf, ptr-buf);
	}
	else if(!strcmp(arg[0], "dt") && arg.size()>1) {
		int addr = arg2addr(arg[1]);
		std::vector<uint8_t> dt;
		for (size_t i=2; i<arg.size(); i++)
			arg2vec(arg[i], dt);

		auto &ff = get_core()->frontends;
		for (auto it=ff.begin(); it!=ff.end(); it++) {
			class m200i_frontend *f = *it;
			f->send_dt1(addr, dt.size(), &dt[0]);
			break;
		}
	}
	else if(!strcmp(arg[0], "rq") && arg.size()==3) {
		int addr = arg2addr(arg[1]);
		int size = arg2addr(arg[2]);

		auto &ff = get_core()->frontends;
		for (auto it=ff.begin(); it!=ff.end(); it++) {
			class m200i_frontend *f = *it;
			f->send_rq1(addr, size);
			break;
		}
	}
	else if(!strcmp(arg[0], "rb") && arg.size()==3) {
		int addr = arg2addr(arg[1]);
		int size = arg2addr(arg[2]);
		std::vector<uint8_t> dt(size);

		auto &ff = get_core()->frontends;
		for (auto it=ff.begin(); it!=ff.end(); it++) {
			class m200i_frontend *f = *it;
			f->read_dt1(addr, size, &dt[0]);
			break;
		}

		char *ptr = buf;
		for (size_t i=0; i<dt.size(); i++) {
			sprintf(ptr, " %02X", dt[i]&0x7F);
			ptr+=3;
		}
		*ptr++ = '\r';
		*ptr++ = '\n';

		write(data->fd, buf+1, ptr-buf-1);
	}
}

static void handle_recv_data(tcpcmd_data *data)
{
	auto &buf_recv = data->buf_recv;
	char *begin = &buf_recv[0];
	char *end = begin + buf_recv.size();
	char *next = begin;

	for (char *ptr=begin; ptr!=end; ptr++) {
		if (*ptr=='\r' || *ptr=='\n') {
			*ptr = 0;
			if (*next)
				handle_line(data, next);
			next = ptr+1;
		}
	}

	buf_recv.erase(buf_recv.begin(), buf_recv.begin()+(next-begin));
}

void tcpcmd::available_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds)
{
	const int fd = data->fd;
	if (fd>=0 && FD_ISSET(fd, read_fds)) {
		char buf[1024];
		int ret = read(fd, buf, sizeof(buf));
		data->buf_recv.insert(data->buf_recv.end(), buf, buf+ret);
		handle_recv_data(data);
	}
}

bool tcpcmd::is_closed()
{
	return data->fd < 0;
}
