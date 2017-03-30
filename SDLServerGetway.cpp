#include "SDLServerGetway.h"
#include "../GameEngine/Display/DisplayManager.hpp"
#include "../GameEngine/Input/InputManager.h"
#include "../GameEngine/Debug_/Log.h"
#include <thread>

SDLServerGetway & SDLServerGetway::Instance()
{
	static SDLServerGetway getway;
	return getway;
}

SDLServerGetway::~SDLServerGetway()
{
	if (buffer != nullptr)
		delete[] buffer;

	SDLNet_FreeSocketSet(socketSet);
	SDLNet_Quit();
	SDL_Quit();
}

void SDLServerGetway::Start(unsigned short port, unsigned short buffer_size)
{
	buffer = new char[buffer_size];
	m_BufferSize = buffer_size;

	if (SDLNet_Init() == -1)
	{
		Error("Failed to intialise SDL_net: " + std::string(SDLNet_GetError()));
		exit(-1); // Quit!
	}

	socketSet = SDLNet_AllocSocketSet(m_MaxClients + 1);

	if (socketSet == NULL)
	{
		Error("Failed to allocate the socket set:: " + std::string(SDLNet_GetError()));
		exit(-1); // Quit!
	}
	else
	{
		std::string str = "Allocated socket set with size:  " + std::to_string(m_MaxClients + 1) + ", of which " + std::to_string(m_MaxClients) + " are availble for use by clients.";
		Log(str);
	}

	int hostResolved = SDLNet_ResolveHost(&serverIP, NULL, port);

	if (hostResolved == -1)
	{
		Error("Failed to resolve the server host: " + std::string(SDLNet_GetError()));
	}
	else // If we resolved the host successfully, output the details
	{
		// Get our IP address in proper dot-quad format by breaking up the 32-bit unsigned host address and splitting it into an array of four 8-bit unsigned numbers...
		Uint8 * dotQuad = (Uint8*)&serverIP.host;

		//... and then outputting them cast to integers. Then read the last 16 bits of the serverIP object to get the port number
		std::string str =
			"Successfully resolved server host to IP: " +
			std::to_string((unsigned short)dotQuad[0]) + "." +
			std::to_string((unsigned short)dotQuad[1]) + "." +
			std::to_string((unsigned short)dotQuad[2]) + "." +
			std::to_string((unsigned short)dotQuad[3]) +
			" port " + std::to_string(SDLNet_Read16(&serverIP.port));
		Log(str);
	}

	// Try to open the server socket
	serverSocket = SDLNet_TCP_Open(&serverIP);

	if (!serverSocket)
	{
		Error("Failed to open the server socket: " + std::string(SDLNet_GetError()));
		exit(-1);
	}
	else
	{
		Log("Sucessfully created server socket.");
	}

	// Add our server socket to the socket set
	SDLNet_TCP_AddSocket(socketSet, serverSocket);

	Log("Connection is open.");
}

void SDLServerGetway::CheckNewConnections()
{
	int numActiveSockets = SDLNet_CheckSockets(socketSet, 0);

	int serverSocketActivity = SDLNet_SocketReady(serverSocket);

	if (serverSocketActivity != 0)
	{
		// If we have room for more clients...
		if (m_ClientsCount < m_MaxClients)
		{
			// Find the first free socket in our array of client sockets

			UserData usr;
			usr.socket = SDLNet_TCP_Accept(serverSocket);			

			// ...add the new client socket to the socket set (i.e. the list of sockets we check for activity)
			SDLNet_TCP_AddSocket(socketSet, usr.socket);

			users.push_back(usr);
			// Increase our client count
			m_ClientsCount++;

			// Send a message to the client saying "OK" to indicate the incoming connection has been accepted
			//strcpy_s(buffer, 2, "OK");

			std::string ok = "OK";
			SDLNet_TCP_Send(usr.socket, (void *)ok.c_str(), ok.size() + 1);

			Log("Client connected. There are now " + std::to_string(m_ClientsCount) + " client(s) connected.");
		}
		else // If we don't have room for new clients...
		{
			Log("*** Maximum client count reached - rejecting client connection ***");

			// Accept the client connection to clear it from the incoming connections list
			TCPsocket tempSock = SDLNet_TCP_Accept(serverSocket);

			// Send a message to the client saying "FULL" to tell the client to go away
			std::string full = "FULL";
			SDLNet_TCP_Send(tempSock, (void *)full.c_str(), full.size() + 1);

			// Shutdown, disconnect, and close the socket to the client
			SDLNet_TCP_Close(tempSock);
		}

	} // End of if server socket is has activity check
}

