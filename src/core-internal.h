#pragma once

#include <list>

struct core_internal
{
	std::list<class worker*> workers;
	std::list<class m200i_frontend*> frontends;
	std::list<class tcpcmd_listener*> tcpcmd_listeners;
};

void core_init();
int core_loop();
struct core_internal *get_core();
