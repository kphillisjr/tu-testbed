// constrained_triangulate.h	-- Thatcher Ulrich 2004

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Code to triangulate arbitrary 2D polygonal regions.
//
// Ear-clipping (similar to FIST), but not relying on topology of the
// loops to untangle coincident verts; instead, when handling
// coincident verts use robust ear checks.  This avoids complicated
// non-local analysis when joining loops together.


#include "base/constrained_triangulate.h"
#include "base/grid_index.h"
#include "base/tu_random.h"
#include "base/vert_types.h"
#include <algorithm>


#define PROFILE_TRIANGULATE
#ifdef PROFILE_TRIANGULATE
#include "base/tu_timer.h"
#endif // PROFILE_TRIANGULATE


struct poly_vert {
	vec2<sint16> m_v;
	int m_next;
	int m_prev;
	enum state_enum {
		DIRTY = 0,
		REFLEX,
		DELETED,
	};
	state_enum m_state;

	poly_vert() : m_next(-1), m_prev(-1), m_state(DIRTY) {}
	poly_vert(sint16 x, sint16 y, int prev, int next) : m_v(x, y), m_next(next), m_prev(prev), m_state(DIRTY) {}

	index_point<sint16>	get_index_point() const { return index_point<sint16>(m_v.x, m_v.y); }
};


// Represents an edge.  Stores the indices of the two verts.
struct edge {
	int m_0, m_1;

	edge(int v0 = -1, int v1 = -1) : m_0(v0), m_1(v1) {}

	static bool sort_by_startpoint(const edge& a, const edge& b) {
		if (a.m_0 < b.m_0) {
			return true;
		} else if (a.m_0 == b.m_0) {
			return a.m_1 < b.m_1;
		} else {
			return false;
		}
	}

	static bool sort_by_endpoint(const edge& a, const edge& b) {
		if (a.m_1 < b.m_1) {
			return true;
		} else if (a.m_1 == b.m_1) {
			return a.m_0 < b.m_0;
		} else {
			return false;
		}
	}
};

struct active_edge : public edge {
	bool m_in;

	active_edge(int v0 = -1, int v1 = -1, bool in = false) : edge(v0, v1), m_in(in) {}
	active_edge(const edge& e, bool in = false) : edge(e), m_in(in) {}
};


// Info for path joining.
struct path_info {
	// Paths have contiguous indices in original input vert array.
	int m_begin_vert_orig;  // original index of first vert in path
	int m_end_vert_orig;    // original index of last+1 vert in path
	int m_leftmost_vert;

	path_info() :
		m_begin_vert_orig(-1), m_end_vert_orig(-1),
		m_leftmost_vert(-1) {
	}

	bool operator<(const path_info& pi) {
		assert(m_leftmost_vert >= 0);
		assert(pi.m_leftmost_vert >= 0);

		return m_leftmost_vert < pi.m_leftmost_vert;
	}
};


// Triangulator state.
struct tristate {
	tristate()
		: m_next_dirty(0), m_reflex_point_index(NULL), m_edge_index(NULL)
	{
	}
	~tristate()
	{
		delete m_reflex_point_index;
		delete m_edge_index;
	}
	
	array<sint16>* m_results;
	array<poly_vert> m_verts;
	array<path_info> m_input_paths;

	int m_next_dirty;  // points into the m_verts list

	index_box<sint16> m_bbox;
	
	// A search index for fast checking of reflex verts within a
	// triangle.
	//
	// TODO: we don't need a payload, but having one bloats the
	// index by 2x.  Is it worth making a version of
	// grid_index_point that stores the point locations only, with
	// no payload?
	grid_index_point<sint16, bool>* m_reflex_point_index;

	grid_index_box<sint16, bool>* m_edge_index;

	// For debugging.
	int m_debug_halt_step;
	array<sint16>* m_debug_edges;
};


int	compare_vertices(const poly_vert& a, const poly_vert& b)
// For qsort.  Sort by x, then by y.
{
	if (a.m_v.x < b.m_v.x) {
		return -1;
	} else if (a.m_v.x > b.m_v.x) {
		return 1;
	} else {
		if (a.m_v.y < b.m_v.y)
			return -1;
		else if (a.m_v.y > b.m_v.y)
			return 1;
	}

	return 0;
}


// Helper for sorting verts.
struct vert_index_sorter {
	vert_index_sorter(const array<poly_vert>& verts)
		: m_verts(verts) {
	}

	// Return true if m_verts[a] < m_verts[b].
	bool operator()(int a, int b) {
		return compare_vertices(m_verts[a], m_verts[b]) == -1;
	}

	const array<poly_vert>& m_verts;
};


void edges_intersect_sub(int* e0_vs_e1, int* e1_vs_e0,
			 const vec2<sint16>& e0v0, const vec2<sint16>& e0v1,
			 const vec2<sint16>& e1v0, const vec2<sint16>& e1v1)
