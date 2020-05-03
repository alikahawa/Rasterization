/* This file defines inlinable functions from vec3D.hpp. See the somewhat
 * exhaustive comments in that file for explanations.
 */

#include <cmath>

/* As mentioned, the "inline" specifier is quite important when defining
 * functions in a header/inline file. Omitting them may result in multiple
 * definition errors when the header file is included from multiple translation
 * units!
 */
template< typename tType > inline
tType& Vec3D<tType>::operator[] (std::size_t aIdx)
{
	return p[aIdx];
}
template< typename tType > inline
tType const& Vec3D<tType>::operator[] (std::size_t aIdx) const
{
	return p[aIdx];
}

template< typename tType > inline
Vec3D<tType>& Vec3D<tType>::init( tType aX, tType aY, tType aZ )
{
	p[0] = aX;
	p[1] = aY;
	p[2] = aZ;
	return *this;
}

template< typename tType > inline
tType Vec3D<tType>::getSquaredLength() const
{
	return p[0]*p[0] + p[1]*p[1] + p[2]*p[2];
}
template< typename tType > inline
tType Vec3D<tType>::getLength() const
{
	return std::sqrt(getSquaredLength());
}

template< typename tType > inline
tType Vec3D<tType>::normalize()
{
	auto const length = getLength();
	if( tType(0) == length )
		return tType(0);

	// A common "optimization" is to compute the reciprocal length (1/length)
	// and multiply with it in the following instead of dividing
	// (multiplications are quite a bit cheaper than divisions). However, this
	// only works for floating point types, and depending on the compiler
	// flags, the compiler can do this optimization for us anyway.
	p[0] /= length;
	p[1] /= length;
	p[2] /= length;
	return length;
}

template< typename tType > inline
tType* Vec3D<tType>::pointer()
{
	return p;
}
template< typename tType > inline
tType const* Vec3D<tType>::pointer() const
{
	return p;
}
template< typename tType > inline
tType* Vec3D<tType>::data()
{
	return p;
}
template< typename tType > inline
tType const* Vec3D<tType>::data() const
{
	return p;
}


template< typename tType > inline
void Vec3D<tType>::getTwoOrthogonals( Vec3D& aOutU, Vec3D& aOutV ) const
{
	if( std::abs(p[0]) < std::abs(p[1]) )
	{
		if( std::abs(p[0]) < std::abs(p[2]) )
			aOutU = Vec3D( tType(0), -p[2], p[1] );
		else
			aOutU = Vec3D( -p[1], p[0], tType(0) );
	} 
	else
	{
		if( std::abs(p[1]) < std::abs(p[2]) )
			aOutU = Vec3D( p[2], tType(0), -p[0] );
		else
			aOutU = Vec3D( -p[1], p[0], tType(0) );
	}

	aOutV = crossProduct( *this, aOutU );
}
template< typename tType > inline
Vec3D<tType> Vec3D<tType>::projectOn( Vec3D const& aN, Vec3D const& aP ) const
{
	auto const w = dotProduct( *this - aP, aN );
	return *this - aN*w;
}
template< typename tType > inline
Vec3D<tType> Vec3D<tType>::transformIn( Vec3D const& aPos, Vec3D const& aN, Vec3D const& aU, Vec3D const& aV ) 
{
	auto const q = (*this) - aPos;
	return Vec3D{
		aU[0]*q[0] + aU[1]*q[1] + aU[2]*q[2],
		aV[0]*q[0] + aV[1]*q[1] + aV[2]*q[2],
		aN[0]*q[0] + aN[1]*q[1] + aN[2]*q[2]
	};
}

template< typename tType > inline
void Vec3D<tType>::fromTo( Vec3D const& aP1, Vec3D const& aP2 )
{
	*this = aP2 - aP1;
}
template< typename tType > inline // Not sure what this function is about...
tType Vec3D<tType>::transProduct( Vec3D const& aV ) const
{
	return dotProduct( *this, aV );
}


