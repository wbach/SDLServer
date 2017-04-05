#include "GameServer.h"
#include "Network/SDLServerGetway.h"
#include <Gwint/Cards/GameCards.h>
#include <thread>

GameServer::GameServer()
	: display("Gwint Server", 320, 240, false)
{
	GameLines[0] =
	{
		{ LineTypes::CLOSE_COMBAT, GameLine(glm::vec3(-.225f, 0.015, .63f)) },
		{ LineTypes::ARCHEER, GameLine(glm::vec3(-.225f, -0.225, .63f)) },
		{ LineTypes::BALIST, GameLine(glm::vec3(-.225f, -0.48, .63f)) }
	};
	GameLines[1] =
	{
		{ LineTypes::CLOSE_COMBAT, GameLine(glm::vec3(-.225f, 0.015, .63f)) },
		{ LineTypes::ARCHEER, GameLine(glm::vec3(-.225f, -0.225, .63f)) },
		{ LineTypes::BALIST, GameLine(glm::vec3(-.225f, -0.48, .63f)) }
	};
}

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
	if (SDLServerGetway::Instance().GetUserCount() >= 2)
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
		return true;
	}
	return false;
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
		if(SDLServerGetway::Instance().GetUserCount() <= 0)
			current_state = GameStates::WAIT_FOR_PLAYERS;

		switch (current_state)
		{
		case GameStates::WAIT_FOR_PLAYERS:
			if (WaitForPlayers())
				ChangeState(GameStates::SEND_DECKS);
			break;
		case GameStates::SEND_DECKS:
			if (NewGameProcedure())
				ChangeState(GameStates::SEND_CARDS_IN_HAND);
			break;
		case GameStates::SEND_CARDS_IN_HAND:
			if (SendCardsInHand())
				ChangeState(GameStates::SWAP_CARDS_START);
			break;
		case GameStates::SWAP_CARDS_START:
			if(SwapCardStarProcedure())
				ChangeState(GameStates::START_GAME);
			break;
		case GameStates::START_GAME:
			{
				if (StartGameProcedure())
				{
					if (player_start_game == 0)
					{
						ChangeState(GameStates::PLAYER_1_MOVE);
					}
					else if (player_start_game == 1)
					{
						ChangeState(GameStates::PLAYER_2_MOVE);
					}
					Log("START GAME");
				}
			}
			break;
		case GameStates::PLAYER_1_MOVE:
			if (Player1Move())
				ChangeState(GameStates::PLAYER_2_MOVE);
			break;
		case GameStates::PLAYER_2_MOVE:
			if (Player2Move())
				ChangeState(GameStates::PLAYER_1_MOVE);
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


	auto t1 = std::chrono::high_resolution_clock::now();

	while (1)
	{
		uint player_index;
		if (SDLServerGetway::Instance().WaitForRespone(player_index, "HAND_CARDS_OK", 10))
		{
			player[player_index].have_cards_in_hand = true;
		}

		if (player[0].have_cards_in_hand && player[1].have_cards_in_hand)
			return true;
		
		auto t2 = std::chrono::high_resolution_clock::now();
		auto durration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1);
		if (durration.count() > 2000)
		{
			t1 = std::chrono::high_resolution_clock::now();

			if(!player[0].have_cards_in_hand)
				SendCardsInHand(0);
			if (!player[1].have_cards_in_hand)
				SendCardsInHand(1);
		}

	}
	return false;
}

void GameServer::SendCardsInHand(uint player_index)
{
	if (player[player_index].have_cards_in_hand)
		return;

	uint i = 0;
	auto& p = player[player_index].player;

	SDLServerGetway::Instance().SendMessage(player_index, "BEGIN_XML_MESSAGE");
	for (const auto& card : p.cards_in_hand)
	{
		GwentMessages::SwapCardMessage msg;
		msg.type = GwentMessages::SwapCardMessage::MessageType::RESPONSE;
		msg.card_name = card.name;
		msg.index = p.indexes[i++];
		std::string msg_str = msg.ToString() + '\n';
		SDLServerGetway::Instance().SendMessage(player_index, msg_str);
	}
	SDLServerGetway::Instance().SendMessage(player_index, "END_XML_MESSAGE");
}