bool SDLServerGetway::WaitForRespone(uint& user_index, const std::string & response, uint time)
{
	auto t1 = std::chrono::high_resolution_clock::now();
	while (1)
	{
		GetMessage();

		if (!m_IncomingMessages.empty())
		{
			for (auto it = m_IncomingMessages.begin(); it != m_IncomingMessages.end(); ++it)
			{
				if (it->second == response)
				{
					user_index = it->first;
					m_IncomingMessages.erase(it);
					return true;
				}
			}
		}			

		auto t2 = std::chrono::high_resolution_clock::now();
		auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

		if (int_ms.count() > time && time > 0)
			break;
	}
	return false;
}

void SDLServerGetway::GetMessage()
{
	int socketActive = SDLNet_CheckSockets(socketSet, 0);

	if (socketActive == 0)
		return;

	int clientNumber = -1;
	for (auto& user : users)
	{
		clientNumber++;

		int clientSocketActivity = SDLNet_SocketReady(user.socket);

		if (clientSocketActivity == 0)
			continue;

		int receivedByteCount = SDLNet_TCP_Recv(user.socket, buffer, m_BufferSize);

		if (receivedByteCount <= 0)
		{
			Log("Client " + std::to_string(clientNumber) + " disconnected.");
			DisconnectUser(user, clientNumber);
			Log("Server is now connected to: " + std::to_string(m_ClientsCount) + " client(s).");
			continue;
		}
		m_IncomingMessages.insert({ clientNumber, std::string(buffer) });
	}
}

std::string SDLServerGetway::GetFirstMessageToUser(uint index)
{
	for (auto it = m_IncomingMessages.begin(); it != m_IncomingMessages.end(); ++it)
	{
		if (it->first == index)
		{
			auto msg = it->second;
			m_IncomingMessages.erase(it);
			return msg;
		}
	}
	return std::string();
}

void SDLServerGetway::SendMessage(const UserData & user, const std::string & message)
{
	auto sended_bytes = SDLNet_TCP_Send(user.socket, (void *)message.c_str(), message.size() + 1);
	Log("Sended message, size : " + std::to_string(sended_bytes) + "/" + std::to_string(message.size() + 1) + " (" + std::to_string((int)(((float)sended_bytes/(float)(message.size() + 1))*100.f)) + "%)");
}

void SDLServerGetway::SendMessage(uint user, const std::string & message)
{
	if (user >= users.size())
		return;

	auto msize = message.size();
	uint max_frame_size = 511;
	if (msize > max_frame_size)
	{
		auto c_message = message;
		std::vector<std::string> parts;
		while (1)
		{
			auto delta = msize - max_frame_size;
			if (c_message.size() > max_frame_size)
			{
				std::string part = c_message.substr(0, max_frame_size);
				parts.push_back(part);
				c_message = c_message.substr(max_frame_size);
			}
			else
			{
				parts.push_back(c_message);
				break;
			}
		}
		//std::cout << "Sending message which size is over "+ std::to_string(max_frame_size) + ", size : " << message.size() << std::endl;
		std::string start_msg("START_SEND_PARTS");
		SendMessage(users[user], start_msg);
		for (const auto& part : parts)
		{
			//std::cout << "Sending part, size : " << part.size() << std::endl;
			SendMessage(users[user], part);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::string end_msg("END_SEND_PARTS");
		SendMessage(users[user], end_msg);
		//std::cout << "End sending message which size is over "+ std::to_string(max_frame_size) +", size. " << std::endl;
	}
	else
	{
		//std::cout << "Sending message, size : " << message.size() << std::endl;
		SendMessage(users[user], message);
	}	
}

void SDLServerGetway::SendMessage(std::string & message)
{
	for (const auto& user : users)
		SendMessage(user, message);
}

void SDLServerGetway::DisconnectUser(UserData & user, uint i)
{
	//...so output a suitable message and then...		
	SDLNet_TCP_DelSocket(socketSet, user.socket);
	SDLNet_TCP_Close(user.socket);
	users.erase(users.begin() + i);
	m_ClientsCount--;
}