// Return {-1,0,1} for edge A {crossing, vert-touching, not-crossing}
// the line of edge B.
//
// Specialized for sint16
{
	// If e1v0,e1v1 are on opposite sides of e0, and e0v0,e0v1 are
	// on opposite sides of e1, then the segments cross.  These
	// are all determinant checks.

	// The main degenerate case we need to watch out for is if
	// both segments are zero-length.
	//
	// If only one is degenerate, our tests are still OK.

	if (e0v0.x == e0v1.x && e0v0.y == e0v1.y)
	{
		// e0 is zero length.
		if (e1v0.x == e1v1.x && e1v0.y == e1v1.y)
		{
			if (e1v0.x == e0v0.x && e1v0.y == e0v0.y) {
				// Coincident.
				*e0_vs_e1 = 0;
				*e1_vs_e0 = 0;
				return;
			}
		}
	}

	// See if e1 crosses line of e0.
	sint64	det10 = determinant_sint16(e0v0, e0v1, e1v0);
	sint64	det11 = determinant_sint16(e0v0, e0v1, e1v1);

	// Note: we do > 0, which means a vertex on a line counts as
	// intersecting.  In general, if one vert is on the other
	// segment, we have to go searching along the path in either
	// direction to see if it crosses or not, and it gets
	// complicated.  Better to treat it as intersection.

	int det1sign = 0;
	if (det11 < 0) det1sign = -1;
	else if (det11 > 0) det1sign = 1;
	if (det10 < 0) det1sign = -det1sign;
	else if (det10 == 0) det1sign = 0;

	if (det1sign > 0) {
		// e1 doesn't cross the line of e0.
		*e1_vs_e0 = 1;
	} else if (det1sign < 0) {
		// e1 does cross the line of e0.
		*e1_vs_e0 = -1;
	} else {
		// One (or both) of the endpoints of e1 are on the
		// line of e0.
		*e1_vs_e0 = 0;
	}

	// See if e0 crosses line of e1.
	sint64	det00 = determinant_sint16(e1v0, e1v1, e0v0);
	sint64	det01 = determinant_sint16(e1v0, e1v1, e0v1);

	int det0sign = 0;
	if (det01 < 0) det0sign = -1;
	else if (det01 > 0) det0sign = 1;
	if (det00 < 0) det0sign = -det0sign;
	else if (det00 == 0) det0sign = 0;

	if (det0sign > 0) {
		// e0 doesn't cross the line of e1.
		*e0_vs_e1 = 1;
	} else if (det0sign < 0) {
		// e0 crosses line of e1
		*e0_vs_e1 = -1;
	} else {
		// One (or both) of the endpoints of e0 are on the
		// line of e1.
		*e0_vs_e1 = 0;
	}
}


bool any_edge_intersects(const tristate* ts, const edge& e, grid_index_box<sint16, bool>* edge_index)
// Return true if any edge intersects the edge e.
//
// Intersection is defined as any part of an edgeset edge touching any
// part of the *interior* of e.
{
	const vec2<sint16>& ev0 = ts->m_verts[e.m_0].m_v;
	const vec2<sint16>& ev1 = ts->m_verts[e.m_1].m_v;
	
	index_box<sint16> bound(ev0.x, ev0.y);
	bound.expand_to_enclose(ev1.x, ev1.y);

	for (grid_index_box<sint16, bool>::iterator it = edge_index->begin(bound); !it.at_end(); ++it) {
		vec2<sint16> eev0(it->bound.get_min().x, it->bound.get_min().y);
		vec2<sint16> eev1(it->bound.get_max().x, it->bound.get_max().y);
		if (it->value == false) {
			// Edge crosses the indexed bounding box w/ negative slope, not positive.
			swap(&eev0.y, &eev1.y);
		}

		int e_vs_ee, ee_vs_e;
		edges_intersect_sub(&e_vs_ee, &ee_vs_e, ev0, ev1, eev0, eev1);

		bool e_crosses_line_of_ee = e_vs_ee < 0;
		bool ee_touches_line_of_e = ee_vs_e <= 0;
		if (e_crosses_line_of_ee && ee_touches_line_of_e) {
			return true;
		}
	}
	return false;
}


int find_valid_bridge_vert(const tristate* ts, int v1, grid_index_box<sint16, bool>* edge_index)
// Find v2 such that v2 is left of v1, and the segment v1-v2 doesn't
// cross any edges.
{
	// We work backwards from v1.  Normally we shouldn't have to
	// look very far.
	assert(v1 > 0);
	for (int i = v1 - 1; i >= 0; i--) {
		if (!any_edge_intersects(ts, edge(v1, i), edge_index)) {
			// This vert is good.
			return i;
		}
	}

	// If we get here, then the input is not well formed.
	// TODO log something
	assert(0);  // temp xxxxx

	// Default fallback: join to the next-most-leftmost vert,
	// regardless of crossing other edges.
	return v1 - 1;
}


void add_edge(grid_index_box<sint16, bool>* edge_index, const vec2<sint16>& v0, const vec2<sint16>& v1)
{
	bool m_slope_up = (v1.x - v0.x) * (v1.y - v0.y) > 0;
	index_box<sint16> bound(v0.x, v0.y);
	bound.expand_to_enclose(v1.x, v1.y);
	edge_index->add(bound, m_slope_up);
}


void add_all_edges_into_index(tristate* ts, grid_index_box<sint16, bool>* edge_index)
{
	for (int i = 0; i < ts->m_verts.size(); i++) {
		const vec2<sint16>& v0 = ts->m_verts[i].m_v;
		const vec2<sint16>& v1 = ts->m_verts[ts->m_verts[i].m_next].m_v;
		add_edge(edge_index, v0, v1);
	}
}