/* Note: We don't repeat the "static" from the declaration. */
template< typename tType > inline
tType Vec3D<tType>::dotProduct( Vec3D const& aU, Vec3D const& aV )
{
	return aU[0]*aV[0] + aU[1]*aV[1] + aU[2]*aV[2];
}
template< typename tType > inline
tType Vec3D<tType>::squaredDistance( Vec3D const& aU, Vec3D const& aV )
{
	return (aU-aV).getSquaredLength();
}
template< typename tType > inline
tType Vec3D<tType>::distance( Vec3D const& aU, Vec3D const& aV )
{
	return (aU-aV).getLength();
}

template< typename tType > inline
Vec3D<tType> Vec3D<tType>::crossProduct( Vec3D const& aU, Vec3D const& aV )
{
	Vec3D result;
	result[0] = aU[1]*aV[2] - aU[2]*aV[1];
	result[1] = aU[2]*aV[0] - aU[0]*aV[2];
	result[2] = aU[0]*aV[1] - aU[1]*aV[0];
	return result;
}

/* We can use the new trailing return type syntax to avoid a bit of typing.
 * Doesn't make a huge difference here, but for more compliated templates or
 * nested types, this because quite a bit more convenient. 
 */
template< typename tType > inline
auto Vec3D<tType>::segment( Vec3D const& aA, Vec3D const& aB ) -> Vec3D
{
	return Vec3D{
		aB[0] - aA[0],
		aB[1] - aA[1],
		aB[2] - aA[2]
	};
}
template< typename tType > inline
auto Vec3D<tType>::interpolate( Vec3D const& aU, Vec3D const& aV, tType aAlpha ) -> Vec3D
{
	return aU * (tType(1)-aAlpha) + aV * aAlpha;
}
template< typename tType > inline
auto Vec3D<tType>::projectOntoVector( Vec3D const& aU, Vec3D const& aV ) -> Vec3D
{
	return aV * dotProduct(aU,aV);
}

template< typename tType > inline
auto Vec3D<tType>::cartesianToPolar( Vec3D const& v ) -> Vec3D
{
	// Note-2018: this code doesn't properly handle tType not being float. It's
	// just copy-pasta from the old version.
	Vec3D polar;
	polar[0] = v.getLength();
	if (v[2] > 0.0f)
		polar[1] = (tType) atan (sqrt (v[0] * v[0] + v[1] * v[1]) / v[2]);
	else if (v[2] < 0.0f)
		polar[1] = (tType) atan (sqrt (v[0] * v[0] + v[1] * v[1]) / v[2]) + M_PI;
	else
		polar[1] = M_PI * 0.5f;
	if (v[0] > 0.0f)
		polar[2] = (tType) atan (v[1] / v[0]);
	else if (v[0] < 0.0f)
		polar[2] = (tType) atan (v[1] / v[0]) + M_PI;
	else if (v[1] > 0)
		polar[2] = M_PI * 0.5f;
	else
		polar[2] = -M_PI * 0.5;
	return polar;
}
template< typename tType > inline
auto Vec3D<tType>::polarToCartesian( Vec3D const& v ) -> Vec3D
{
	Vec3D cart;
	cart[0] = v[0] * std::sin(v[1]) * std::cos(v[2]);
	cart[1] = v[0] * std::sin(v[1]) * std::sin(v[2]);
	cart[2] = v[0] * std::cos(v[1]);
	return cart;
}


template< typename tType > inline
Vec3D<tType> operator+ (Vec3D<tType> const& aU)
{
	// If you're thinking that this is a no-op for primitive types, you're
	// guessing right. :-)
	return {+aU[0], +aU[1], +aU[2]};
}
template< typename tType > inline
Vec3D<tType> operator- (Vec3D<tType> const& aU)
{
	return {-aU[0], -aU[1], -aU[2]};
}

