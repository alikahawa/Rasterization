#pragma once

/* A simple 3D Vector template. 
 *
 * We use it to demonstrate a few common C+++ implementation techniques, some
 * of which are briefly elaborated on in this header.
 *
 * This is the 2018 overhaul of the orginal Vec3D template. It provides the
 * same interface as the original implementation, so it should be possible to
 * use as a drop-in replacement. This also means that there are some design
 * decisions that are perhaps ... questionable.
 *
 * Overall, the implementation shows the following common C++ features
 *
 *  - Templates / templating
 *    The Vec3D template can be instantiated with different underlying types
 *    (e.g., int, double, float etc.), which gives us 3D vector types of all
 *    these types with a single implementation. There is no run-time overhead
 *    to this; the type is fixed at compile-time and the compiler will apply
 *    the same optimizations as it would for a hand-written class specific to
 *    the selected type.
 *
 *  - Inlining
 *    Most of the Vec3D member methods and assocated functions are very short
 *    helpers. By making these inline-able, there is again no run-time overhead
 *    to the use of these helper methods: `dotProduct(a,b)` will (in an
 *    optimized build) generate the same code as typing out the dot-product
 *    `a.x*b.x+a.y*b.y+a.z*b.z` - indeed, the name "inlining" comes from the
 *    process of lifting the code out of the function definition and "pasting"
 *    it into the call-site.
 *
 *    Inlining requires the compiler to see the function definitions, not just
 *    the function declarations, so we cannot move the code into a separate
 *    translation unit (= ".cpp" file, roughly). There are several ways to 
 *    achieve this, and the code below shows+mentions a few.
 *
 * The above two techniques are the foundation for building "zero overhead
 * abstractions", which is a core idea in C++: we can provide nice interfaces
 * to the user without incurring any run-time overhead/cost for doing so.
 *
 * As mentioned above, this version is mainly for illustration. Should you need
 * a more competent and complete library, there are several well-tested and
 * much more extensive options, including:
 *
 *  - GLM: https://glm.g-truc.net/0.9.9/index.html
 * 
 * If you set out on writing your own, there are a few worthwhile resources.
 * In particular,
 *
 *  - http://www.reedbeta.com/blog/on-vector-math-libraries/
 *
 * discusses at some length a few common ideas and their consequences (read
 * especially the section on SIMD!). It's a bit dated now, and the more modern
 * C++ standards change some up a bit.
 *
 * Generally, writing your own library is a great exercise, but for production
 * use, you should stick to something well-tested and complete.
 */ 

#include <cstddef>
#include <iosfwd>

template< typename tType = float > 
class Vec3D
{
	/* Note: Currently, Vec3D doesn't use "constexpr", despite it being
	 * applicable to most methods (with C++14 and newer). The goal is to remain
	 * C++11 compatible for now.
	 */
	
	public:
		using element_type = tType;
	
	public:
		/* We can both declare+define functions immediately:
		 */
		Vec3D()
		{
			p[0] = tType(); // Value-initialization (=tType()) will set 
			p[1] = tType(); //   primitive types like float, int, etc to zero.
			p[2] = tType();
		}

		/* Or we can just declare a function and define it later. Here, we can
		 * use the "inline" specifier to indicate that this function is 
		 * inlineable(*) and that we will provide the definition in some place
		 * visible to the compiler.
		 *
		 * This constructor is defined just after the end of the Vec3D class.
		 *
		 * In declarations, we do not need to name the arguments.
		 *
		 * (*) The "inline" specifier is a bit special. Although it hints at
		 * the function being inline-able, the compiler will make the final
		 * decision. In practice, it changes linkage allowing multiple
		 * definitions of a single function to occur in different translation
		 * units. Details can be found in 
		 * https://en.cppreference.com/w/cpp/language/inline
		 */
		inline Vec3D( tType, tType, tType );

		/* We can, in the declaration, omit the "inline" specifier and only
		 * decide this when we define the method (constructor).
		 *
		 * The "explicit" keyword indicates that this constructor should not
		 * be called automatically to convert from a tType (const) pointer to
		 * a Vec3D instance. Without it, the following code would be legal:
		 *  
		 *  float a = ...;
		 *  Vec3D<float> v = &a; // Error with explicit, crash at runtime without 
		 */
		explicit Vec3D( tType const* );
		