void join_paths_into_one_poly(tristate* ts)
// Use zero-area bridges to connect separate polys & islands into one
// big continuous poly.
//
// Uses info from ts->m_input_paths.
{
	// Connect separate paths with bridge edges, into one big path.
	//
	// Bridges are zero-area regions that connect a vert on each
	// of two paths.
	if (ts->m_input_paths.size() > 1) {
		// Sort polys in order of each poly's leftmost vert.
		std::sort(&ts->m_input_paths[0], &ts->m_input_paths[0] + ts->m_input_paths.size());
		assert(ts->m_input_paths[0].m_leftmost_vert <= ts->m_input_paths[1].m_leftmost_vert);

		// Init index to speed up edge-crossing checks.
		ts->m_edge_index = new grid_index_box<sint16,bool>(GRID_INDEX_AUTOSIZE, ts->m_bbox, ts->m_verts.size());
		add_all_edges_into_index(ts, ts->m_edge_index);
		
		// Iterate from left to right
		for (int i = 1; i < ts->m_input_paths.size(); i++) {
			const path_info& pi = ts->m_input_paths[i];
			
			int	v1 = pi.m_leftmost_vert;
			assert(v1 >= 0);

			//     find a vert v2, such that:
			//       v2 is to the left of v1,
			//       and v1-v2 seg doesn't intersect any other edges

			//     // (note that since v1 is next-most-leftmost, v1-v2 can't
			//     // hit anything in pi, nor any paths further down the list,
			//     // it can only hit edges in the joined poly) (need to think
			//     // about equality cases)
			//
			if (v1 > 0) {
				int	v2 = find_valid_bridge_vert(ts, v1, ts->m_edge_index);
				assert(v2 != v1);

				// Join pi.

				// We're going from:
				//
				// >---v2----->
				//         
				//   <----v1------<
				//
				// to:
				//
				// >---v2 n2-->
				//      v\ \^ 
				//   <---v1 n1----<
				//
				// (v1 and n1 are coincident, v2 and
				// n2 are coincident, gap exaggerated
				// for clarity).

				int n1 = ts->m_verts.size();
				int n2 = n1 + 1;
				ts->m_verts.resize(ts->m_verts.size() + 2);
				ts->m_verts[n1] = ts->m_verts[v1];
				ts->m_verts[n2] = ts->m_verts[v2];

				ts->m_verts[v1].m_prev = v2;
				ts->m_verts[v2].m_next = v1;
				ts->m_verts[n1].m_next = n2;
				ts->m_verts[n2].m_prev = n1;
				ts->m_verts[ts->m_verts[n1].m_prev].m_next = n1;
				ts->m_verts[ts->m_verts[n2].m_next].m_prev = n2;

				add_edge(ts->m_edge_index, ts->m_verts[v1].m_v, ts->m_verts[v2].m_v);

				assert(ts->m_verts[ts->m_verts[v1].m_prev].m_next == v1);
				assert(ts->m_verts[ts->m_verts[v1].m_next].m_prev == v1);
				assert(ts->m_verts[ts->m_verts[v2].m_prev].m_next == v2);
				assert(ts->m_verts[ts->m_verts[v2].m_next].m_prev == v2);
				assert(ts->m_verts[ts->m_verts[n1].m_prev].m_next == n1);
				assert(ts->m_verts[ts->m_verts[n1].m_next].m_prev == n1);
				assert(ts->m_verts[ts->m_verts[n2].m_prev].m_next == n2);
				assert(ts->m_verts[ts->m_verts[n2].m_next].m_prev == n2);
			}
			// else no joining required; vert[0] is already joined and v1 is coincident with it
			
			// TODO: update edge index
		}
	}

	// TODO delete m_edge_index
}



void sort_and_remap(tristate* ts)
// Sort the verts by position, and remap the things that refer to
// verts.
{
	// Sort.
	array<poly_vert> verts = ts->m_verts;
	array<int> vert_indices(verts.size());   // verts[vert_indices[0]] --> leftmost vert
	for (int i = 0; i < verts.size(); i++) {
		vert_indices[i] = i;
	}
	vert_index_sorter sorter(verts);
	std::sort(&vert_indices[0], &vert_indices[0] + vert_indices.size(), sorter);

	// Make the old-to-new mapping.
	array<int> old_to_new;
	old_to_new.resize(verts.size());

// 	// Remove dupes.
// 	int last_unique_vi = 0;
// 	for (int i = 1; i < vert_indices.size(); i++) {
// 		int old_index = vert_indices[i];
// 		if (verts[old_index].m_v == verts[vert_indices[last_unique_vi]].m_v) {
// 			// Dupe!  Don't increment last_unique_vi.
// 		} else {
// 			last_unique_vi++;
// 			vert_indices[last_unique_vi] = old_index;
// 			old_to_new[old_index] = last_unique_vi;
// 		}
// 		old_to_new[old_index] = last_unique_vi;
// 	}
// 	vert_indices.resize(last_unique_vi + 1);  // drop duped verts.

	// Make old_to_new mapping.
	for (int i = 0; i < vert_indices.size(); i++) {
		int old_index = vert_indices[i];
		old_to_new[old_index] = i;
	}

	// Make output array.
	ts->m_verts.resize(vert_indices.size());
	for (int i = 0; i < vert_indices.size(); i++) {
		ts->m_verts[i] = verts[vert_indices[i]];
	}

	// Remap edge indices.
	for (int i = 0; i < ts->m_verts.size(); i++) {
		ts->m_verts[i].m_next = old_to_new[ts->m_verts[i].m_next];
		ts->m_verts[i].m_prev = old_to_new[ts->m_verts[i].m_prev];
	}

	// Update path info.
	for (int i = 0; i < ts->m_input_paths.size(); i++) {
		ts->m_input_paths[i].m_leftmost_vert = old_to_new[ts->m_input_paths[i].m_leftmost_vert];
	}
}


