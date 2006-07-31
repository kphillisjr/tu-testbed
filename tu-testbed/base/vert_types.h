// vert_types.h  -- Thatcher Ulrich 2006

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some basic geometric vertex types.


#ifndef VERT_TYPES_H
#define VERT_TYPES_H


// convenience struct; this could use some public vec2 type, but often
// it's nicer for users if the external interface is smaller and more
// c-like, since they probably have their own vec2 that they prefer.
template<class coord_t>
struct vec2
{
	vec2() : x(0), y(0) {}
	vec2(coord_t _x, coord_t _y) : x(_x), y(_y) {}

	bool	operator==(const vec2<coord_t>& v) const
	{
		return x == v.x && y == v.y;
	}
	bool operator!=(const vec2<coord_t>& v) const
	{
		return !operator==(v);
	}
	bool operator<(const vec2<coord_t>& v) const
	// For sorting verts lexicographically.
	{
		if (x < v.x) {
			return true;
		} else if (x == v.x) {
			if (y < v.y) {
				return true;
			}
		}
		return false;
	}
	
//data:
	coord_t	x, y;
};


inline double	determinant_float(const vec2<float>& a, const vec2<float>& b, const vec2<float>& c)
{
	return (double(b.x) - double(a.x)) * (double(c.y) - double(a.y))
		- (double(b.y) - double(a.y)) * (double(c.x) - double(a.x));
}


// This is not really valid!  double has only 52 mantissa bits, but we
// need about 65 for worst-case 32-bit determinant.
inline double	determinant_sint32(const vec2<sint32>& a, const vec2<sint32>& b, const vec2<sint32>& c)
{
	return (double(b.x) - double(a.x)) * (double(c.y) - double(a.y))
		- (double(b.y) - double(a.y)) * (double(c.x) - double(a.x));
}


// double would be OK, and possibly faster on PC-like architectures,
// but I'm curious to see if I can get away w/ no floating point for
// triangulating meshes w/ 16-bit coords.
inline sint64	determinant_sint16(const vec2<sint16>& a, const vec2<sint16>& b, const vec2<sint16>& c)
{
	return (sint64(b.x) - sint64(a.x)) * (sint64(c.y) - sint64(a.y))
		- (sint64(b.y) - sint64(a.y)) * (sint64(c.x) - sint64(a.x));
}


// Return {-1,0,1} if c is {to the right, on, to the left} of the
// directed edge defined by a->b.
template<class coord_t>
inline int	vertex_left_test(const vec2<coord_t>& a, const vec2<coord_t>& b, const vec2<coord_t>& c)
{
	compiler_assert(0);	// must specialize
	return -1;
}


template<>
inline int	vertex_left_test(const vec2<float>& a, const vec2<float>& b, const vec2<float>& c)
// Specialize for vec2<float>
{
	double	det = determinant_float(a, b, c);
	if (det > 0) return 1;
	else if (det < 0) return -1;
	else return 0;
}


template<>
inline int	vertex_left_test(const vec2<sint32>& a, const vec2<sint32>& b, const vec2<sint32>& c)
// Specialize for vec2<sint32>
{
	double	det = determinant_sint32(a, b, c);
	if (det > 0) return 1;
	else if (det < 0) return -1;
	else return 0;
}


template<>
inline int	vertex_left_test(const vec2<sint16>& a, const vec2<sint16>& b, const vec2<sint16>& c)
// Specialize for vec2<sint16>
{
	sint64	det = determinant_sint16(a, b, c);
	if (det > 0) return 1;
	else if (det < 0) return -1;
	else return 0;
}


#endif // VERT_TYPES_H

