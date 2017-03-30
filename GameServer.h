#pragma once
#include <SDL2/SDL_net.h>
#include "../../Gwint/GameLine.h"
#include "../../Gwint/Player.h"
#include "../GameEngine/Input/InputManager.h"
#include "../GameEngine/Display/DisplayManager.hpp"
#include <map>

struct GamePlayer
{
	Player player;
	std::string deck_xml;
	uint swaped_cards_at_start = 0;
	bool accepted_deck = false;		 //download deck
	bool have_cards_in_hand = false;
	bool end_choosing_cards = false; //swap cards at the start
};

class GameServer
{
	enum class GameStates
	{
		WAIT_FOR_PLAYERS,
		SEND_DECKS,
		SEND_CARDS_IN_HAND,
		SWAP_CARDS_START,
		START_GAME,
		PLAYER_1_TURN,
		PLAYER_2_TURN
	};
public:
	GameServer()
		: display("Gwint Server", 320, 240, false)
	{}
	void Start();
	bool WaitForPlayers();
	bool NewGameProcedure();
	bool SendCardsInHand();
	bool SwapCardStarProcedure();
	bool StartGameProcedure(uint& player_start_number);

	void SendCardsInHand(uint player_index);
	void WaitForAkceptDeckPlayer(uint player_index);
	void PushCard(uint who, uint i);
	void SwapCardsPlayer(uint player_index);

	void Update();	

	CInputManager m_InputManager;
	CDisplayManager display;

	std::map<LineTypes, GameLine> P1_Lines;
	std::map<LineTypes, GameLine> P2_Lines;

	GamePlayer player[2];

	GameStates current_state = GameStates::WAIT_FOR_PLAYERS;
};