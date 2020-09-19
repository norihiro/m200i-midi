#include "m200i-midi-config.h"
#include "m200i-frontend.h"
#include "midibase.h"
#include "core.h"

#include <deque>
#include <cstdio>
#include <cstring>

#define add2int(a0, a1, a2, a3) ( \
		(((a0)&0x7F) << 21) | \
		(((a1)&0x7F) << 14) | \
		(((a2)&0x7F) << 7) | \
		( (a3)&0x7F      ) )

#define addr_hep0(a) (((a)>>21) & 0x7F)
#define addr_hep1(a) (((a)>>14) & 0x7F)
#define addr_hep2(a) (((a)>> 7) & 0x7F)
#define addr_hep3(a) ( (a)      & 0x7F)

typedef enum
{
	s_init = 0,
	s_wait_identity,
	s_handshaked,
	s_error
} m200i_state_t;

struct m200i_frontend_int
{
	uint8_t cache[128*128*128*17];
	m200i_state_t state;

	// exclusive message
	uint8_t ex_msg;
	uint8_t ex_data[256]; // only first data
	int ex_data_size;

	// identity
	uint8_t id_dev, id_rev0, id_rev1, id_rev2, id_rev3;
};

m200i_frontend::m200i_frontend(class midibase *_base)
{
	base = _base;

	internal = new m200i_frontend_int;
	internal->ex_msg = 0;

	// 7-bit x 4, 00_00_00_00 - 10_7F_7F_7F (34 MB)

	core_add_worker(this);
}

m200i_frontend::~m200i_frontend()
{
	delete internal;
}

int m200i_frontend::get_fds(fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	m200i_frontend_int &m = *internal;
	const int fd = base->get_fd();
	if (m.state == s_init) {
		FD_SET(fd, writefds);
	}
	FD_SET(fd, readfds);
	return fd;
}

inline
static bool is_readonly(int addr)
{
	const uint8_t a0 = addr_hep0(addr);
	const uint8_t a1 = addr_hep1(addr);
	const uint8_t a2 = addr_hep2(addr);
	const uint8_t a3 = addr_hep3(addr);
	switch(a0) {
		case 0x00: // input board parameters
			if (0x00<=a1 && a1<=0x27) switch(a2<<8 | a3) {
				case 0x0000: return 1;
				case 0x0004: return 1;
			}
			else if (0x50<=a1 && a1<=0x67) switch(a2<<8 | a3) {
				case 0x0000: return 1;
			}
			return 0;
		case 0x01: // output board parameters
			if (0x00<=a1 && a1<=0x27) switch(a2<<8 | a3) {
				case 0x0000: return 1;
			}
			return 0;
			// 0x02 - 0x0F has no read-only parameters.
		case 0x10: // system
			switch(a1<<16 | a2<<8 | a3) {
				case 0x0101: return 1;
				case 0x0112: return 1;
				case 0x011A: return 1;
				case 0x0122: return 1;
				case 0x012A: return 1;
			}
			return 0;
			// TODO: implement remaining parameters
	}
	return 0;
}

