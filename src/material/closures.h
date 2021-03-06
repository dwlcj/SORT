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

#include <OSL/oslexec.h>

class Bsdf;
class Bssrdf;
class Intersection;
class ScatteringEvent;

enum BXDF_CLOSURE_TYPE {
    CLOSURE_LAMBERT = 0,
    CLOSURE_OREN_NAYAR,
    CLOSURE_DISNEY,
    CLOSURE_MICROFACET_REFLECTION,
    CLOSURE_MICROFACET_REFRACTION,
    CLOSURE_ASHIKHMANSHIRLEY,
    CLOSURE_PHONG,
    CLOSURE_LAMBERT_TRANSMITTANCE,
    CLOSURE_MIRROR,
    CLOSURE_DIELETRIC,
    CLOSURE_MICROFACET_REFLECTION_DIELETRIC,
    CLOSURE_HAIR,
    CLOSURE_FOURIER_BDRF,
    CLOSURE_MERL_BRDF,
    CLOSURE_COAT,
    CLOSURE_DOUBLESIDED,
    CLOSURE_DISTRIBUTIONBRDF,
    CLOSURE_FABRIC,
    CLOSURE_SSS,
    CLOSURE_CNT,
};

//! @brief  Register all closures supported by SORT.
//!
//! @param  shadingsys      Shading system of OSL.
void RegisterClosures(OSL::ShadingSystem* shadingsys);

// To be deprecated
void ProcessClosure(Bsdf* bsdf, Bssrdf*& bssrdf, const Intersection& intersection, const OSL::ClosureColor* closure, const OSL::Color3& w , bool replaceBSSRDF );

//! @brief  Process the closure tree result and populate the BSDF.
//!
//! @param  closure         The closure tree in the osl shader.
//! @param  w               The weight of this closure tree, this also counts the weight inderits from the higher level tree nodes.
//! @param  se              The result scattering event.
void ProcessClosure(const OSL::ClosureColor* closure, const OSL::Color3& w , ScatteringEvent& se );