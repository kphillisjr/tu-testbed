// gameswf_log.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Helpers for logging messages & errors.


#ifndef GAMESWF_LOG_H
#define GAMESWF_LOG_H


namespace gameswf
{
	// Printf-style interfaces.

	void	log_msg(const char* fmt, ...);
	void	log_error(const char* fmt, ...);
}


#endif // GAMESWF_LOG_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End: