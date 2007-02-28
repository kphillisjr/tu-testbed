// tu_random.h	-- Thatcher Ulrich 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Pseudorandom number generator.


#ifndef TU_RANDOM_H
#define TU_RANDOM_H


#include "base/tu_config.h"
#include "base/tu_types.h"


namespace tu_random
{
	// Global generator.
	Uint32	next_random();
	void	seed_random(Uint32 seed);
	float	get_unit_float();

	const int	SEED_COUNT = 8;
	
	// In case you need independent generators.  The global
	// generator is just an instance of this.
	struct generator
	{
		generator();
		void	seed_random(Uint32 seed);	// not necessary
		Uint32	next_random();
		float	get_unit_float();

	private:
		Uint32	Q[SEED_COUNT];
		Uint32	c;
		Uint32	i;
	};

}	// end namespace tu_random


#endif // TU_RANDOM_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