void GameServer::WaitForAkceptDeckPlayer(uint player_index)
{
	if (player[player_index].accepted_deck)
		return;

	SDLServerGetway::Instance().SendMessage(player_index, player[player_index].deck_xml);
	std::cout << "Wait for client 1 accepting deck\n";
	uint user_index;
	if (SDLServerGetway::Instance().WaitForRespone(user_index, "DECK_OK", 0))
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
	if (!SDLServerGetway::Instance().m_IncomingMessages.empty())
	{
		auto message = SDLServerGetway::Instance().GetFirstMessageToUser(player_index);
		GwentMessages::SwapCardMessage msg;

		if (msg.Serialized(message))
		{
			//Max 2 cards can be swaped
			if (player[player_index].swaped_cards_at_start >= 2)
			{
				player[player_index].end_choosing_cards = true;
				SDLServerGetway::Instance().SendMessage(player_index, "END_SWAPING_CARDS");
				return;
			}

			if (player[player_index].player.cards_in_deck.empty())
			{
				player[player_index].end_choosing_cards = true;
				SDLServerGetway::Instance().SendMessage(player_index, "END_SWAPING_CARDS");
				return;
			}

			if (player[player_index].player.cards_in_hand.empty())
			{
				player[player_index].end_choosing_cards = true;
				SDLServerGetway::Instance().SendMessage(player_index, "END_SWAPING_CARDS");
				return;
			}

			if (player[player_index].end_choosing_cards)
			{
				player[player_index].end_choosing_cards = true;
				SDLServerGetway::Instance().SendMessage(player_index, "END_SWAPING_CARDS");
				return;
			}


			auto card = player[player_index].player.cards_in_hand[msg.index];
			if (msg.card_name == card.name)
			{
				uint i;
				if (player[player_index].cards_to_swap.empty())
				{
					i = rand() % player[player_index].player.cards_in_deck.size();
					player[player_index].cards_to_swap[msg.index] = i;
				}
				else
				{
					i = player[player_index].cards_to_swap[msg.index];
				}

				auto swaped_card = player[player_index].player.cards_in_deck[i];				

				GwentMessages::SwapCardMessage response;
				response.card_name = swaped_card.name;
				response.index = i;
				response.prev_card_name = msg.card_name;
				response.prev_index = msg.index;
				response.type = GwentMessages::SwapCardMessage::MessageType::RESPONSE;
				Log("Send swaped card to player : " + std::to_string(player_index)+ "... :\n" + card.name + " id: " + std::to_string(msg.index) + " => " + swaped_card.name + " id: " + std::to_string(i));
				SDLServerGetway::Instance().SendMessage(player_index, response.ToString());
			}
		}
	}
}

void GameServer::AddPushCard(const GwentMessages::PushCardMessage & msg, uint player_index)
{
	uint index = 0;
	auto cards = player[player_index].player.cards_in_hand;
	for (const auto& card : cards)
	{
		if (card.name == msg.card_name)
		{
			GameLines[player_index][card.type].AddCard(card);
			cards.erase(cards.begin() + index);
			return;
		}
		index++;
	}
}

