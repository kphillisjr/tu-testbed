// gameswf.cpp	-- Vitaly Alexeev <tishka92@yahoo.com>	2007

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// gameswf player implementation

#include "gameswf/gameswf_player.h"
#include "gameswf/gameswf_player.h"
#include "base/tu_timer.h"

namespace gameswf
{

	gameswf_player::gameswf_player() :
		mouse_x(0),
		mouse_y(0),
		mouse_buttons(0),
		start_ticks(tu_timer::get_ticks()),
		last_ticks(tu_timer::get_ticks()),
		frame_counter(0),
		last_logged_fps(tu_timer::get_ticks()),
		do_render(true)
	{
	}

	gameswf_player::~gameswf_player()
	{
		// Clean up gameswf as much as possible, so valgrind will help find actual leaks.
		clear_gameswf();
	}
}



// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:

