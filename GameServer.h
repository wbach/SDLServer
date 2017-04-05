#pragma once
#include <SDL2/SDL_net.h>
#include <Gwint/GameLine.h>
#include <Gwint/Player.h>
#include <Gwint/Messages/Messages.h>
#include <Input/InputManager.h>
#include <Display/DisplayManager.hpp>
#include <map>

struct GamePlayer
{
	Player player;
	std::string deck_xml;
	uint swaped_cards_at_start = 0;
	bool accepted_deck = false;		 //download deck
	bool accepted_enemy_deck = false;
	bool have_cards_in_hand = false;
	bool end_choosing_cards = false; //swap cards at the start
	bool pushed_card = false;
	std::map<uint, uint> cards_to_swap;
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
		PLAYER_1_MOVE,
		PLAYER_2_MOVE
	};
public:
	GameServer();
	void Start();
	bool WaitForPlayers();
	bool NewGameProcedure();
	bool SendCardsInHand();
	bool SwapCardStarProcedure();
	bool StartGameProcedure();
	bool Player1Move();
	bool Player2Move();
	void ChangeState(GameStates state);

	void SendCardsInHand(uint player_index);
	void WaitForAkceptDeckPlayer(uint player_index);
	void PushCard(uint who, uint i);
	void SwapCardsPlayer(uint player_index);
	void AddPushCard(const GwentMessages::PushCardMessage& msg, uint player_index);
	GwentMessages::ScoreMessage PrepareScoreMsg(uint player);
	void Update();

	CInputManager m_InputManager;
	CDisplayManager display;

	std::map<LineTypes, GameLine> GameLines[2];

	GamePlayer player[2];

	GameStates current_state = GameStates::WAIT_FOR_PLAYERS;

	uint player_start_game = 3; //
};