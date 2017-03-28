#pragma once
#include "User.h"
#include <SDL2/SDL_net.h>
#include "../GameEngine/Display/DisplayManager.hpp"
#include "../GameEngine/Input/InputManager.h"
#include "../GameEngine/Debug_/Log.h"

class SDLGetway
{
public:
	~SDLGetway();
	void Start(unsigned short port = 1234, unsigned short buffer_size = 4096);
	void CheckNewConnections();
	bool WaitForRespone(const std::string& response, unsigned int time = 0);
	void GetMessage();
	void SendMessage(const UserData& user, std::string& message);
	void SendMessage(unsigned int user, std::string& message);
	void SendMessage(std::string& message);
	void DisconnectUser(UserData& user, unsigned int i);
	int GetUserCount() { return m_ClientsCount; }
	std::vector<std::string> m_IncomingMessages;
private:
	IPaddress serverIP;                  // The IP of the server (this will end up being 0.0.0.0 - which means roughly "any IP address")
	TCPsocket serverSocket;              // The server socket that clients will use to connect to us
	SDLNet_SocketSet socketSet;

	std::vector<UserData> users;

	unsigned int m_MaxClients = 2;
	unsigned int m_ClientsCount = 0;

	char* buffer = nullptr;
	unsigned short m_BufferSize = 0;	
};