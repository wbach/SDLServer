#include "GameServer.h"


int main(int argc, char* argv[])
{

	GameServer game_server;
	/*
	std::ofstream ofile("example_msg.xml");

	std::multimap<std::string, std::string> messeges = 
	{
		{"from", "p1"},
		{"push_card_from_hand", "ciri"}
	};
	ofile << Utils::MessageBuilder(messeges);
	ofile.close();

	return 0;*/
	CLogger::Instance().EnableLogs();

	game_server.Start();
	game_server.Update();
	
	return 0;
}