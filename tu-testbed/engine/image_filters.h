// image_filters.h	-- Thatcher Ulrich <tu@tulrich.com> 2002

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Image rescaling routines.


#ifndef IMAGE_FILTERS_H
#define IMAGE_FILTERS_H


namespace image_filters
{
	enum filter_type {
		BOX,
		TRIANGLE,
		BELL,
		B_SPLINE,
		SOME_CUBIC,	// Cubic approximation of Sinc's hump (but no tails).
		LANCZOS3,
		MITCHELL	// This one is alleged to be pretty nice.
	};

	void	resample(SDL_Surface* out,
			 SDL_Surface* in, float in_x0, float in_y0, float in_x1, float in_y1);
};


#endif // IMAGE_FILTERS_H

// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:

