#include <SDL2/SDL_net.h>
#include "SDLGetway.h"
#include "../../Gwint/GameLine.h"
#include "../../Gwint/Player.h"
#include "../Gwint/GameCards.h"
#include <map>
#include <sstream>

bool GameStarted = false;

struct GamePlayer
{
	Player player;
	std::string deck_xml;
	bool accepted_deck = false;
};

class GameServer
{
public:
	GameServer()
		: display("Gwint Server", 320, 240, false)
	{}
	void Start()
	{
		SDL_Init(SDL_INIT_EVERYTHING);		
		display.SetInput(m_InputManager.m_Input);
		getway.Start();

		GameCards::Instance().LoadCards();
		auto cards = GameCards::Instance().GetCards();

		player[0].player.cards_in_deck = cards[DeckType::NORTH];
		player[1].player.cards_in_deck = cards[DeckType::NORTH];

		//messeges just read xml file to string and sent
		player[0].deck_xml = Utils::ReadFile("../Data/Gwint/Cards/KrolestwaPolnocy.xml");
		player[1].deck_xml = Utils::ReadFile("../Data/Gwint/Cards/Cards.xml");
	}	

	void PushCard(unsigned int who, unsigned int i)
	{
		if (who == 0)
		{
			if (i > player[0].player.cards_in_hand.size())
				return;
			
			auto card = player[0].player.cards_in_hand[i];
			P1_Lines[card.type].AddCard(card);
			player[0].player.cards_in_hand.erase(player[0].player.cards_in_hand.begin() + i);

			//Send push messaeg to all players
			getway.SendMessage(0, std::string("OK"));
			//getway.SendMessage(1, MessageBuilder("OK"));
			//SendE
		}
		else if (who == 1)
		{

		}
	}
	void SwapCardStart(unsigned int who, unsigned int i)
	{
	}
	void ProccesMessage(const std::string& message)
	{		
		rapidxml::xml_document<> document;
		try
		{
			document.parse<0>(const_cast<char*>(message.c_str()));
		}
		catch (rapidxml::parse_error p)
		{
			p.what();
			return;
		}

		auto node = document.first_node();
		if (node == nullptr)
			return;

		unsigned int from = 0;
		for (auto snode = node->first_node(); snode; snode = snode->next_sibling())
		{
			std::string name = snode->name();
			std::string value = snode->value();
			if (name == "from")
				from = std::stoi(value);
			else if (name == "push_card_from_hand")
				PushCard(from, std::stoi(value));
			else if (name == "swap_card_start")
				SwapCardStart(from, std::stoi(value));
		}
		
	}

	void Update()
	{
		ApiMessages::Type m_ApiMessage = ApiMessages::NONE;
		while (m_ApiMessage != ApiMessages::QUIT)
		{
			NewGameProcedure();

			getway.CheckNewConnections();
			getway.GetMessage();

			m_ApiMessage = display.PeekMessage();

			if (m_InputManager.GetKey(KeyCodes::ESCAPE))
				m_ApiMessage = ApiMessages::QUIT;

			for(const auto& message : getway.m_IncomingMessages)
				ProccesMessage(message);

			display.Update();
		}
	}

	void NewGameProcedure()
	{
		if (GameStarted)
			return;

		while (getway.GetUserCount() >= 2 && !player[0].accepted_deck)
		{
			getway.SendMessage(0, player[0].deck_xml);
			std::cout << "Wait for client 1 accepting deck\n";
			if (getway.WaitForRespone("DECK_OK", 10000))
			{
				player[0].accepted_deck = true;
				std::cout << "client 1 accepted deck\n";
				break;
			}			
		}

		while (getway.GetUserCount() >= 2 && !player[1].accepted_deck)
		{
			getway.SendMessage(1, player[1].deck_xml);
			std::cout << "Wait for client 2 accepting deck\n";
			if (getway.WaitForRespone("DECK_OK", 10000))
			{
				player[1].accepted_deck = true;
				std::cout << "client 2 accepted deck\n";
				break;
			}
		}

		if (player[0].accepted_deck && player[1].accepted_deck)
		{
			getway.SendMessage(0, std::string("START_GAME"));
			getway.SendMessage(1, std::string("START_GAME"));
			GameStarted = true;
		}
			
		/*std::cout << "Wait for client 2 accepting deck\n";
		getway.SendMessage(1, p2_deck_xml);
		getway.WaitForRespone("DECK_OK");
		std::cout << "client 2 accepted deck\n";*/

		//std::cout << "Start Game...\n";
		
	}

	CInputManager m_InputManager;
	SDLGetway getway;
	CDisplayManager display;

	std::map<LineTypes, GameLine> P1_Lines;
	std::map<LineTypes, GameLine> P2_Lines;
	
	GamePlayer player[2];
};

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