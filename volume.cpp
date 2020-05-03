#include "volume.hpp"

#include <cstdio>
#include <cstring>

#include <limits>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdexcept>

#include "miniz.h"

namespace
{
	enum class MHDType
	{
		unknown,
		u8,
		s16
	};
	
	struct MHDInfo
	{
		bool typeIsImage     = false;
		bool dataIsBinary    = false;
		bool dataCompressed  = false;

		int ndims            = -1;
		int x = -1, y = -1, z = -1;

		MHDType elementType  = MHDType::unknown;

		std::string dataFile;
	};


	template< typename tElementType >
	Volume load_data_raw_( FILE* );
	template< typename tElementType >
	Volume load_data_compressed_( FILE* );


	// This is a helper class + function for ON_SCOPE_EXIT().
	template< class tFunc >
	struct OnScopeExit_
	{
		OnScopeExit_( tFunc&& aFunc ) : func(std::move(aFunc)) {}
		~OnScopeExit_() { func(); }
		private: tFunc func;
	};

	template< class tFunc >
	OnScopeExit_<tFunc> make_scope_exit( tFunc&& aFunc ) {
		return OnScopeExit_<tFunc>{ std::forward<tFunc>(aFunc) };
	};

	// ON_SCOPE_EXIT(): helper to safetly clean up stuff.
#	define ON_SCOPE_EXIT(fn) auto CONCAT_(scopeExitGuard,__LINE__) = make_scope_exit(fn)

#	define CONCAT_(a,b) CONCAT0_(a,b)
#	define CONCAT0_(a,b) a##b
}

namespace
{
	template< typename tType > inline
	Volume load_data_raw_( FILE* aFile, MHDInfo const& aInfo )
	{
		std::vector<tType> buffer( aInfo.x*aInfo.y*aInfo.z );
		auto rd = std::fread( buffer.data(), sizeof(tType), buffer.size(), aFile );
		if( rd != buffer.size() )
			throw std::runtime_error( "Could not read volume data" );
	
		Volume ret( aInfo.x, aInfo.y, aInfo.z );

		float minf = float(std::numeric_limits<tType>::max());
		float maxf = float(std::numeric_limits<tType>::min());
		for( auto const elem : buffer )
		{
			float const elemf = float(elem);
			if( elemf > maxf ) maxf = elemf;
			else if( elemf < minf ) minf = elemf;
		}
		
		std::transform( buffer.begin(), buffer.end(), ret.data(), [&] (tType aValue) {
			return (aValue-minf) / (maxf-minf);
		} );

		return ret;
	}

	template< typename tType > inline
	Volume load_data_compressed_( FILE* aFile, MHDInfo const& aInfo )
	{
		// Figure out size of compressed data
		std::fseek( aFile, 0, SEEK_END );
		auto const size = std::ftell( aFile );
		std::fseek( aFile, 0, SEEK_SET );

		// Read compressed data
		std::vector<std::uint8_t> compressed( size );
		auto rd = std::fread( compressed.data(), 1, size, aFile );
		if( long(rd) != size )
			throw std::runtime_error( "Could not read compressed volume data" );
		
		// Decompress using the "miniz" library.
		std::vector<tType> buffer( aInfo.x*aInfo.y*aInfo.z );

		mz_stream mzs{};
		mzs.avail_in = unsigned(compressed.size());
		mzs.next_in = compressed.data();
		mzs.avail_out = unsigned(buffer.size()*sizeof(tType));
		mzs.next_out = reinterpret_cast<std::uint8_t*>(buffer.data());

		auto iret = mz_inflateInit( &mzs );
		if( MZ_OK != iret )
		{
			std::ostringstream oss;
			oss << "miniz: could not initialize decompressor: " << mz_error(iret);
			throw std::runtime_error( oss.str() );
		}

		auto jret = mz_inflate( &mzs, MZ_FINISH );

		mz_inflateEnd( &mzs );
		
		if( MZ_OK != jret )
		{
			if( MZ_STREAM_END == jret && mzs.total_out == buffer.size()*sizeof(tType) )
			{
				/* OK, seems like the some of the compressed streams aren't
				 * properly finalized or something. So, if the error is
				 * MZ_STREAM_END but we've got all our data, we're going to
				 * YOLO it and ignore the error.
				 */
			}
			else
			{
				std::ostringstream oss;
				oss << "miniz: decompression failed: " << mz_error(jret);
				throw std::runtime_error( oss.str() );
			}
		}

		Volume ret( aInfo.x, aInfo.y, aInfo.z );

		float minf = float(std::numeric_limits<tType>::max());
		float maxf = float(std::numeric_limits<tType>::min());
		for( auto const elem : buffer )
		{
			float const elemf = float(elem);
			if( elemf > maxf ) maxf = elemf;
			else if( elemf < minf ) minf = elemf;
		}

		std::transform( buffer.begin(), buffer.end(), ret.data(), [&] (tType aValue) {
			return (aValue-minf) / (maxf-minf);
		} );

		return ret;
	}
}