		/* C++ provides default copy-constructors and copy-assignment
		 * operators. We can explicitly mention that these exist by defaulting
		 * them.
		 *
		 * If we wanted to make the Vec3D class non-copyable, we could delete
		 * them with "= delete;" instead.
		 *
		 * Either way, we do not have to provide a definition of these two
		 * operations, as the C++ compiler will generate one for us. Not all
		 * functions may be defaulted - only a few special ones that have a
		 * standard behaviour.
		 */
		Vec3D( Vec3D const& ) = default;
		Vec3D& operator= (Vec3D const&) = default;

	public:
		/* The following overloads allow us to write (for example)
		 *
		 *  Vec3D v;
		 *  v[0] = 1.f; 
		 *
		 * instead of the more awkward
		 *
		 *  Vec3D v;
		 *  v.p[0] = 1.f;
		 *
		 * The second version is used when the Vec3D instance is const, e.g.:
		 *
		 *  Vec3D v; // v is not const
		 *  Vec3D const u = v; // u is const and may not be modified
		 *
		 *  u[0] = 1.f; // Error: cannot modify a const value
		 *
		 * It differs from the normal version in that it returns a const
		 * reference, i.e., a reference that is read-only in the given context.
		 */
		tType& operator[] (std::size_t);
		tType const& operator[] (std::size_t) const;

	public:
		Vec3D& init( tType, tType, tType );

		tType getSquaredLength() const;
		tType getLength() const;

		tType normalize();

		tType* pointer(); // old names
		tType const* pointer() const;

		tType* data(); // mimic C++ standard names
		tType const* data() const;


		void getTwoOrthogonals( Vec3D&, Vec3D& ) const;
		Vec3D projectOn( Vec3D const&, Vec3D const& ) const;
		Vec3D transformIn( Vec3D const&, Vec3D const&, Vec3D const&, Vec3D const& );

		void fromTo( Vec3D const&, Vec3D const& );
		tType transProduct( Vec3D const& ) const;

	public:
		static tType dotProduct( Vec3D const&, Vec3D const& );
		static tType squaredDistance( Vec3D const&, Vec3D const& );
		static tType distance( Vec3D const&, Vec3D const& );

		static Vec3D crossProduct( Vec3D const&, Vec3D const& );

		static Vec3D segment( Vec3D const&, Vec3D const& );
		static Vec3D interpolate( Vec3D const&, Vec3D const&, tType );
		static Vec3D projectOntoVector( Vec3D const&, Vec3D const& );

		static Vec3D cartesianToPolar( Vec3D const& );
		static Vec3D polarToCartesian( Vec3D const& );

	public:
		tType p[3];
};

/* Define the constructor declared inside the class.
 *
 * The "inline" specifier is necessary here, otherwise we will end up with
 * multiple-definition errors if this file is included and the Vec3D template
 * is instantiated in multiple translation units.
 */
template< typename tType > inline
Vec3D<tType>::Vec3D( tType aX, tType aY, tType aZ )
{
	p[0] = aX;
	p[1] = aY;
	p[2] = aZ;
}

/* Note that we don't repeat the "explicit" keyword here.
 */
template< typename tType > inline
Vec3D<tType>::Vec3D( tType const* aPtr )
{
	p[0] = *aPtr++;
	p[1] = *aPtr++;
	p[2] = *aPtr++;
}

/* It's good practice to keep header files free from implementation as much as
 * possible - this makes it easy to get a quick overview of the functionality
 * by glancing over the header files.
 *
 * Because of this, the remaining methods are defined in the associated .inl
 * ("inline") file, vec3D.inl. This file is #included at the end of this
 * header, making its contents automatically visible to the compiler whenever
 * the vec3D.hpp header is included. (Reminder: never #include a .cpp file!)
 *
 * "inline" files are not as standardized as source and header files but are
 * relatively common. Occasionally, projects opt to just use other header files
 * (.hpp, .hxx, etc) in e.g. a "detail/" directory as a substitute.
 */


