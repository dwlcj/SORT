/*
    This file is a part of SORT(Simple Open Ray Tracing), an open-source cross
    platform physically based renderer.
 
    Copyright (c) 2011-2019 by Cao Jiayin - All rights reserved.
 
    SORT is a free software written for educational purpose. Anyone can distribute
    or modify it under the the terms of the GNU General Public License Version 3 as
    published by the Free Software Foundation. However, there is NO warranty that
    all components are functional in a perfect manner. Without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.
 
    You should have received a copy of the GNU General Public License along with
    this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.html>.
 */

#include "disney.h"
#include "sampler/sample.h"
#include "core/samplemethod.h"
#include "core/sassert.h"
#include "lambert.h"
#include "microfacet.h"

constexpr static float ior_in = 1.5f;          // hard coded index of refraction below the surface
constexpr static float ior_ex = 1.0f;          // hard coded index of refraction above the surface
constexpr static float eta = ior_ex / ior_in;  // hard coded index of refraction ratio
constexpr static float inv_eta = 1.0f / eta;   // hard coded reciprocal of IOR ratio

float ClearcoatGGX::D(const Vector& h) const {
    // D(h) = ( alpha^2 - 1 ) / ( 2 * PI * ln(alpha) * ( 1 + ( alpha^2 - 1 ) * cos(\theta) ^ 2 )

    // This function should return INV_PI when alphaU equals to 1.0, which is not possible given the implementation of disney BRDF in SORT.
    sAssert(alphaU != 1.0f, MATERIAL);

    const auto cos = CosTheta(h);
    return (alphaU2 - 1) / (PI * log(alphaU2) * (1 + (alphaU2 - 1) * SQR(cos)));
}

Vector ClearcoatGGX::sample_f(const BsdfSample& bs) const {
    // phi = 2 * PI * u
    // theta = acos( sqrt( ( exp( 2 * ln(alpha) * v ) - 1 ) / ( alpha^2 - 1.0f ) ) )
    const auto phi = TWO_PI * bs.u;
    const auto theta = alphaU2 == 1.0f ? acos(sqrt(bs.v)) : acos(sqrt((exp(log(alphaU2) * bs.v) - 1.0f) / (alphaU2 - 1.0f)));
    return SphericalVec(theta, phi);
}

float ClearcoatGGX::G1(const Vector& v) const {
    if (AbsCosTheta(v) == 1.0f)
        return 0.0f;
    const auto tan_theta_sq = TanTheta2(v);
    constexpr auto alpha = 0.25f;
    constexpr auto alpha2 = alpha * alpha;
    return 1.0f / (1.0f + sqrt(1.0f + alpha2 * tan_theta_sq));
}

