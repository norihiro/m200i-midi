#include "m200i-midi-config.h"
#include "core-internal.h"
#include "m200i-usb-backend.h"
#include "m200i-frontend.h"
#include "tcpcmd.h"

#include <stdio.h>
#include <stdlib.h>

static int new_m200i_usb(const char *dev)
{
	class midibase *base = new m200i_usb_backend(dev);
	if (base->get_fd()<0)
		return 1;

	m200i_frontend *f = new m200i_frontend(base);
	get_core()->frontends.push_back(f);

	return 0;
}

static int new_tcpcmd_listen(int port)
{
	class tcpcmd_listener *t = new tcpcmd_listener(port);
	get_core()->tcpcmd_listeners.push_back(t);
	return 0;
}

int main(int argc, char **argv)
{
	core_init();

	for (int i=1; i<argc; i++) {
		char *ai = argv[i];
		if (ai[0]=='-' && ai[1]=='-') {
			// TODO: long options
			if (1) {
				fprintf(stderr, "Error: unknown option %s\n", ai);
				return 1;
			}
		}
		else if (ai[0]=='-') while (char c=*++ai) switch(c) {
			case 'u':
				if (i+1>=argc) {
					fprintf(stderr, "Error: option -u requires an argument for the device name.\n");
					return 1;
				}
				if (int ret = new_m200i_usb(argv[++i])) {
					fprintf(stderr, "Error: failed to open USB device %s.\n", argv[i]);
					return ret;
				}
				break;
			case 'p':
				if (i+1>=argc) {
					fprintf(stderr, "Error: option -p requires an argument for the port number.\n");
					return 1;
				}
				if (int ret = new_tcpcmd_listen(atoi(argv[++i]))) {
					fprintf(stderr, "Error: failed to listen port %s.\n", argv[i]);
					return ret;
				}
				break;
			default:
				fprintf(stderr, "Error: unknown option %c\n", c);
				return 1;
		}
		else {
			fprintf(stderr, "Error: unknown option %s\n", ai);
			return 1;
		}
	}

	return core_loop();
}
