#pragma once
#include "midibase.h"

class m200i_usb_backend : public midibase
{
	public:
		m200i_usb_backend(const char *dev);
		virtual int send(const void *data, int size);
		virtual int recv(void *data, int size);
		virtual int get_fd();
		virtual int get_error();
		virtual const char * get_name();
		virtual ~m200i_usb_backend();

	private:
		char *my_name;
		int fd;
};
