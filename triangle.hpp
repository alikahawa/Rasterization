#pragma once

/** A Triangle
 *
 * The `Triangle` structure holds three indices to the vertices that 
 * correspond to the triangle's corners.
 */
struct Triangle
{
	Triangle()
	{
		v[0] = 0;
		v[1] = 0;
		v[2] = 0;
	}

	Triangle( unsigned v0, unsigned v1, unsigned v2 )
	{
		v[0] = v0;
		v[1] = v1;
		v[2] = v2;
	}

	Triangle( Triangle const& ) = default;
	Triangle& operator= (Triangle const&) = default;

	unsigned v[3];
};
