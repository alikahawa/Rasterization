#pragma once

#include <vector>

#include <cassert>
#include <cstddef>

/* A 3D volume
 *
 * The `Volume` holds a scalar 3D volume, i.e., a 3D volume where each point
 * has a certain density. The size of the volume is given by the
 *   - `width()`
 *   - `height()`
 *   - `depth()'
 * member functions. Important: the volume is not necessarily cubic!
 *
 * Individual elements of the volume can be accessed via the operator(). For
 * example:
 *
 *   Volume myVolume = ...;
 *   float element = myVolume( 1, 3, 8 );
 *
 * `element` contains the density at the coordinate (1,3,8). Note: you must not
 * attempt to read the value of an element outside of the defined volume. So
 *
 *   std::size_t i = ..., j = ..., k = ...;
 *   float elem = myVolume( i, j, k );
 *
 * requires that
 *   - i >= 0 and i < myVolume.width()
 *   - j >= 0 and j < myVolume.height()
 *   - k >= 0 and k < myVolume.depth()
 *
 * The total number of elements in the volume is given by
 * `total_element_count()`, which is equal to `width()*height()*depth()`.
 */
class Volume
{
	public:
		Volume( std::size_t aWidth, std::size_t aHeight, std::size_t aDepth );

	public:
		float& operator() (std::size_t aI, std::size_t aJ, std::size_t aK);
		float const& operator() (std::size_t aI, std::size_t aJ, std::size_t aK) const;

	public:
		std::size_t width() const;
		std::size_t height() const;
		std::size_t depth() const;
		std::size_t total_element_count() const;

		float* data();
		float const* data() const;

		std::size_t to_linear_index( std::size_t, std::size_t, std::size_t ) const;

	private:
		std::vector<float> mData;
		std::size_t mWidth, mHeight, mDepth;
};

/* Use this method to load a Volume from a file. We've included two example
 * volumes in the `data/` subdirectory.
 */
Volume load_mhd_volume( char const* aFileName );


// Implementation:
inline
Volume::Volume( std::size_t aWidth, std::size_t aHeight, std::size_t aDepth )
	: mData( aWidth*aHeight*aDepth )
	, mWidth(aWidth)
	, mHeight(aHeight)
	, mDepth(aDepth)
{}

inline
float& Volume::operator() (std::size_t aI, std::size_t aJ, std::size_t aK)
{
	assert( aI < mWidth && aJ < mHeight && aK < mDepth );
	return mData[to_linear_index(aI,aJ,aK)];
}
inline
float const& Volume::operator() (std::size_t aI, std::size_t aJ, std::size_t aK) const
{
	assert( aI < mWidth && aJ < mHeight && aK < mDepth );
	return mData[to_linear_index(aI,aJ,aK)];
}


inline 
std::size_t Volume::width() const
{
	return mWidth;
}
inline 
std::size_t Volume::height() const
{
	return mHeight;
}
inline
std::size_t Volume::depth() const
{
	return mDepth;
}
inline
std::size_t Volume::total_element_count() const
{
	return mWidth*mHeight*mDepth;
}

inline
float* Volume::data()
{
	return mData.data();
}
inline
float const* Volume::data() const
{
	return mData.data();
}

inline
std::size_t Volume::to_linear_index( std::size_t aI, std::size_t aJ, std::size_t aK ) const
{
	return aK*mWidth*mHeight + aJ*mWidth + aI;
}
