// tu_types.h	-- Ignacio Casta�o, Thatcher Ulrich 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Minimal typedefs.  Follows SDL conventions; falls back on SDL.h if
// platform isn't obvious.


#ifndef TU_TYPES_H
#define TU_TYPES_H


#include <stdio.h>


#if defined(__I386__) || defined(_WIN32)

	// On known little-endian platforms, define this stuff.
	#define _TU_LITTLE_ENDIAN_	1
	
	typedef unsigned char	Uint8;
	typedef signed char	Sint8;
	typedef unsigned short	Uint16;
	typedef short	Sint16;
	typedef unsigned int	Uint32;
	typedef int	Sint32;
	#ifndef _MSC_VER
		// Probably gcc or something compatible
		typedef unsigned long long	Uint64;
		typedef long long Sint64;
	#else	// _MSC_VER
		typedef unsigned __int64	Uint64;
		typedef __int64	Sint64;
	#endif	// _MSC_VER

#else	// not __I386__ and not _WIN32

	// On unknown platforms, rely on SDL
	#include <SDL.h>
	
	#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	#define _TU_LITTLE_ENDIAN_ 1
	#endif // SDL_BYTEORDER == SDL_LIL_ENDIAN

#endif	// not __I366__ and not _WIN32


// A function to run some validation checks.
bool	tu_types_validate();


#endif // TU_TYPES_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