void init(tristate* ts, array<sint16>* results, int path_count, const array<sint16> paths[],
	  int debug_halt_step, array<sint16>* debug_edges)
// Pull the paths into *tristate.
{
	assert(results);
	ts->m_results = results;
	ts->m_debug_halt_step = debug_halt_step;
	ts->m_debug_edges = debug_edges;
	ts->m_next_dirty = 0;

	// Dump verts and edges into tristate.
	ts->m_input_paths.resize(path_count);
	for (int i = 0; i < path_count; i++) {
		assert((paths[i].size() & 1) == 0);

		// Keep some info about the path itself (so we can
		// sort and join paths later).
		path_info* pi = &ts->m_input_paths[i];
		pi->m_begin_vert_orig = ts->m_verts.size();

		// Grab all the verts and edges.
		int path_length = paths[i].size() / 2;
		int previous_vert = ts->m_verts.size() + path_length - 1;
		for (int j = 0; j < paths[i].size(); j += 2) {
			int vert_index = ts->m_verts.size();
			ts->m_verts.push_back(poly_vert(paths[i][j], paths[i][j + 1], previous_vert, vert_index + 1));
			previous_vert = vert_index;
			
			// Update bounding box.
			if (vert_index == 0) {
				ts->m_bbox.set_to_point(ts->m_verts.back().get_index_point());
			} else {
				ts->m_bbox.expand_to_enclose(ts->m_verts.back().get_index_point());
			}

			
			if (pi->m_leftmost_vert == -1
			    || compare_vertices(ts->m_verts[pi->m_leftmost_vert], ts->m_verts[vert_index]) > 0) {
				pi->m_leftmost_vert = vert_index;
			}
		}
		ts->m_verts.back().m_next = pi->m_begin_vert_orig;  // close the path
		pi->m_end_vert_orig = ts->m_verts.size();
	}

	// Init reflex point search index.
	//
	// TODO: experiment with estimated item count.
 	ts->m_reflex_point_index = new grid_index_point<sint16, bool>(GRID_INDEX_AUTOSIZE, ts->m_bbox, ts->m_verts.size() / 2);

	// TODO: since we're keeping loop info in the verts, we could
	// do this much more simply.
	for (int i = 0; i < ts->m_input_paths.size(); i++) {
		const path_info* pi = &ts->m_input_paths[i];
		
		// Identify any reflex verts and put them in an index.
		int pathsize = pi->m_end_vert_orig - pi->m_begin_vert_orig;
		if (pathsize > 2) {
			// Thanks Sean Barrett for the fun/sneaky
			// trick for iterating over subsequences of 3
			// verts.
			for (int j = pi->m_begin_vert_orig, k = pi->m_end_vert_orig - 1, l = pi->m_end_vert_orig - 2;
			     j < pi->m_end_vert_orig;
			     l = k, k = j, j++) {
				const vec2<sint16>& v0 = ts->m_verts[l].m_v;
				const vec2<sint16>& v1 = ts->m_verts[k].m_v;
				const vec2<sint16>& v2 = ts->m_verts[j].m_v;
				if (vertex_left_test(v0, v1, v2) <= 0) {
					ts->m_reflex_point_index->add(index_point<sint16>(v1.x, v1.y), 0);
					// TODO: mark ts->m_verts[k] as REFLEX (or COINCIDENT...)
				}
			}
		}
	}

	// TODO: optionally find & fix edge intersections

	sort_and_remap(ts);
	if (ts->m_input_paths.size() > 1) {
		join_paths_into_one_poly(ts);
		sort_and_remap(ts);
	}

	ts->m_results->reserve(2 * 3 * ts->m_verts.size());
}


bool vert_in_triangle(const vec2<sint16>& v, const vec2<sint16>& v0, const vec2<sint16>& v1, const vec2<sint16>& v2)
// Return true if v touches the boundary or the interior of triangle (v0, v1, v2).  
{
	sint64 det0 = determinant_sint16(v0, v1, v);
	if (det0 >= 0) {
		sint64 det1 = determinant_sint16(v1, v2, v);
		if (det1 >= 0) {
			sint64 det2 = determinant_sint16(v2, v0, v);
			if (det2 >= 0) {
				// Point touches the triangle.
				return true;
			}
		}
	}
	return false;
}


bool any_reflex_vert_in_triangle(const tristate* ts, int vi0, int vi1, int vi2)
// Return true if there is any reflex vertex in tristate that touches
// the interior or edges of the given triangle.  Verts coincident with
// the actual triangle verts will return false.
{
	const vec2<sint16>& v0 = ts->m_verts[vi0].m_v;
	const vec2<sint16>& v1 = ts->m_verts[vi1].m_v;
	const vec2<sint16>& v2 = ts->m_verts[vi2].m_v;

	const index_point<sint16>& ip0 = ts->m_verts[vi0].get_index_point();
	const index_point<sint16>& ip1 = ts->m_verts[vi1].get_index_point();
	const index_point<sint16>& ip2 = ts->m_verts[vi2].get_index_point();

	// Compute the bounding box of reflex verts we want to check.
	index_box<sint16>	query_bound(ip0);
	query_bound.expand_to_enclose(ip1);
	query_bound.expand_to_enclose(ip2);

	for (grid_index_point<sint16, bool>::iterator it = ts->m_reflex_point_index->begin(query_bound);
	     ! it.at_end();
	     ++it)
	{
		if (ip0 == it->location || ip1 == it->location || ip2 == it->location) {
			continue;
		}

		if (query_bound.contains_point(it->location)) {
			vec2<sint16> v(it->location.x, it->location.y);
			if (vert_in_triangle(v, v0, v1, v2)) {
				return true;
			}
		}
	}
	return false;
}