Spectrum DisneyBRDF::f( const Vector& wo , const Vector& wi ) const {
    const auto aspect = sqrt(sqrt(1.0f - anisotropic * 0.9f));
    const auto diffuseWeight = (1.0f - metallic) * (1.0 - specTrans);

    const auto wh = Normalize(wo + wi);
    const auto HoO = Dot(wo, wh);
    const auto HoO2ByRoughness = SQR(HoO) * roughness;
    
    const auto luminance = basecolor.GetIntensity();
    const auto Ctint = luminance > 0.0f ? basecolor * (1.0f / luminance) : Spectrum(1.0f);

    auto ret = RGBSpectrum(0.0f);

    const auto evaluate_reflection = PointingUp( wo ) && PointingUp( wi );

    if (diffuseWeight > 0.0f) {
        const auto NoO = CosTheta(wo);
        const auto NoI = CosTheta(wi);
        const auto Clampped_NoI = saturate(NoI);
        const auto FO = SchlickWeight(NoO);
        const auto FI = SchlickWeight(NoI);

        if (thinSurface) {
            if( evaluate_reflection ){
                if (flatness < 1.0f) {
                    // Diffuse
                    // Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering, eq (4)
                    const auto disneyDiffuse = basecolor * (INV_PI * (1.0 - FO * 0.5f) * (1.0 - FI * 0.5f));
                    ret += diffuseWeight * (1.0 - flatness) * (1.0f - diffTrans) * disneyDiffuse * Clampped_NoI;
                }
                if (flatness > 0.0f) {
                    // Fake sub-surface scattering
                    // Reflection from Layered Surfaces due to Subsurface Scattering
                    // https://cseweb.ucsd.edu/~ravir/6998/papers/p165-hanrahan.pdf
                    // Based on Hanrahan-Krueger BRDF approximation of isotropic BSSRDF
                    // 1.25 scale is used to (roughly) preserve albedo
                    // Fss90 used to "flatten" retro-reflection based on roughness
                    const auto Fss90 = HoO2ByRoughness;
                    const auto Fss = slerp(1.0f, Fss90, FO) * slerp(1.0f, Fss90, FI);
                    const auto disneyFakeSS = basecolor * (1.25f * (Fss * (1 / (NoO + NoI) - 0.5f) + 0.5f) * INV_PI);
                    ret += diffuseWeight * flatness * (1.0f - diffTrans) * disneyFakeSS * Clampped_NoI;
                }
            }
        } else {
            if (scatterDistance > 0.0f) {
                // Handle sub-surface scattering branch, to be done.
                // There is a following up task to support SSS in SORT, after which this can be easily done.
                // Issue tracking ticket, https://github.com/JerryCao1985/SORT/issues/85
            } else if( evaluate_reflection ){
                // Fall back to the Disney diffuse due to the lack of sub-surface scattering
                const auto disneyDiffuse = basecolor * (INV_PI * (1.0 - FO * 0.5f) * (1.0 - FI * 0.5f));
                ret += diffuseWeight * disneyDiffuse * Clampped_NoI;
            }
        }

        if( evaluate_reflection ){
            // Retro-reflection
            // Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering, eq (4)
            const auto Rr = 2.0 * HoO2ByRoughness;
            const auto frr = basecolor * (INV_PI * Rr * (FO + FI + FO * FI * (Rr - 1.0f)));
            ret += diffuseWeight * frr * Clampped_NoI;

            // This is not totally physically correct. However, dielectric model presented by Walter et al. loses energy due to lack
            // of microfacet inter-reflection/refraction and the sheen component can approximately compensate for it.
            if (sheen > 0.0f) {
                const auto Csheen = slerp(Spectrum(1.0f), Ctint, sheenTint);
                const auto FH = SchlickWeight(HoO);
                const auto Fsheen = FH * sheen * Csheen;
                ret += diffuseWeight * Fsheen * Clampped_NoI;
            }
        }
    }

    // Specular reflection term in Disney BRDF
    const GGX ggx(roughness / aspect, roughness * aspect);
    const auto Cspec0 = slerp(specular * SchlickR0FromEta( ior_ex / ior_in ) * slerp(Spectrum(1.0f), Ctint, specularTint), basecolor, metallic);
    if (!Cspec0.IsBlack() && evaluate_reflection) {
        const FresnelDisney fresnel(Cspec0, ior_ex, ior_in, metallic);
        const MicroFacetReflection mf(WHITE_SPECTRUM, &fresnel, &ggx, FULL_WEIGHT, nn);
        ret += mf.f(wo, wi);
    }

    // Another layer of clear coat on top of everything below.
    if (clearcoat > 0.0f && evaluate_reflection) {
        const ClearcoatGGX cggx(sqrt(slerp(0.1f, 0.001f, clearcoatGloss)));
        const FresnelSchlick<float> fresnel(0.04f);
        const MicroFacetReflection mf_clearcoat(WHITE_SPECTRUM, &fresnel, &cggx, FULL_WEIGHT, nn);
        ret += clearcoat * mf_clearcoat.f(wo, wi);
    }

    // Specular transmission
    if (specTrans > 0.0f) {
        if (thinSurface) {
            // Scale roughness based on IOR (Burley 2015, Figure 15).
            const auto rscaled = (0.65f * inv_eta - 0.35f) * roughness;
            const auto ru = SQR(rscaled) / aspect;
            const auto rv = SQR(rscaled) * aspect;
            const GGX scaledDist(ru, rv);

            MicroFacetRefraction mr(basecolor.Sqrt(), &scaledDist, ior_ex, ior_in, FULL_WEIGHT, nn);
            ret += specTrans * (1.0f - metallic) * mr.f(wo, wi);
        } else {
            // Microfacet Models for Refraction through Rough Surfaces
            // https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
            MicroFacetRefraction mr(basecolor, &ggx, ior_ex, ior_in, FULL_WEIGHT, nn);
            ret += specTrans * (1.0f - metallic) * mr.f(wo, wi);
        }
    }

    // Diffuse transmission
    if ( thinSurface && diffTrans > 0.0f && diffuseWeight > 0.0f ) {
        LambertTransmission lambert_transmission(basecolor , 1.0f, nn);
        ret += diffTrans * diffuseWeight * lambert_transmission.f(wo, wi) ;
    }

    return ret;
}

