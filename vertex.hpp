#pragma once

#include "vec3D.hpp"

/** A Vertex
 *
 * A Vertex in a mesh that contains a position `p` and a normal `n`.
 */
struct Vertex 
{
    Vertex() = default;

	Vertex( Vec3Df const& aPos )
		: p(aPos)
	{}
	Vertex( Vec3Df const& aPos, Vec3Df const& aNorm )
		: p(aPos)
		, n(aNorm)
	{}
	
	Vertex( Vertex const& ) = default;
	Vertex& operator= (Vertex const& ) = default;
	
    Vec3Df p;
    Vec3Df n;
};
