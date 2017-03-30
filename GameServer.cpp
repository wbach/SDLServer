#include "GameServer.h"
#include "../Gwint/Messages/Messages.h"
#include "SDLServerGetway.h"
#include "../Gwint/GameCards.h"

void GameServer::Start()
{
	SDL_Init(SDL_INIT_EVERYTHING);
	display.SetInput(m_InputManager.m_Input);
	SDLServerGetway::Instance().Start();

	GameCards::Instance().LoadCards();
	auto cards = GameCards::Instance().GetCards();

	player[0].player.cards_in_deck = cards[DeckType::NORTH];
	player[1].player.cards_in_deck = cards[DeckType::HEROS];

	//messeges just read xml file to string and sent
	player[0].deck_xml = Utils::ReadFile("../Data/Gwint/Cards/KrolestwaPolnocy.xml");
	player[1].deck_xml = Utils::ReadFile("../Data/Gwint/Cards/Cards.xml");
}

bool GameServer::WaitForPlayers()
{
	return SDLServerGetway::Instance().GetUserCount() >= 2;
}

void GameServer::PushCard(uint who, uint i)
{
	//if (who == 0)
	//{
	//	if (i > player[0].player.cards_in_hand.size())
	//		return;

	//	auto card = player[0].player.cards_in_hand[i];
	//	P1_Lines[card.type].AddCard(card);
	//	player[0].player.cards_in_hand.erase(player[0].player.cards_in_hand.begin() + i);

	//	//Send push messaeg to all players
	//	SDLServerGetway::Instance().SendMessage(0, std::string("OK"));
	//	//getway.SendMessage(1, MessageBuilder("OK"));
	//	//SendE
	//}
	//else if (who == 1)
	//{

	//}
}

void GameServer::Update()
{
	ApiMessages::Type m_ApiMessage = ApiMessages::NONE;
	while (m_ApiMessage != ApiMessages::QUIT)
	{
		switch (current_state)
		{
		case GameStates::WAIT_FOR_PLAYERS:
			if (WaitForPlayers())
				current_state = GameStates::SEND_DECKS;
			break;
		case GameStates::SEND_DECKS:
			if(NewGameProcedure())
				current_state = GameStates::SEND_CARDS_IN_HAND;
			break;
		case GameStates::SEND_CARDS_IN_HAND:
			if (SendCardsInHand())
				current_state = GameStates::SWAP_CARDS_START;
			break;
		case GameStates::SWAP_CARDS_START:
			if(SwapCardStarProcedure())
				current_state = GameStates::START_GAME;
			break;
		case GameStates::START_GAME:
			{
				uint player_start_number = 0;;
				if (StartGameProcedure(player_start_number))
				{
					if (player_start_number == 0)
					{
						current_state = GameStates::PLAYER_1_TURN;
					}
					else
					{
						current_state = GameStates::PLAYER_2_TURN;
					}
				}
			}
			break;
		}		

		SDLServerGetway::Instance().CheckNewConnections();
		SDLServerGetway::Instance().GetMessage();

		m_ApiMessage = display.PeekMessage();

		if (m_InputManager.GetKey(KeyCodes::ESCAPE))
			m_ApiMessage = ApiMessages::QUIT;

		display.Update();
	}
}

bool GameServer::NewGameProcedure()
{
	WaitForAkceptDeckPlayer(0);
	WaitForAkceptDeckPlayer(1);

	if (player[0].accepted_deck && player[1].accepted_deck)
	{
		SDLServerGetway::Instance().SendMessage(0, std::string("START_GAME"));
		SDLServerGetway::Instance().SendMessage(1, std::string("START_GAME"));
		return true;
	}

	return false;
}

bool GameServer::SendCardsInHand()
{
	player[0].player.TakeCardsStart();
	player[1].player.TakeCardsStart();

	SendCardsInHand(0);
	SendCardsInHand(1);

	uint player_index;
	if (SDLServerGetway::Instance().WaitForRespone(player_index, "HAND_CARDS_OK", 1000))
	{
		player[player_index].have_cards_in_hand = true;
	}

	if (player[0].have_cards_in_hand && player[1].have_cards_in_hand)
		return true;

	return false;
}

void GameServer::SendCardsInHand(uint player_index)
{
	uint i = 0;
	auto& p = player[player_index].player;
	SDLServerGetway::Instance().SendMessage(player_index, "START_SEND_CARDS_IN_HAND");
	for (const auto& card : p.cards_in_hand)
	{
		GwentMessages::SwapCardMessage msg;
		msg.type = GwentMessages::SwapCardMessage::MessageType::RESPONSE;
		msg.card_name = card.name;
		msg.index = p.indexes[i++];
		std::string msg_str = msg.ToString();
		SDLServerGetway::Instance().SendMessage(player_index, msg_str);
	}
	SDLServerGetway::Instance().SendMessage(player_index, "END_SEND_CARDS_IN_HAND");
}

void GameServer::WaitForAkceptDeckPlayer(uint player_index)
{
	if (player[player_index].accepted_deck)
		return;

	SDLServerGetway::Instance().SendMessage(player_index, player[player_index].deck_xml);
	std::cout << "Wait for client 1 accepting deck\n";
	uint user_index;
	if (SDLServerGetway::Instance().WaitForRespone(user_index, "DECK_OK", 1000))
	{
		if (user_index == player_index)
		{
			player[player_index].accepted_deck = true;
			std::cout << "client 1 accepted deck\n";
		}
	}	
}

void GameServer::SwapCardsPlayer(uint player_index)
{
	//Max 2 cards can be swaped
	if (player[player_index].swaped_cards_at_start >= 2)
		return;

	if (player[player_index].player.cards_in_deck.empty())
		return;

	if (player[player_index].player.cards_in_hand.empty())
		return;

	if (player[player_index].end_choosing_cards)
		return;

	SDLServerGetway::Instance().GetMessage();
	if (!SDLServerGetway::Instance().m_IncomingMessages.empty())
	{
		auto message = SDLServerGetway::Instance().GetFirstMessageToUser(player_index);
		GwentMessages::SwapCardMessage msg;
		if (msg.Serialized(message))
		{
			auto card = player[player_index].player.cards_in_hand[msg.index];
			if (msg.card_name == card.name)
			{
				auto i = rand() % player[player_index].player.cards_in_deck.size();
				auto swaped_card = player[player_index].player.cards_in_deck[i];
				player[player_index].player.cards_in_hand[msg.index] = swaped_card;
				player[player_index].player.cards_in_deck[i] = card;
				player[player_index].swaped_cards_at_start++;

				GwentMessages::SwapCardMessage response;
				response.card_name = swaped_card.name;
				response.index = i;
				response.type = GwentMessages::SwapCardMessage::MessageType::RESPONSE;

				SDLServerGetway::Instance().SendMessage(response.ToString());
			}
		}
	}	
}

bool GameServer::SwapCardStarProcedure()
{
	SwapCardsPlayer(0);
	SwapCardsPlayer(1);

	if (player[0].swaped_cards_at_start >= 2 && player[1].swaped_cards_at_start >= 2)
		return true;

	return false;
}

bool GameServer::StartGameProcedure(uint & player_start_number)
{
	return false;
}