bool vertex_in_cone(const vec2<sint16>& vert,
		    const vec2<sint16>& cone_v0, const vec2<sint16>& cone_v1, const vec2<sint16>& cone_v2)
// Returns true if vert is within the cone defined by [v0,v1,v2].
/*
//  (out)  v0
//        /
//    v1 <   (in)
//        \
//         v2
*/
{
	bool	acute_cone = vertex_left_test(cone_v0, cone_v1, cone_v2) > 0;

	// Include boundary in our tests.
	bool	left_of_01 =
		vertex_left_test(cone_v0, cone_v1, vert) >= 0;
	bool	left_of_12 =
		vertex_left_test(cone_v1, cone_v2, vert) >= 0;

	if (acute_cone)
	{
		// Acute cone.  Cone is intersection of half-planes.
		return left_of_01 && left_of_12;
	}
	else
	{
		// Obtuse cone.  Cone is union of half-planes.
		return left_of_01 || left_of_12;
	}
}


void fill_debug_out(tristate* ts)
{
	for (int i = 0; i < ts->m_verts.size(); i++) {
		if (ts->m_verts[i].m_state == poly_vert::DELETED) {
			continue;
		}
		const vec2<sint16>& v0 = ts->m_verts[i].m_v;
		const vec2<sint16>& v1 = ts->m_verts[ts->m_verts[i].m_next].m_v;
		const vec2<sint16>& vprev = ts->m_verts[ts->m_verts[i].m_prev].m_v;
		
		ts->m_debug_edges->push_back(v0.x);
		ts->m_debug_edges->push_back(v0.y);
		ts->m_debug_edges->push_back(v1.x);
		ts->m_debug_edges->push_back(v1.y);
		
		ts->m_debug_edges->push_back(v0.x);
		ts->m_debug_edges->push_back(v0.y);
		ts->m_debug_edges->push_back(vprev.x);
		ts->m_debug_edges->push_back(vprev.y);
	}
}


bool check_debug_dump(tristate* ts)
// If we should debug dump now, this function returns true and fills
// the debug output.
//
// Call this each iteration of triangulate_plane(); if it returns
// true, then return early.
{
	if (ts->m_debug_halt_step > 0) {
		ts->m_debug_halt_step--;
		if (ts->m_debug_halt_step == 0) {
			// Emit some debug info.
			fill_debug_out(ts);
			return true;
		}
	}
	return false;
}


#define DEBUG_MARKUP
#ifdef DEBUG_MARKUP

vec2<sint16> debug_centroid(const tristate* ts, int vi0, int vi1, int vi2)
{
	int x = ts->m_verts[vi0].m_v.x + ts->m_verts[vi1].m_v.x + ts->m_verts[vi2].m_v.x;
	int y = ts->m_verts[vi0].m_v.y + ts->m_verts[vi1].m_v.y + ts->m_verts[vi2].m_v.y;

	return vec2<sint16>(x / 3, y / 3);
}


void debug_make_x(array<sint16>* out, const vec2<sint16>& v)
{
	out->push_back(v.x - 200);
	out->push_back(v.y - 200);
	out->push_back(v.x + 200);
	out->push_back(v.y + 200);
	out->push_back(v.x - 200);
	out->push_back(v.y + 200);
	out->push_back(v.x + 200);
	out->push_back(v.y - 200);
}


void debug_make_plus(array<sint16>* out, const vec2<sint16>& v)
{
	out->push_back(v.x);
	out->push_back(v.y - 200);
	out->push_back(v.x);
	out->push_back(v.y + 200);
	out->push_back(v.x - 200);
	out->push_back(v.y);
	out->push_back(v.x + 200);
	out->push_back(v.y);
}


void debug_make_square(array<sint16>* out, const vec2<sint16>& v)
{
	out->push_back(v.x - 200);
	out->push_back(v.y - 200);
	out->push_back(v.x + 200);
	out->push_back(v.y - 200);

	out->push_back(v.x + 200);
	out->push_back(v.y - 200);
	out->push_back(v.x + 200);
	out->push_back(v.y + 200);

	out->push_back(v.x + 200);
	out->push_back(v.y + 200);
	out->push_back(v.x - 200);
	out->push_back(v.y + 200);

	out->push_back(v.x - 200);
	out->push_back(v.y + 200);
	out->push_back(v.x - 200);
	out->push_back(v.y - 200);
}

#endif // DEBUG_MARKUP


