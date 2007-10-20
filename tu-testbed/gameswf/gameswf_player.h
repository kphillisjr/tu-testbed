// gameswf.cpp	-- Vitaly Alexeev <tishka92@yahoo.com>	2007

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Player 
//TODO

#ifndef GAMESWF_PLAYER_H
#define GAMESWF_PLAYER_H

#include "base/utility.h"
#include "gameswf/gameswf.h"

namespace gameswf
{
	struct player;

	exported_module player* create_player();

	struct player : public ref_counted
	{

		// Mouse state.
		int	mouse_x;
		int	mouse_y;
		int	mouse_buttons;

		Uint32	start_ticks;
		Uint32	last_ticks;
		int	frame_counter;
		int	last_logged_fps;
		bool	do_render;

		player();
		~player();

		void run();

	};
}

#endif

