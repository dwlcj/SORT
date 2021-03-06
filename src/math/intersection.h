/*
    This file is a part of SORT(Simple Open Ray Tracing), an open-source cross
    platform physically based renderer.

    Copyright (c) 2011-2019 by Jiayin Cao - All rights reserved.

    SORT is a free software written for educational purpose. Anyone can distribute
    or modify it under the the terms of the GNU General Public License Version 3 as
    published by the Free Software Foundation. However, there is NO warranty that
    all components are functional in a perfect manner. Without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License along with
    this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.html>.
 */

#pragma once

#include <float.h>
#include "math/point.h"
#include "spectrum/spectrum.h"

class Primitive;

/**
 * Intersection records all necessary data when a ray intersection a primitive.
 */ 
class   Intersection{
public:
    // get the emissive
    Spectrum Le( const Vector& wo , float* directPdfA = 0 , float* emissionPdf = 0 ) const;

    // the intersection point
    Point   intersect;
    // the shading normal
    Vector  normal;
    // the geometry normal
    Vector  gnormal;
    // tangent vector
    Vector  tangent;
    // viewing direction in world space, this is usually Wo.
    Vector  view;
    // the uv coordinate
    float   u = 0.0f , v = 0.0f;
    // the delta distance from the original point
    float   t = FLT_MAX;
    // the intersected primitive
    const Primitive*  primitive = nullptr;

    //! @brief  Reset the intersection.
    //!
    //! Intersection has some input and output for primitive intersection test at the same time.
    //! This is a helper function that clears the state ( not all of it, just the relevant ones )
    //! so that the rest of the algorithm can treat it as a 'new' intersection data structure.
    SORT_FORCEINLINE void Reset(){
        t = FLT_MAX;
        primitive = nullptr;
    }
};