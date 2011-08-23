/*
   FileName:      square.h

   Created Time:  2011-08-21 11:36:25

   Auther:        Cao Jiayin

   Email:         soraytrace@hotmail.com

   Location:      China, Shanghai

   Description:   SORT is short for Simple Open-source Ray Tracing. Anyone could checkout the source code from
                'sourceforge', https://soraytrace.svn.sourceforge.net/svnroot/soraytrace. And anyone is free to
                modify or publish the source code. It's cross platform. You could compile the source code in 
                linux and windows , g++ or visual studio 2008 is required.
*/

#ifndef	SORT_SQUARE
#define	SORT_SQUARE

#include "shape.h"
#include "utility/creator.h"

/////////////////////////////////////////////////////////////////////////////////
//	definition of the square
class	Square : public Shape
{
// public method
public:
	DEFINE_CREATOR(Square);

	// default constructor
	Square(){}
	// destruction
	~Square(){}
	
	// sample a point on shape
	// para 'ls': the light sample
	// para 'p'	: the target point for the light to lit.
	// para 'wi': the sampled vector ( output )
	// para 'pdf': the pdf of the light sample ( output )
	// result   : a sampled point from the light source
	virtual Point sample_l( const LightSample& ls , const Point& p , Vector& wi , float* pdf ) const;

	// the surface area of the shape
	virtual float SurfaceArea() const;

	// get intersection between the light surface and the ray
	virtual bool GetIntersect( const Ray& ray , Intersection* intersect ) const;

};

#endif