#define NEW_SAMPLING_METHOD

Spectrum DisneyBRDF::sample_f(const Vector& wo, Vector& wi, const BsdfSample& bs, float* pPdf) const {
#if defined(NEW_SAMPLING_METHOD)
    const auto aspect = sqrt(sqrt(1.0f - anisotropic * 0.9f));
    const auto luminance = basecolor.GetIntensity();
    const auto Ctint = luminance > 0.0f ? basecolor * (1.0f / luminance) : Spectrum(1.0f);
    const auto min_specular_amount = SchlickR0FromEta(ior_ex / ior_in);
    const auto Cspec0 = slerp(specular * min_specular_amount * slerp(Spectrum(1.0f), Ctint, specularTint), basecolor, metallic);

    const auto base_color_intensity = basecolor.GetIntensity();
    const auto clearcoat_weight = clearcoat * 0.04;
    const auto specular_reflection_weight = Cspec0.GetIntensity() * metallic;
    const auto specular_transmission_weight = base_color_intensity * (1.0f - metallic) * specTrans;
    const auto diffuse_reflection_weight = base_color_intensity * (1.0f - metallic) * (1.0f - specTrans) * (thinSurface ? (1.0f - diffTrans) : 1.0f);
    const auto diffuse_transmission_weight = thinSurface ? base_color_intensity * (1.0f - metallic) * (1.0f - specTrans) * diffTrans : 0.0f;

    const auto total_weight = clearcoat_weight + specular_reflection_weight + specular_transmission_weight + diffuse_reflection_weight + diffuse_transmission_weight;
    sAssert(total_weight > 0.0f, MATERIAL);
    if (total_weight <= 0.0f) {
        if (pPdf)
            *pPdf = 0.0f;
        return 0.0f;
    }

    const auto inv_total_weight = 1.0f / total_weight;
    const auto cc_w = clearcoat_weight * inv_total_weight;
    const auto sr_w = specular_reflection_weight * inv_total_weight + cc_w;
    const auto st_w = specular_transmission_weight * inv_total_weight + sr_w;
    const auto dr_w = diffuse_reflection_weight * inv_total_weight + st_w;
    //const auto dt_w = diffuse_transmission_weight * inv_total_weight + dr_w;

    const GGX ggx(roughness / aspect, roughness * aspect);
    const auto r = sort_canonical();
    if (r <= cc_w) {
        BsdfSample sample(true);
        Vector wh;
        const ClearcoatGGX cggx(sqrt(slerp(0.1f, 0.001f, clearcoatGloss)));
        wh = cggx.sample_f(sample);
        wi = 2 * Dot(wo, wh) * wh - wo;
    }else if (r <= sr_w) {
        BsdfSample sample(true);
        Vector wh;
        wh = ggx.sample_f(sample);
        wi = 2 * Dot(wo, wh) * wh - wo;
    }else if (r <= st_w) {
        if (thinSurface) {
            // Scale roughness based on IOR (Burley 2015, Figure 15).
            const auto rscaled = (0.65f * inv_eta - 0.35f) * roughness;
            const auto ru = SQR(rscaled) / aspect;
            const auto rv = SQR(rscaled) * aspect;
            const GGX scaledDist(ru, rv);
            MicroFacetRefraction mr(WHITE_SPECTRUM, &scaledDist, ior_ex, ior_in, FULL_WEIGHT, nn);
            mr.sample_f(wo, wi, bs, pPdf);
        }
        else {
            // Sampling the transmission BTDF
            MicroFacetRefraction mr(WHITE_SPECTRUM, &ggx, ior_ex, ior_in, FULL_WEIGHT, nn);
            mr.sample_f(wo, wi, bs, pPdf);
        }
    }else if (r <= dr_w) {
        wi = CosSampleHemisphere(sort_canonical(), sort_canonical());
    }else // if( r <= dt_w )
    {
        LambertTransmission lambert_transmission(basecolor, diffTrans, nn);
        lambert_transmission.sample_f(wo, wi, bs, pPdf);
    }

    if (pPdf) *pPdf = pdf(wo, wi);

#else
    const auto aspect = sqrt(sqrt(1.0f - anisotropic * 0.9f));
    const auto luminance = basecolor.GetIntensity();
    const auto Ctint = luminance > 0.0f ? basecolor * (1.0f / luminance) : Spectrum(1.0f);
    const auto min_specular_amount = SchlickR0FromEta( ior_ex / ior_in );
    const auto Cspec0 = slerp(specular * min_specular_amount * slerp(Spectrum(1.0f), Ctint, specularTint), basecolor, metallic);
    const auto clearcoat_intensity = clearcoat;
    const auto specular_intensity = Cspec0.GetIntensity();
    const auto total_specular_reflection = clearcoat_intensity + specular_intensity;

    const GGX ggx(roughness / aspect, roughness * aspect);
    const auto sample_nonspecular_reflection_ratio = total_specular_reflection == 0.0f ? 1.0f : ( 1.0f - metallic ) * ( 1.0f - specular * min_specular_amount ) * basecolor.GetIntensity();
    if( bs.u < sample_nonspecular_reflection_ratio || sample_nonspecular_reflection_ratio == 1.0f ){
        const auto r = sort_canonical();
        if (r < specTrans || specTrans == 1.0f) {
            if (thinSurface) {
                // Scale roughness based on IOR (Burley 2015, Figure 15).
                const auto rscaled = (0.65f * inv_eta - 0.35f) * roughness;
                const auto ru = SQR(rscaled) / aspect;
                const auto rv = SQR(rscaled) * aspect;
                const GGX scaledDist(ru, rv);

                const auto T = specTrans * basecolor.Sqrt();
                MicroFacetRefraction mr(T, &scaledDist, ior_ex, ior_in, FULL_WEIGHT, nn);
                mr.sample_f(wo, wi, bs, pPdf);
            } else {
                // Sampling the transmission BTDF
                const auto T = specTrans * basecolor.Sqrt();
                MicroFacetRefraction mr(T, &ggx, ior_ex, ior_in, FULL_WEIGHT, nn);
                mr.sample_f(wo, wi, bs, pPdf);
            }
        } else {
            const auto r = sort_canonical();

            if ( thinSurface && ( r < diffTrans || diffTrans == 1.0f ) ) {
                LambertTransmission lambert_transmission(basecolor, diffTrans, nn);
                lambert_transmission.sample_f(wo, wi, bs, pPdf);
            } else {
                // Sampling the reflection BRDF
                wi = CosSampleHemisphere( sort_canonical() , sort_canonical() );
            }
        }
    }else{
        // Sampling the metallic BRDF, including clear-coat if needed
        const auto r = sort_canonical();
        BsdfSample sample(true);
        Vector wh;

        const auto clearcoat_ratio = clearcoat_intensity / total_specular_reflection;
        if (r < clearcoat_ratio || clearcoat_ratio == 1.0f) {
            const ClearcoatGGX cggx(sqrt(slerp(0.1f, 0.001f, clearcoatGloss)));
            wh = cggx.sample_f(sample);
        } else {
            wh = ggx.sample_f(sample);
        }
        wi = 2 * Dot(wo, wh) * wh - wo;
    }
    
    if( pPdf ) *pPdf = pdf( wo , wi );

#endif

    return f( wo , wi );
}