GwentMessages::ScoreMessage GameServer::PrepareScoreMsg(uint player)
{
	GwentMessages::ScoreMessage score_msg;
	score_msg.scorePlayer[GwentMessages::ScoreMessage::Total] = 0;
	score_msg.scoreEnemy[GwentMessages::ScoreMessage::Total] = 0;

	for (auto& line : GameLines[0])
	{
		int s = line.second.CalculateStrengthLine();
		switch (line.first)
		{
		case LineTypes::CLOSE_COMBAT:
			if(player == 0)
				score_msg.scorePlayer[GwentMessages::ScoreMessage::Line1] = s;
			else
				score_msg.scoreEnemy[GwentMessages::ScoreMessage::Line1] = s;
			break;
		case LineTypes::ARCHEER:
			if (player == 0)
				score_msg.scorePlayer[GwentMessages::ScoreMessage::Line2] = s;
			else
				score_msg.scoreEnemy[GwentMessages::ScoreMessage::Line2] = s;
			break;
		case LineTypes::BALIST:
			if (player == 0)
				score_msg.scorePlayer[GwentMessages::ScoreMessage::Line3] = s;
			else
				score_msg.scoreEnemy[GwentMessages::ScoreMessage::Line3] = s;
			break;
		}
		if (player == 0)
			score_msg.scorePlayer[GwentMessages::ScoreMessage::Total] += s;
		else
			score_msg.scoreEnemy[GwentMessages::ScoreMessage::Total] += s;
		
	}

	for (auto& line : GameLines[1])
	{
		int s = line.second.CalculateStrengthLine();
		switch (line.first)
		{
		case LineTypes::CLOSE_COMBAT:
			if (player == 1)
				score_msg.scorePlayer[GwentMessages::ScoreMessage::Line1] = s;
			else
				score_msg.scoreEnemy[GwentMessages::ScoreMessage::Line1] = s;
			break;
		case LineTypes::ARCHEER:
			if (player == 1)
				score_msg.scorePlayer[GwentMessages::ScoreMessage::Line2] = s;
			else
				score_msg.scoreEnemy[GwentMessages::ScoreMessage::Line2] = s;
			break;
		case LineTypes::BALIST:
			if (player == 1)
				score_msg.scorePlayer[GwentMessages::ScoreMessage::Line3] = s;
			else
				score_msg.scoreEnemy[GwentMessages::ScoreMessage::Line3] = s;
			break;
		}
		if (player == 1)
			score_msg.scorePlayer[GwentMessages::ScoreMessage::Total] += s;
		else
			score_msg.scoreEnemy[GwentMessages::ScoreMessage::Total] += s;

	}
	return score_msg;
}

bool GameServer::SwapCardStarProcedure()
{
	SDLServerGetway::Instance().GetMessage();

	SwapCardsPlayer(0);
	SwapCardsPlayer(1);

	if (player[0].end_choosing_cards && player[1].end_choosing_cards)
		return true;

	uint pIndex;
	if (SDLServerGetway::Instance().WaitForRespone(pIndex, "SWAP_CARD_OK", 10))
	{
		std::list<uint> card_to_remove;
		for (const auto& pair : player[pIndex].cards_to_swap)
		{
			auto card = player[pIndex].player.cards_in_hand[pair.first];
			auto swaped_card = player[pIndex].player.cards_in_deck[pair.second];
			player[pIndex].player.cards_in_hand[pair.first] = swaped_card;
			player[pIndex].player.cards_in_deck[pair.second] = card;
			card_to_remove.push_back(pair.first);
		}
		for (const auto& i : card_to_remove)
		{
			player[pIndex].cards_to_swap.erase(i);
		}

		player[pIndex].swaped_cards_at_start++;
		if (player[pIndex].swaped_cards_at_start >= 2)
		{
			player[pIndex].end_choosing_cards = true;
			SDLServerGetway::Instance().SendMessage(pIndex, "END_SWAPING_CARDS");
		}
	}

	return false;
}

