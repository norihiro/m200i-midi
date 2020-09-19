#pragma once

class midibase
{
	public:
		virtual int send(const void *data, int size) = 0;
		virtual int recv(void *data, int size) = 0;
		virtual int get_fd() = 0;
		virtual int get_error() = 0;
		virtual const char * get_name() = 0;
		virtual ~midibase() = 0;
};
