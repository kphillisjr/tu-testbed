// chunklod.h	-- tu@tulrich.com Copyright 2001 by Thatcher Ulrich

// Header declaring data structures for chunk LOD rendering.

#ifndef CHUNKLOD_H
#define CHUNKLOD_H


#include <engine/geometry.h>
#include <engine/cull.h>	// Hm.  Put cull stuff in geometry?


struct lod_chunk;

struct render_options {
	bool	show_box;
	bool	show_geometry;
	bool	morph;

	render_options()
		: show_box(false), show_geometry(true), morph(true)
	{
	}
};


class lod_chunk_tree {
// Use this class as the UI to a chunked-LOD object.
// !!! turn this into an interface class and get the data into the .cpp file !!!
public:
	// External interface.
	lod_chunk_tree(SDL_RWops* src);
	void	set_parameters(float max_pixel_error, float screen_width, float horizontal_FOV_degrees);
	void	update(const vec3& viewpoint);
	int	render(const plane_info frustum[6], render_options opt);

	// Used by our contained chunks.
	Uint16	compute_lod(const vec3& center, const vec3& extent, const vec3& viewpoint) const;
//data:
	lod_chunk*	root;
	int	tree_depth;	// from chunk data.
	float	error_LODmax;	// from chunk data.
	float	distance_LODmax;	// computed from chunk data params and set_parameters() inputs.
};


#endif // CHUNKLOD_H
