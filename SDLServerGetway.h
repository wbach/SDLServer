#pragma once
#include "../GameEngine/Utils/Types.h"
#include "User.h"
#include <SDL2/SDL_net.h>
#include <vector>
#include <map>

class SDLServerGetway
{
public:
	static SDLServerGetway& Instance();
	~SDLServerGetway();
	void Start(unsigned short port = 1234, unsigned short buffer_size = 512);
	void CheckNewConnections();
	bool WaitForRespone(uint& user_index, const std::string& response, uint time = 0);
	void GetMessage();
	std::string GetFirstMessageToUser(uint index);
	void SendMessage(const UserData& user, const std::string& message);
	void SendMessage(uint user, const std::string& message);
	void SendMessage(std::string& message);
	void DisconnectUser(UserData& user, uint i);
	int GetUserCount() { return m_ClientsCount; }
	std::multimap<int, std::string> m_IncomingMessages;
private:
	IPaddress serverIP;                  // The IP of the server (this will end up being 0.0.0.0 - which means roughly "any IP address")
	TCPsocket serverSocket;              // The server socket that clients will use to connect to us
	SDLNet_SocketSet socketSet;

	std::vector<UserData> users;

	uint m_MaxClients = 2;
	uint m_ClientsCount = 0;

	char* buffer = nullptr;
	unsigned short m_BufferSize = 0;

	SDLServerGetway() {}
};