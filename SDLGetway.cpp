#include "SDLGetway.h"

SDLGetway::~SDLGetway()
{
	if (buffer != nullptr)
		delete[] buffer;

	SDLNet_FreeSocketSet(socketSet);
	SDLNet_Quit();
	SDL_Quit();
}

void SDLGetway::Start(unsigned short port, unsigned short buffer_size)
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
		std::cout << str << std::endl;
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
		std::cout << str << std::endl;
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

	std::cout << "Connection is open." << std::endl;
}

void SDLGetway::CheckNewConnections()
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

			std::cout << "Client connected. There are now " << m_ClientsCount << " client(s) connected." << std::endl << std::endl;
		}
		else // If we don't have room for new clients...
		{
			std::cout << "*** Maximum client count reached - rejecting client connection ***" << std::endl;

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

bool SDLGetway::WaitForRespone(const std::string & response, unsigned int time)
{
	auto t1 = std::chrono::high_resolution_clock::now();
	while (1)
	{
		GetMessage();

		if(!m_IncomingMessages.empty())
			if (m_IncomingMessages.back() == response)
			{
				m_IncomingMessages.pop_back();
				return true;
			}

		auto t2 = std::chrono::high_resolution_clock::now();
		auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

		if (int_ms.count() > time && time > 0)
			break;
	}
	return false;
}

void SDLGetway::GetMessage()
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
			std::cout << "Client " << clientNumber << " disconnected." << std::endl << std::endl;
			DisconnectUser(user, clientNumber);
			std::cout << "Server is now connected to: " << m_ClientsCount << " client(s)." << std::endl << std::endl;
			continue;
		}

		m_IncomingMessages.emplace_back(buffer);
	}
}

void SDLGetway::SendMessage(const UserData & user, std::string & message)
{
	SDLNet_TCP_Send(user.socket, (void *)message.c_str(), message.size() + 1);
}

void SDLGetway::SendMessage(unsigned int user, std::string & message)
{
	if (user >= users.size())
		return;

	std::cout << "Sending message, size : " << message.size() << std::endl;
	SDLNet_TCP_Send(users[user].socket, (void *)message.c_str(), message.size() + 1);
}

void SDLGetway::SendMessage(std::string & message)
{
	for (const auto& user : users)
		SDLNet_TCP_Send(user.socket, (void *)message.c_str(), message.size() + 1);
}

void SDLGetway::DisconnectUser(UserData & user, unsigned int i)
{
	//...so output a suitable message and then...		
	SDLNet_TCP_DelSocket(socketSet, user.socket);
	SDLNet_TCP_Close(user.socket);
	users.erase(users.begin() + i);
	m_ClientsCount--;
}