/* The original code prefers to define functions as static members of the Vec3D
 * class. This is not necessary, and modern style typically prefers free
 * functions over static members.
 *
 * Free functions have the advantage that they participate in argument
 * dependent lookups (ADL) when namespaces are used. Additionally, free
 * functions can be added without changing the class defition and are limited
 * by private/protected/public sections. (Neither is really used in this
 * example.)
 */
template< typename tType > inline
void swap( Vec3D<tType>& aX, Vec3D<tType>& aY )
{
	/* Note: that std::swap() would be fine for our Vec3D class too, since
	 * there is nothing fancy to do other than straight up copies.
	 */
	auto tmp = aX;
	aX = aY;
	aY = tmp;
}

/* Again, we can declare free functions and provide their definitions elsewhere
 * (e.g., in the .inl inline file).
 *
 * The followin free functions define various aritmethic operators, so that we
 * can type something like
 *
 *   Vec3D<float> u, v;
 *   auto x = 3.f * (u-v);
 */
// Unary operators
template< typename tType >
Vec3D<tType> operator+ (Vec3D<tType> const&);
template< typename tType >
Vec3D<tType> operator- (Vec3D<tType> const&);

// Binary operators
template< typename tType >
Vec3D<tType> operator+ (Vec3D<tType> const&, Vec3D<tType> const&);
template< typename tType >
Vec3D<tType> operator- (Vec3D<tType> const&, Vec3D<tType> const&);

template< typename tType, typename tScalar >
Vec3D<tType> operator* (Vec3D<tType> const&, tScalar);
template< typename tType, typename tScalar >
Vec3D<tType> operator* (tScalar, Vec3D<tType> const&);
template< typename tType >
Vec3D<tType> operator* (Vec3D<tType> const&, Vec3D<tType> const&);

template< typename tType, typename tScalar >
Vec3D<tType> operator/ (Vec3D<tType> const&, tScalar);
template< typename tType >
Vec3D<tType> operator/ (Vec3D<tType> const&, Vec3D<tType> const&);

// Binary self-assignment
template< typename tType >
Vec3D<tType>& operator+= (Vec3D<tType>&, Vec3D<tType> const&);
template< typename tType >
Vec3D<tType>& operator-= (Vec3D<tType>&, Vec3D<tType> const&);

template< typename tType >
Vec3D<tType>& operator*= (Vec3D<tType>&, Vec3D<tType> const&);
template< typename tType, typename tScalar >
Vec3D<tType>& operator*= (Vec3D<tType>&, tScalar);


template< typename tType >
Vec3D<tType>& operator/= (Vec3D<tType>&, Vec3D<tType> const&);
template< typename tType, typename tScalar >
Vec3D<tType>& operator/= (Vec3D<tType>&, tScalar);

// Comparison
template< typename tType >
bool operator== (Vec3D<tType> const&, Vec3D<tType> const&);
template< typename tType >
bool operator!= (Vec3D<tType> const&, Vec3D<tType> const&);

// Original code defines operators < and <=, but this is fishy --- vectors are
// not strictly ordered.

/* The following two methods enable printing (and reading) of Vec3D instances
 * using the standard C++ streams. E.g.;
 *
 *   Vec3D<float> v = ...;
 *   std::cout << v << "\n";
 *
 * Note: this header only includes <iosfwd>, a header that declares various C++
 * stream objects, but doesn't define them. The above example requires that
 * <iostream> is included as well in your .cpp file.
 */
template< typename tType > 
std::ostream& operator<< (std::ostream&, Vec3D<tType> const& );
template< typename tType >
std::istream& operator>> (std::istream&, Vec3D<tType>& );


/* Finally, we can provide a few default instances of the Vec3D template:
 */
using Vec3Df = Vec3D<float>;
using Vec3Dd = Vec3D<double>;
using Vec3Di = Vec3D<int>;

/* Note: the above use the "new" C++11 style. The old (but still common) style
 * looks like:
 *
 *  typedef Vec3d<float> Vec3Df;
 *  ...
 */

#include "vec3D.inl"