int find_ear_vertex(const tristate* ts, int vi0, int vi1)
// Find a vertex index vi2 such that:
//
// * vi0-vi1-vi2 is the sharpest left turn through vi0-vi1, and there
// are no incoming edges in the cone of vi0-vi1-vi2.
//
// * vi0-vi1 and vi1-vi2 are not both degenerate.
//
// * triangle vi0-vi1-vi2 doesn't contain any (reflex) verts
//
// Return -1 if we don't find anything valid.
{
	assert(vi0 != vi1);
	if (ts->m_verts[vi0].m_v == ts->m_verts[vi1].m_v) {
		// Zero-length edge.  Treat it like an ear, to get rid of it!
		//
		// TODO: should we eliminate zero-length edges before
		// we even make them?
		int vi1n = ts->m_verts[vi1].m_next;
		return vi1n;
	}

	assert(ts->m_verts[vi1].m_state != poly_vert::DELETED);
	if (ts->m_verts[vi1].m_state == poly_vert::REFLEX) {
		// A reflex vert can't make an ear.
		// A reflex vert should not be coincident with any other live verts!
		assert(vi1 == 0
		       || ts->m_verts[vi1 - 1].m_v != ts->m_verts[vi1].m_v
		       || ts->m_verts[vi1 - 1].m_state == poly_vert::DELETED);
		assert(vi1 == ts->m_verts.size() - 1
		       || ts->m_verts[vi1 + 1].m_v != ts->m_verts[vi1].m_v
		       || ts->m_verts[vi1 + 1].m_state == poly_vert::DELETED);
		return -1;
	}

	// Find an outgoing edge from vi1, whose other vert is a valid
	// left turn for vi0-vi1.

	// Find the set of verts coincident with e.m_1.
	int vi1_begin = vi1;
	while (vi1_begin > 0 && ts->m_verts[vi1_begin - 1].m_v == ts->m_verts[vi1].m_v) {
		vi1_begin--;
	}
	int vi1_end = vi1 + 1;
	while (vi1_end < ts->m_verts.size() && ts->m_verts[vi1_end].m_v == ts->m_verts[vi1].m_v) {
		vi1_end++;
	}
	
	// Find inside-most outgoing edge.
	int vi2 = -1;
	for (int i = vi1_begin; i < vi1_end; i++) {
		int v = ts->m_verts[i].m_next;
		if (ts->m_verts[v].m_state == poly_vert::DELETED) {
			continue;
		}
		
		// Is this a valid out edge?
		if (vertex_left_test(ts->m_verts[vi0].m_v, ts->m_verts[vi1].m_v, ts->m_verts[v].m_v) > 0) {
			// Is this the inside-most outgoing edge so far?
			if (vi2 == -1
			    || vertex_in_cone(ts->m_verts[v].m_v,
					      ts->m_verts[vi0].m_v, ts->m_verts[vi1].m_v, ts->m_verts[vi2].m_v)) {
				vi2 = v;
			}
		}
	}
	if (vi2 == -1) {
		return -1;
	}

	// See if any in-edge is in our cone (thus blocking the ear).
	for (int i = vi1_begin; i < vi1_end; i++) {
		int v = ts->m_verts[i].m_prev;
		if (ts->m_verts[v].m_state == poly_vert::DELETED) {
			continue;
		}
		
		if (ts->m_verts[v].m_v != ts->m_verts[vi0].m_v
		    && ts->m_verts[v].m_v != ts->m_verts[vi2].m_v
		    && vertex_left_test(ts->m_verts[vi0].m_v, ts->m_verts[vi1].m_v, ts->m_verts[v].m_v) > 0
		    && vertex_in_cone(ts->m_verts[v].m_v,
				      ts->m_verts[vi0].m_v, ts->m_verts[vi1].m_v, ts->m_verts[vi2].m_v)) {
			// Can't clip this ear; there's an edge in the way.
#ifdef DEBUG_MARKUP
			vec2<sint16> centroid = debug_centroid(ts, vi0, vi1, vi2);
			debug_make_square(ts->m_debug_edges, debug_centroid(ts, vi0, vi1, vi2));
#endif // DEBUG_MARKUP
			return -1;
		}
	}

	// Make sure at least one of the ear sides is not
	// degenerate.
	int valence0 = 0;
	int valence1 = 0;
	for (int i = vi1_begin; i < vi1_end; i++) {
		int v_in = ts->m_verts[i].m_prev;
		if (ts->m_verts[v_in].m_state == poly_vert::DELETED) {
			continue;
		}
		
		if (ts->m_verts[v_in].m_v == ts->m_verts[vi2].m_v) {
			// coincident with v2, but in reverse
			valence1--;
		} else if (ts->m_verts[v_in].m_v == ts->m_verts[vi0].m_v) {
			// coincident with v0
			valence0++;
		}

		int v_out = ts->m_verts[i].m_next;
		if (ts->m_verts[v_out].m_v == ts->m_verts[vi2].m_v) {
			// coincident with v1-v2
			valence1++;
		} else if (ts->m_verts[v_out].m_v == ts->m_verts[vi0].m_v) {
			// coincident with e, but in reverse
			valence0--;
		}
	}
	if (valence0 < 1 && valence1 < 1) {
		// valence error
#ifdef DEBUG_MARKUP
		vec2<sint16> centroid = debug_centroid(ts, vi0, vi1, vi2);
		debug_make_x(ts->m_debug_edges, debug_centroid(ts, vi0, vi1, vi2));
#endif // DEBUG_MARKUP
		return -1;
	}

	if (any_reflex_vert_in_triangle(ts, vi0, vi1, vi2)) {
#ifdef DEBUG_MARKUP
		vec2<sint16> centroid = debug_centroid(ts, vi0, vi1, vi2);
		debug_make_plus(ts->m_debug_edges, debug_centroid(ts, vi0, vi1, vi2));
#endif // DEBUG_MARKUP
		return -1;
	}

	return vi2;
}