bool GameServer::StartGameProcedure()
{	
	if (player_start_game == 3)
	{
		auto i = rand() % 100;
		if (i < 50)
			player_start_game = 0;
		else
			player_start_game = 1;
	}	

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	if(player_start_game == 0)
	{
		GwentMessages::StartGameMessage start_msg;
		start_msg.playerToStart = GwentMessages::Player::PLAYER;
		Log("Send to 0 start msg : \n" + start_msg.ToString());
		SDLServerGetway::Instance().SendMessage(0, start_msg.ToString());
		start_msg.playerToStart = GwentMessages::Player::ENEMY;
		SDLServerGetway::Instance().SendMessage(1, start_msg.ToString());
		Log("Send to 1 start msg : \n" + start_msg.ToString());
		Log("Start game p1 start");
	}
	else if(player_start_game == 1)
	{
		GwentMessages::StartGameMessage start_msg;
		start_msg.playerToStart = GwentMessages::Player::PLAYER;
		SDLServerGetway::Instance().SendMessage(1, start_msg.ToString());
		Log("Send to 0 start msg : \n" + start_msg.ToString());
		start_msg.playerToStart = GwentMessages::Player::ENEMY;
		SDLServerGetway::Instance().SendMessage(0, start_msg.ToString());
		Log("Send to 1 start msg : \n" + start_msg.ToString());
		Log("Start game p2 start");
	}

	return true;
}

bool GameServer::Player1Move()
{
	auto msg = SDLServerGetway::Instance().GetMessage(0);

	if (!msg.empty())
	{
		GwentMessages::PushCardMessage push_card_msg;

		if (push_card_msg.Serialized(msg))
		{
			SDLServerGetway::Instance().SendMessage(0, push_card_msg.ToString());
			push_card_msg.player = GwentMessages::Player::ENEMY;
			SDLServerGetway::Instance().SendMessage(1, push_card_msg.ToString());
			AddPushCard(push_card_msg, 0);

			SDLServerGetway::Instance().SendMessage(0, PrepareScoreMsg(0).ToString());
			SDLServerGetway::Instance().SendMessage(1, PrepareScoreMsg(1).ToString());

			Log("P1 push card :\n" + msg);
		}
	}	

	uint pIndex = 0;
	if (SDLServerGetway::Instance().WaitForRespone(pIndex, "PUSH_CARD_OK", 10) || player[0].player.cards_in_hand.empty())
	{
		//player[pIndex].pushed_card = true;
		SDLServerGetway::Instance().SendMessage(0, std::string("END_MOVE"));
		SDLServerGetway::Instance().SendMessage(1, std::string("START_MOVE"));
		return true;
	}

	if(player[0].pushed_card && player[1].pushed_card)
		return true;

	return false;
}

bool GameServer::Player2Move()
{
	auto msg = SDLServerGetway::Instance().GetMessage(1);

	if (!msg.empty())
	{
		GwentMessages::PushCardMessage push_card_msg;

		if (push_card_msg.Serialized(msg))
		{
			SDLServerGetway::Instance().SendMessage(1, push_card_msg.ToString());
			push_card_msg.player = GwentMessages::Player::ENEMY;
			SDLServerGetway::Instance().SendMessage(0, push_card_msg.ToString());
			AddPushCard(push_card_msg, 1);

			SDLServerGetway::Instance().SendMessage(0, PrepareScoreMsg(0).ToString());
			SDLServerGetway::Instance().SendMessage(1, PrepareScoreMsg(1).ToString());
		

			Log("P2 push card : \n" + msg);
		}
	}

	uint pIndex = 0;
	if (player[1].player.cards_in_hand.empty() || SDLServerGetway::Instance().WaitForRespone(pIndex, "PUSH_CARD_OK", 10))
	{
		//player[pIndex].pushed_card = true;
		SDLServerGetway::Instance().SendMessage(1, std::string("END_MOVE"));
		SDLServerGetway::Instance().SendMessage(0, std::string("START_MOVE"));
		return true;
	}

	if (player[0].pushed_card && player[1].pushed_card)
		return true;

	return false;
}

void GameServer::ChangeState(GameStates state)
{
	current_state = state;
	SDLServerGetway::Instance().ClearMessages();
}
