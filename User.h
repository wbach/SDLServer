#pragma once
#include <SDL2/SDL_net.h>

struct UserData
{
	TCPsocket socket = nullptr;
	Uint32	  timeout = 0;
};