void check_loops_valid(tristate* ts)
// assert if there's anything amiss in our vertex loops.
{
// Can be a bit expensive; enable to help debug problems.
#if 0
	for (int i = 0; i < ts->m_verts.size(); i++) {
		const poly_vert& v = ts->m_verts[i];
		if (v.m_state == poly_vert::DELETED) {
			continue;
		}
		
		const poly_vert& vp = ts->m_verts[v.m_prev];
		const poly_vert& vn = ts->m_verts[v.m_next];

		assert(vp.m_next == i);
		assert(vn.m_prev == i);
	}
#endif
}


bool find_and_clip_ear(tristate* ts)
// Return true if we found an ear to clip.
{
	while (ts->m_next_dirty < ts->m_verts.size()) {
		int vi1 = ts->m_next_dirty;
		int vi0 = ts->m_verts[vi1].m_prev;
		ts->m_next_dirty++;

		if (ts->m_verts[vi1].m_state == poly_vert::DELETED) {
			continue;
		}
		assert(ts->m_verts[vi0].m_state != poly_vert::DELETED);

		if (vi0 == vi1) {
			// TODO: would be nicer not to make these at all.
			continue;
		}
		int vi2 = find_ear_vertex(ts, vi0, vi1);
		if (vi2 >= 0) {
			assert(vi2 != vi0);
			assert(vi2 != vi1);
			
			// clip it!

			//        v2---
			//    \   |
			//     v0-v1
			//
			// to:
			//
			//        v2---
			//    \  / 
			//     v0 v1

			assert(ts->m_verts[vi0].m_next == vi1);
			assert(ts->m_verts[vi1].m_prev == vi0);

			int vi2p = ts->m_verts[vi2].m_prev;
			assert(ts->m_verts[vi2p].m_next == vi2);
			if (vi2p != vi1) {
				// Fixup.
				int vi1n = ts->m_verts[vi1].m_next;
				ts->m_verts[vi2p].m_next = vi1n;
				ts->m_verts[vi1n].m_prev = vi2p;

				ts->m_verts[vi2].m_prev = vi1;
				ts->m_verts[vi1].m_next = vi2;

				assert(ts->m_verts[ts->m_verts[vi2p].m_prev].m_next == vi2p);
				assert(ts->m_verts[ts->m_verts[vi2p].m_next].m_prev == vi2p);
				assert(ts->m_verts[ts->m_verts[vi1n].m_prev].m_next == vi1n);
				assert(ts->m_verts[ts->m_verts[vi1n].m_next].m_prev == vi1n);
				assert(ts->m_verts[ts->m_verts[vi1].m_prev].m_next == vi1);
				assert(ts->m_verts[ts->m_verts[vi1].m_next].m_prev == vi1);
				assert(ts->m_verts[ts->m_verts[vi2].m_prev].m_next == vi2);
				assert(ts->m_verts[ts->m_verts[vi2].m_next].m_prev == vi2);

				check_loops_valid(ts);
			}
			
			ts->m_verts[vi1].m_state = poly_vert::DELETED;
			ts->m_verts[vi1].m_prev = vi1;
			ts->m_verts[vi1].m_next = vi1;
			
			ts->m_verts[vi0].m_next = vi2;
			ts->m_verts[vi2].m_prev = vi0;

			const poly_vert& pvi0 = ts->m_verts[vi0];
			const poly_vert& pvi1 = ts->m_verts[vi1];
			const poly_vert& pvi2 = ts->m_verts[vi2];

			// Update m_next_dirty, in case we modified a
			// vert earlier in the array.
			if (vi0 < ts->m_next_dirty) {
				ts->m_next_dirty = vi0;
			}
			if (vi1 < ts->m_next_dirty) {
				ts->m_next_dirty = vi1;
			}
			if (vi2 < ts->m_next_dirty) {
				ts->m_next_dirty = vi2;
			}
			// Make sure we include any previous verts coincident with m_next_dirty.
			while (ts->m_next_dirty > 0
			       && ts->m_verts[ts->m_next_dirty - 1].m_v == ts->m_verts[ts->m_next_dirty].m_v) {
				ts->m_next_dirty--;
			}

			// Verify the consistency of our link edits.
			assert(pvi0.m_next == vi2);
			assert(ts->m_verts[pvi0.m_prev].m_next == vi0);
			assert(pvi2.m_prev == vi0);
			assert(ts->m_verts[pvi2.m_next].m_prev == vi2);

			assert(ts->m_verts[ts->m_verts[vi0].m_prev].m_next == vi0);
			assert(ts->m_verts[ts->m_verts[vi0].m_next].m_prev == vi0);
			assert(ts->m_verts[ts->m_verts[vi2].m_prev].m_next == vi2);
			assert(ts->m_verts[ts->m_verts[vi2].m_next].m_prev == vi2);

			check_loops_valid(ts);

			// Emit triangle.
			if (vi0 != vi1 && vi0 != vi2 && vi1 != vi2) {
				ts->m_results->push_back(ts->m_verts[vi0].m_v.x);
				ts->m_results->push_back(ts->m_verts[vi0].m_v.y);
				ts->m_results->push_back(ts->m_verts[vi1].m_v.x);
				ts->m_results->push_back(ts->m_verts[vi1].m_v.y);
				ts->m_results->push_back(ts->m_verts[vi2].m_v.x);
				ts->m_results->push_back(ts->m_verts[vi2].m_v.y);
			}
			return true;
		}
	}

	return false;
}