static void cache_dt(m200i_frontend_int &m, int addr, int size, const uint8_t *data)
{
	while (size-- > 0) {
	if (0<=addr && addr<(int)sizeof(m.cache) && !is_readonly(addr)) {
		m.cache[addr] = *data;
	}
	addr++; data++;
}

static void send_dt1(class midibase *base, m200i_frontend_int &m, int addr, int size, const uint8_t *data)
{

	const int len = size+13;
	uint8_t dt1[len];
	uint8_t dt1_head[] = {
		0xF0,
		0x41,
		m.id_dev,
		0x00, 0x00, 0x24, // model M-200i
		0x12, // RQ1
		(uint8_t)((addr >> 21) & 0x7F),
		(uint8_t)((addr >> 14) & 0x7F),
		(uint8_t)((addr >>  7) & 0x7F),
		(uint8_t)((addr      ) & 0x7F),
	};
	memcpy(dt1, dt1_head, sizeof(dt1_head));
	for (int i=0; i<size; i++)
		dt1[i+11] = data[i];
	uint8_t sum = 0;
	for (int i=7; i<len-2; i++)
		sum -= dt1[i];
	dt1[size+11] = sum;
	dt1[size+12] = 0xF7;

	fprintf(stderr, "Debug: send_dt1: name=%s addr=0x%X size=0x%X\n", base->get_name(), addr, size);
	cache_dt(m, addr, size, data);
	base->send(dt1, len);
}

static void send_rq1(class midibase *base, m200i_frontend_int &m, int addr, int size)
{
	uint8_t rq1[] = {
		0xF0,
		0x41,
		m.id_dev,
		0x00, 0x00, 0x24, // model M-200i
		0x11, // RQ1
		(uint8_t)((addr >> 21) & 0x7F),
		(uint8_t)((addr >> 14) & 0x7F),
		(uint8_t)((addr >>  7) & 0x7F),
		(uint8_t)((addr      ) & 0x7F),
		(uint8_t)((size >> 21) & 0x7F),
		(uint8_t)((size >> 14) & 0x7F),
		(uint8_t)((size >>  7) & 0x7F),
		(uint8_t)((size      ) & 0x7F),
		0x00, // sum
		0xF7
	};
	const int len = sizeof(rq1);
	uint8_t sum = 0;
	for (int i=7; i<len-2; i++)
		sum -= rq1[i];
	rq1[len-2] = sum & 0x7F;
	fprintf(stderr, "Debug: send_rq1: name=%s addr=0x%X size=0x%X\n", base->get_name(), addr, size);
	base->send(rq1, len);
}

static void handle_eox(m200i_frontend_int &m)
{
	uint8_t *ex = m.ex_data;
	fprintf(stderr, "Debug: ex_msg=0x%X ex_data_size=%d\n", m.ex_msg, m.ex_data_size);
	if (m.ex_data_size==13 && ex[0]==0x7E && ex[2]==0x06 && ex[3]==0x02) {
		// Identity Reply
		m.id_dev = ex[1];
		if (m.state==s_wait_identity && ex[4]==0x41 && ex[5]==0x24 && ex[6]==0x02 && ex[7]==0x00 && ex[8]==0x03) {
			fprintf(stderr, "Debug: s_wait_identity->s_handshaked: id_dev=0x%X\n", m.id_dev);
			m.state = s_handshaked;
		}
		m.id_rev0 = ex[9];
		m.id_rev1 = ex[10];
		m.id_rev2 = ex[11];
		m.id_rev3 = ex[12];
	}
	else if(m.state==s_handshaked && m.ex_data_size>11 &&
			ex[0]==0x41 && ex[1]==m.id_dev && 
			ex[2]==0x00 && ex[3]==0x00 && ex[4]==0x24 &&
			ex[5]==0x12 ) {
		// DT1
		uint8_t sum = 0;
		for (int i=6; i<m.ex_data_size && i<(int)sizeof(m.ex_data); i++)
			sum += ex[i];
		if (sum & 0x7F) {
			fprintf(stderr, "Info: checksum failed\n");
			return;
		}

		int ad = add2int(ex[6], ex[7], ex[8], ex[9]);
		for (int i=10; i+1<m.ex_data_size && i<(int)sizeof(m.ex_data); i++, ad++) {
			if (ad < (int)sizeof(m.cache))
				m.cache[ad] = ex[i];
		}
	}
}

static void handle_msg(m200i_frontend_int &m, uint8_t *data, int size)
{
	while(size>0) {
		uint8_t d = *data++; size--;

		if (d & 0x80) {
			if (m.ex_msg==0xF0 && d==0xF7) handle_eox(m);
			m.ex_msg = d;
			m.ex_data_size = 0;
		}
		else if (m.ex_msg == 0xF0) {
			// body of system exclusive message
			if (m.ex_data_size < (int)sizeof(m.ex_data))
				m.ex_data[m.ex_data_size] = d;
			m.ex_data_size ++;
		}
	}
}

void m200i_frontend::available_fds(fd_set *read_fds, fd_set *writefds, fd_set *exceptfds)
{
	m200i_frontend_int &m = *internal;
	const int fd = base->get_fd();
	if (FD_ISSET(fd, read_fds)) {
		uint8_t data[512];
		int ret = base->recv(data, sizeof(data));
		if (ret>0)
			handle_msg(m, data, ret);
	}

	if (m.state==s_init && FD_ISSET(fd, writefds)) {
		const uint8_t idreq[] = {
			0xF0,
			0x7E,
			0x7F, // dev
			0x06,
			0x01,
			0xF7
		};
		fprintf(stderr, "Debug: s_init->s_wait_identity: name=%s sending idreq\n", base->get_name());
		base->send(idreq, sizeof(idreq));
		m.state = s_wait_identity;
	}
}

void m200i_frontend::send_dt1(int addr, int size, const uint8_t *data)
{
	::send_dt1(base, *internal, addr, size, data);
}

void m200i_frontend::send_rq1(int addr, int size)
{
	::send_rq1(base, *internal, addr, size);
}

void m200i_frontend::read_dt1(int addr, int size, uint8_t *data)
{
	m200i_frontend_int &m = *internal;
	while (size>0) {
		if (0<=addr && addr<(int)sizeof(m.cache))
			*data = m.cache[addr];
		addr++; size--; data++;
	}
}

const char *m200i_frontend::get_name()
{
	return base->get_name();
}