template< typename tType > inline
Vec3D<tType> operator+ (Vec3D<tType> const& aU, Vec3D<tType> const& aV)
{
	// Alternative style A:
	return Vec3D<tType>{ aU[0]+aV[0], aU[1]+aV[1], aU[2]+aV[2] };
}
template< typename tType >
Vec3D<tType> operator- (Vec3D<tType> const& aU, Vec3D<tType> const& aV)
{
	// Alternative style B:
	return Vec3D<tType>( aU[0]-aV[0], aU[1]-aV[1], aU[2]-aV[2] );
}

template< typename tType, typename tScalar > inline
Vec3D<tType> operator* (Vec3D<tType> const& aU, tScalar aX)
{
	/* Note: it might be a good idea to check that the tScalar type is
	 * convertible to a tType via SFINAE. Now compilation will fail with an
	 * error inside of our function.
	 */
	return {aU[0]*aX, aU[1]*aX, aU[2]*aX};
}
template< typename tType, typename tScalar > inline
Vec3D<tType> operator* (tScalar aX, Vec3D<tType> const& aU)
{
	return {aX*aU[0], aX*aU[1], aX*aU[2]};
}
template< typename tType > inline
Vec3D<tType> operator* (Vec3D<tType> const& aU, Vec3D<tType> const& aV)
{
	return {aU[0]*aV[0], aU[1]*aV[1], aU[2]*aV[2]};
}

template< typename tType, typename tScalar > inline
Vec3D<tType> operator/ (Vec3D<tType> const& aU, tScalar aX)
{
	return {aU[0]/aX, aU[1]/aX, aU[2]/aX};
}
template< typename tType > inline
Vec3D<tType> operator/ (Vec3D<tType> const& aU, Vec3D<tType> const& aV)
{
	return {aU[0]/aV[0], aU[1]/aV[1], aU[2]/aV[2]};
}

template< typename tType > inline
Vec3D<tType>& operator+= (Vec3D<tType>& aU, Vec3D<tType> const& aV)
{
	aU[0] += aV[0];
	aU[1] += aV[1];
	aU[2] += aV[2];
	return aU;
}
template< typename tType > inline
Vec3D<tType>& operator-= (Vec3D<tType>& aU, Vec3D<tType> const& aV)
{
	aU[0] -= aV[0];
	aU[1] -= aV[1];
	aU[2] -= aV[2];
	return aU;
}

template< typename tType > inline
Vec3D<tType>& operator*= (Vec3D<tType>& aU, Vec3D<tType> const& aV)
{
	aU[0] *= aV[0];
	aU[1] *= aV[1];
	aU[2] *= aV[2];
	return aU;
}
template< typename tType, typename tScalar > inline
Vec3D<tType>& operator*= (Vec3D<tType>& aU, tScalar aX)
{
	aU[0] *= aX;
	aU[1] *= aX;
	aU[2] *= aX;
	return aU;
}


template< typename tType > inline
Vec3D<tType>& operator/= (Vec3D<tType>& aU, Vec3D<tType> const& aV)
{
	aU[0] /= aV[0];
	aU[1] /= aV[1];
	aU[2] /= aV[2];
	return aU;

}
template< typename tType, typename tScalar > inline
Vec3D<tType>& operator/= (Vec3D<tType>& aU, tScalar aX)
{
	aU[0] /= aX;
	aU[1] /= aX;
	aU[2] /= aX;
	return aU;
}


template< typename tType > inline
bool operator== (Vec3D<tType> const& aU, Vec3D<tType> const& aV)
{
	return aU[0]==aV[0] && aU[1]==aV[1] && aU[2]==aV[2];
}
template< typename tType > inline
bool operator!= (Vec3D<tType> const& aU, Vec3D<tType> const& aV)
{
	return aU[0]!=aV[0] && aU[1]!=aV[1] && aU[2]!=aV[2];
}


template< typename tType > 
std::ostream& operator<< (std::ostream& aOS, Vec3D<tType> const& aU )
{
	aOS << aU[0] << " " << aU[1] << " " << aU[2];
	return aOS;
}
template< typename tType >
std::istream& operator>> (std::istream& aIS, Vec3D<tType>& aU )
{
	aIS >> aU[0] >> aU[1] >> aU[2];
	return aIS;
}