// Edge index ideas.  Needs to, given a vert, quickly find all edges
// into and out of that vert.
//
// * use two sorted lists of edges, m_out and m_in.  m_out is sorted
// by m_0, and m_in is sorted by m_1.  Edges for a particular vert are
// together; to find them, just do binary search.
//
// * when updating edge list, we always drop two and add one.  So to
// update indices, replace one edge (fortunately the new edge touches
// a vert we need to replace, so it can stay where it is) and mark the
// other one as invalid (perhaps with a skip count to get to the next
// valid edge?)
//
// * these indices can directly replace our m_active list.

// Problems with the above: it makes what should be O(1) (finding
// edges incident to a vertex) an O(logN) operation.  Plus it's still
// not that lean in terms of memory, because you need two sorted lists
// (one for incoming, one for outgoing).  Also, deleting edges is kind
// of awkward.

// Alternative: use loops (a la FIST), but have special logic to deal
// with coincident vertices.  If a vert is not coincident, then normal
// loop logic works fine.  Most verts are expected to not be
// coincident.  This should be fairly compact, simple and efficient.


void triangulate_plane(tristate* ts)
// Make triangles.
{
	// Ear-clip, allowing for twisted loops.  (Reasoning: don't
	// need to be careful to do non-local analysis when
	// constructing master loop.)

	// Clip all available ears.
	while (find_and_clip_ear(ts)) {
		if (check_debug_dump(ts)) {
			return;
		}
#ifdef DEBUG_MARKUP
		ts->m_debug_edges->resize(0);
#endif // DEBUG_MARKUP
	}
}


namespace constrained_triangulate {
	void compute(
		array<sint16>* results,
		int path_count,
		const array<sint16> paths[],
		int debug_halt_step,
		array<sint16>* debug_edges)
	{
#ifdef PROFILE_TRIANGULATE
		uint64	start_ticks = tu_timer::get_profile_ticks();
#endif // PROFILE_TRIANGULATE
	
		tristate ts;
		init(&ts, results, path_count, paths, debug_halt_step, debug_edges);

#ifdef PROFILE_TRIANGULATE
		uint64	join_ticks = tu_timer::get_profile_ticks();
		fprintf(stderr, "join poly = %1.6f sec\n", tu_timer::profile_ticks_to_seconds(join_ticks - start_ticks));
#endif // PROFILE_TRIANGULATE
		
		triangulate_plane(&ts);

#ifdef PROFILE_TRIANGULATE
		uint64	clip_ticks = tu_timer::get_profile_ticks();
		fprintf(stderr, "clip poly = %1.6f sec\n", tu_timer::profile_ticks_to_seconds(clip_ticks - join_ticks));
		fprintf(stderr, "total for poly = %1.6f sec\n", tu_timer::profile_ticks_to_seconds(clip_ticks - start_ticks));
		fprintf(stderr, "vert count = %d, verts clipped / sec = %f, verts processed / sec = %f\n",
			ts.m_verts.size(),
			ts.m_verts.size() / tu_timer::profile_ticks_to_seconds(clip_ticks - join_ticks),
			ts.m_verts.size() / tu_timer::profile_ticks_to_seconds(clip_ticks - start_ticks)
			);
#endif // PROFILE_TRIANGULATE
	}
}


/* triangulation notes

Nice lecture notes:
http://arachne.ics.uci.edu/~eppstein/junkyard/godfried.toussaint.html

Narkhede & Manocha's description/code of Seidel's alg:
http://www.cs.unc.edu/~dm/CODE/GEM/chapter.html

Some school project notes w/ triangulation overview & diagrams:
http://www.mema.ucl.ac.be/~wu/FSA2716-2002/project.html

Toussaint paper about sleeve-following, including interesting
description & opinion on various other algorithms:
http://citeseer.ist.psu.edu/toussaint91efficient.html

Toussaint outline & links:
http://cgm.cs.mcgill.ca/~godfried/teaching/cg-web.html


http://geometryalgorithms.com/algorithms.htm

History Of Triangulation Algorithms
http://cgm.cs.mcgill.ca/~godfried/teaching/cg-projects/97/Thierry/thierry507webprj/complexity.html

Ear Cutting For Simple Polygons
http://cgm.cs.mcgill.ca/~godfried/teaching/cg-projects/97/Ian//cutting_ears.html

Intersections for a set of 2D segments
http://geometryalgorithms.com/Archive/algorithm_0108/algorithm_0108.htm

Simple Polygon Triangulation
http://cgafaq.info/wiki/Simple_Polygon_Triangulation

KKT O(n log log n) algo
http://portal.acm.org/citation.cfm?id=150693&dl=ACM&coll=portal&CFID=11111111&CFTOKEN=2222222

Poly2Tri implemenation, good notes and looks like good code, sadly the
license is non-commercial only:
http://www.mema.ucl.ac.be/~wu/Poly2Tri/poly2tri.html

FIST
http://www.cosy.sbg.ac.at/~held/projects/triang/triang.html

Nice slides on monotone subdivision & triangulation:
http://www.cs.ucsb.edu/~suri/cs235/Triangulation.pdf

Interesting forum post re monotone subdivision in Amanith:
http://www.amanith.org/forum/viewtopic.php?pid=43

*/