Volume load_mhd_volume( char const* aFileName ) try
{
	// Load the .mhd, which gives us the meta data for the volume.
	FILE* mhd = std::fopen( aFileName, "rb" );
	if( !mhd )
		throw std::runtime_error( "Could not open source file" );

	ON_SCOPE_EXIT( [&] { std::fclose( mhd ); } );

	MHDInfo info;

	int line = 1;
	char name[64] = "";
	char value[64] = "";

	while( 2 == std::fscanf( mhd, "%63s = %63[^\n\r]", name, value ) )
	{
		if( 0 == std::strcmp( "ObjectType", name ) )
			info.typeIsImage = (0 == std::strcmp( "Image", value ));
		else if( 0 == std::strcmp( "BinaryData", name ) )
			info.dataIsBinary = (0 == std::strcmp( "True", value));
		else if( 0 == std::strcmp( "CompressedData", name ) )
			info.dataCompressed = (0 == std::strcmp( "True", value));
		else if( 0 == std::strcmp( "NDims", name ) )
		{
			char dummy;
			if( 1 != std::sscanf( value, "%d%c", &info.ndims, &dummy ) )
				throw std::runtime_error( "MHD: NDims should be a single integer" );
		}
		else if( 0 == std::strcmp( "DimSize", name ) )
		{
			char dummy;
			if( 3 != std::sscanf( value, "%d %d %d%c", &info.x, &info.y, &info.z, &dummy ) )
				throw std::runtime_error( "MHD: DimSize should be three integers" );
		}
		else if( 0 == std::strcmp( "ElementType", name ) )
		{
			if( 0 == std::strcmp( "MET_UCHAR", value ) )
				info.elementType = MHDType::u8;
			else if( 0 == std::strcmp( "MET_SHORT", value ) )
				info.elementType = MHDType::s16;
			else
			{
				std::ostringstream oss;
				oss << "MHD: ElementType '" << value << "' unknown";
				throw std::runtime_error( oss.str() );
			}
		}
		else if( 0 == std::strcmp( "ElementDataFile", name ) )
		{
			info.dataFile = value;
		}
		else
		{
#			if 0
			std::fprintf( stderr, "Line %d: unused '%s' = '%s'\n", line, name, value );
#			endif
		}
		
		++line;
	}

	if( !std::feof(mhd) )
	{
		std::fprintf( stderr, "MHD (%s): ignored garbage on line %d\n", aFileName, line );
	}

	// Verify that this makes sense
	if( !info.typeIsImage )
		throw std::runtime_error( "Only support ObjectType = Image." );

	if( !info.dataIsBinary )
		throw std::runtime_error( "Only support binary data (BinaryData = True)" );

	if( MHDType::unknown == info.elementType )
		throw std::runtime_error( "Element type of binary data is not recognized" );

	/* Note: this is unlikely to trigger, since we explicitly try to parse
	 * DimSize with three integers.
	 */
	if( 3 != info.ndims )
		throw std::runtime_error( "Only support 3D volumes" );

	if( info.x <= 0 || info.y <= 0 || info.z <= 0 )
		throw std::runtime_error( "Volume size is invalid (DimSize should be three positive integers" );

	// Try to read the volume data
	std::ostringstream oss;
	
	if( char const* lastSlash = std::strrchr( aFileName, '/' ) )
		oss << std::string( aFileName, lastSlash ) << "/";

	oss << info.dataFile; 

	FILE* data = std::fopen( oss.str().c_str(), "rb" );
	if( !data )
	{
		std::ostringstream emsg;
		emsg << "Could not open volume data '" << oss.str() << "' for reading";
		throw std::runtime_error( emsg.str() );
	}

	ON_SCOPE_EXIT( [&] { std::fclose( data ); } );

	if( !info.dataCompressed )
	{
		switch( info.elementType )
		{
			case MHDType::u8: return load_data_raw_<std::uint8_t>( data, info );
			case MHDType::s16: return load_data_raw_<std::int16_t>( data, info );
			case MHDType::unknown: assert(!"Unkown element type"); break;
		}
	}
	else
	{
		switch( info.elementType )
		{
			case MHDType::u8: return load_data_compressed_<std::uint8_t>( data, info );
			case MHDType::s16: return load_data_compressed_<std::int16_t>( data, info );
			case MHDType::unknown: assert(!"Unkown element type"); break;
		}
	}

	// Should be unreachable.
	return Volume(0,0,0);
}
catch( std::exception const& eError )
{
	std::fprintf( stderr, "Error while loading MHD file \"%s\":\n", aFileName ),
	std::fprintf( stderr, "  - %s\n", eError.what() );
	return Volume( 0, 0, 0 );
}