float DisneyBRDF::pdf( const Vector& wo , const Vector& wi ) const {
#if defined(NEW_SAMPLING_METHOD)
    const auto aspect = sqrt(sqrt(1.0f - anisotropic * 0.9f));
    const auto luminance = basecolor.GetIntensity();
    const auto Ctint = luminance > 0.0f ? basecolor * (1.0f / luminance) : Spectrum(1.0f);
    const auto min_specular_amount = SchlickR0FromEta(ior_ex / ior_in);
    const auto Cspec0 = slerp(specular * min_specular_amount * slerp(Spectrum(1.0f), Ctint, specularTint), basecolor, metallic);

    const auto base_color_intensity = basecolor.GetIntensity();
    const auto clearcoat_weight = clearcoat * 0.04;
    const auto specular_reflection_weight = Cspec0.GetIntensity() * metallic;
    const auto specular_transmission_weight = base_color_intensity * (1.0f - metallic) * specTrans;
    const auto diffuse_reflection_weight = base_color_intensity * (1.0f - metallic) * (1.0f - specTrans) * (thinSurface ? (1.0f - diffTrans) : 1.0f);
    const auto diffuse_transmission_weight = thinSurface ? base_color_intensity * (1.0f - metallic) * (1.0f - specTrans) * diffTrans : 0.0f;

    const auto total_weight = clearcoat_weight + specular_reflection_weight + specular_transmission_weight + diffuse_reflection_weight + diffuse_transmission_weight;
    sAssert(total_weight > 0.0f, MATERIAL);

    auto total_pdf = 0.0f;
    auto cc_pdf = 0.0f, sr_pdf = 0.0f, st_pdf = 0.0f, dr_pdf = 0.0f, dt_pdf = 0.0f;
    const auto wh = Normalize(wi + wo);
    const GGX ggx(roughness / aspect, roughness * aspect);
    if (clearcoat_weight > 0.0f) {
        const ClearcoatGGX cggx(sqrt(slerp(0.1f, 0.001f, clearcoatGloss)));
        total_pdf += clearcoat_weight * cggx.Pdf(wh) / (4.0f * AbsDot(wo, wh));
    }
    if (specular_reflection_weight > 0.0f) {
        total_pdf += specular_reflection_weight * ggx.Pdf(wh) / (4.0f * AbsDot(wo, wh));
    }
    if (specular_transmission_weight > 0.0f) {
        if (thinSurface) {
            // Scale roughness based on IOR (Burley 2015, Figure 15).
            const auto rscaled = (0.65f * inv_eta - 0.35f) * roughness;
            const auto ru = SQR(rscaled) / aspect;
            const auto rv = SQR(rscaled) * aspect;
            const GGX scaledDist(ru, rv);

            MicroFacetRefraction mr(WHITE_SPECTRUM, &scaledDist, ior_ex, ior_in, FULL_WEIGHT, nn);
            total_pdf += specular_transmission_weight * mr.pdf(wo, wi);
        }
        else {
            // Sampling the transmission BTDF
            MicroFacetRefraction mr(WHITE_SPECTRUM, &ggx, ior_ex, ior_in, FULL_WEIGHT, nn);
            total_pdf += specular_transmission_weight * mr.pdf(wo, wi);
        }
    }
    if (diffuse_reflection_weight > 0.0f) {
        total_pdf += diffuse_reflection_weight * CosHemispherePdf(wi);
    }
    if (diffuse_transmission_weight > 0.0f) {
        LambertTransmission lambert_transmission(basecolor, diffTrans, nn);
        total_pdf += diffuse_transmission_weight * lambert_transmission.pdf(wo, wi);
    }

    return total_pdf / total_weight;

#else
    const auto aspect = sqrt(sqrt(1.0f - anisotropic * 0.9f));
    const auto luminance = basecolor.GetIntensity();
    const auto Ctint = luminance > 0.0f ? basecolor * (1.0f / luminance) : Spectrum(1.0f);
    const auto min_specular_amount = SchlickR0FromEta( ior_ex / ior_in );
    const auto Cspec0 = slerp(specular * min_specular_amount * slerp(Spectrum(1.0f), Ctint, specularTint), basecolor, metallic);
    const auto clearcoat_intensity = clearcoat;
    const auto specular_intensity = Cspec0.GetIntensity();
    const auto total_specular_reflection = clearcoat_intensity + specular_intensity;

    const GGX ggx(roughness / aspect, roughness * aspect);
    const auto sample_nonspecular_reflection_ratio = (1.0f - metallic) * (1.0f - specular * min_specular_amount) * basecolor.GetIntensity();

    // Sampling specular transmission
    auto pdf_sample_specular_tranmission = 0.0f;
    const auto T = specTrans * basecolor.Sqrt();
    if (thinSurface) {
        // Scale roughness based on IOR (Burley 2015, Figure 15).
        const auto rscaled = (0.65f * inv_eta - 0.35f) * roughness;
        const auto ru = SQR(rscaled) / aspect;
        const auto rv = SQR(rscaled) * aspect;
        const GGX scaledDist(ru, rv);

        MicroFacetRefraction mr(T, &scaledDist, ior_ex, ior_in, FULL_WEIGHT, nn);
        pdf_sample_specular_tranmission = mr.pdf(wo, wi);
    } else {
        // Sampling the transmission BTDF
        MicroFacetRefraction mr(T, &ggx, ior_ex, ior_in, FULL_WEIGHT, nn);
        pdf_sample_specular_tranmission = mr.pdf(wo, wi);
    }

    // Sampling diffuse component
    const auto pdf_sample_diffuse_reflection = CosHemispherePdf(wi);
    LambertTransmission lambert_transmission(basecolor, diffTrans, nn);
    const auto pdf_sample_diffuse_tranmission = lambert_transmission.pdf(wo, wi);
    const auto pdf_sample_diffuse = (thinSurface) ? slerp(pdf_sample_diffuse_reflection, pdf_sample_diffuse_tranmission, diffTrans) : pdf_sample_diffuse_reflection;

    if (total_specular_reflection == 0.0f)
        return slerp(pdf_sample_diffuse, pdf_sample_specular_tranmission, specTrans);

    // Sampling the metallic BRDF, including clear-coat if needed
    const auto wh = Normalize(wi + wo);
    const auto clearcoat_ratio = clearcoat_intensity / total_specular_reflection;
    const ClearcoatGGX cggx(sqrt(slerp(0.1f, 0.001f, clearcoatGloss)));
    const auto pdf_wh_specular_reflection = slerp(ggx.Pdf(wh), cggx.Pdf(wh), clearcoat_ratio);
    const auto same_hemisphere = SameHemiSphere( wi , wo );
    const auto pdf_specular_reflection = same_hemisphere ? pdf_wh_specular_reflection / (4.0f * AbsDot(wo, wh)) : 0.0f;

    return slerp(pdf_specular_reflection, slerp(pdf_sample_diffuse, pdf_sample_specular_tranmission, specTrans), sample_nonspecular_reflection_ratio);
#endif
}
