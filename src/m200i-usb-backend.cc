#include "m200i-midi-config.h"
#include "m200i-usb-backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static char *dev2name(const char *dev)
{
	const char *b, *e;
	for (b=dev, e=dev; *e; e++) {
		if (*e=='/')
			b = e+1;
	}
	char *name = new char[e-b+1];
	strcpy(name, b);
	return name;
}

m200i_usb_backend::m200i_usb_backend(const char *dev)
{
	my_name = dev2name(dev);
	fd = -1;

	fd = open(dev, O_RDWR);
}

int m200i_usb_backend::send(const void *data, int size)
{
	return write(fd, data, size);
}

int m200i_usb_backend::recv(void *data, int size)
{
	return read(fd, data, size);
}

int m200i_usb_backend::get_error() { return fd>=0 ? 0 : 1; }
int m200i_usb_backend::get_fd() { return fd; }
const char *m200i_usb_backend::get_name() { return my_name; }

m200i_usb_backend::~m200i_usb_backend()
{
	if (my_name) delete[] my_name;
}
