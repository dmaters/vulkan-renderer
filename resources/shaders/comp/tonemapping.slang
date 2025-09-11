// Tonemapping shader 
// The implementation is taken from the 2025 presentation at Siggraph of the GT7 tonemapping workflow
// It is stripped of the HDR support since for now it's not supported in the pipeline
// https://blog.selfshadow.com/publications/s2025-shading-course/pdi/s2025_pbs_pdi_slides.pdf
// https://blog.selfshadow.com/publications/s2025-shading-course/pdi/supplemental/gt7_tone_mapping.cpp


#define WHITE_LEVEL 250.0f //White level for SDR (250 mip)
#define LUMINANCE_SCALE_FACTOR 100.0f // cd/m^2 <-> 1.0f

#define BLEND_RATIO 0.6f //Blend ratio between twisted and untwisted rgb values


float frameBufferValueToPhysicalValue(float fbValue)
{
    return fbValue * LUMINANCE_SCALE_FACTOR;
}

float physicalValueToFrameBufferValue(float physical)
{
    return physical / LUMINANCE_SCALE_FACTOR;
}

float chromaCurve(float x, float a, float b)
{
    return 1.0f - smoothstep(a, b, x);
}

// -----------------------------------------------------------------------------
// EOTF / inverse-EOTF for ST-2084 (PQ).
// Note: Introduce exponentScaleFactor to allow scaling of the exponent in the EOTF for Jzazbz.
// -----------------------------------------------------------------------------
float eotfSt2084(float n, float exponentScaleFactor = 1.0f)
{
    n = clamp(n,0,1);


    // Base functions from SMPTE ST 2084:2014
    // Converts from normalized PQ (0-1) to absolute luminance in cd/m^2 (linear light)
    // Assumes float input; does not handle integer encoding (Annex)
    // Assumes full-range signal (0-1)
    const float m1  = 0.1593017578125f;                // (2610 / 4096) / 4
    const float m2  = 78.84375f * exponentScaleFactor; // (2523 / 4096) * 128
    const float c1  = 0.8359375f;                      // 3424 / 4096
    const float c2  = 18.8515625f;                     // (2413 / 4096) * 32
    const float c3  = 18.6875f;                        // (2392 / 4096) * 32
    const float pqC = 10000.0f;                        // Maximum luminance supported by PQ (cd/m^2)

    // Does not handle signal range from 2084 - assumes full range (0-1)
    float np = pow(n, 1.0f / m2);
    float l  = np - c1;
    

    l = max(l, 0.0f);


    l = l / (c2 - c3 * np);
    l = pow(l, 1.0f / m1);

    // Convert absolute luminance (cd/m^2) into the frame-buffer linear scale.
    return physicalValueToFrameBufferValue(l * pqC);
}

float inverseEotfSt2084(float v, float exponentScaleFactor = 1.0f)
{
    const float m1  = 0.1593017578125f;
    const float m2  = 78.84375f * exponentScaleFactor;
    const float c1  = 0.8359375f;
    const float c2  = 18.8515625f;
    const float c3  = 18.6875f;
    const float pqC = 10000.0f;

    // Convert the frame-buffer linear scale into absolute luminance (cd/m^2).
    float physical = frameBufferValueToPhysicalValue(v);
    float y        = physical / pqC; // Normalize for the ST-2084 curve

    float ym = pow(y, m1);
    return exp2(m2 * (log2(c1 + c2 * ym) - log2(1.0f + c3 * ym)));
}


// -----------------------------------------------------------------------------
// Jzazbz conversion.
// Reference:
// Muhammad Safdar, Guihua Cui, Youn Jin Kim, and Ming Ronnier Luo,
// "Perceptually uniform color space for image signals including high dynamic
// range and wide gamut," Opt. Express 25, 15131-15151 (2017)
// Note: Coefficients adjusted for linear Rec.2020
// -----------------------------------------------------------------------------
#define JZAZBZ_EXPONENT_SCALE_FACTOR 1.7f // Scale factor for exponent

float3 rgbToUcs(float3 rgb) // Input: linear Rec.2020
{
    float l = rgb[0] * 0.530004f + rgb[1] * 0.355704f + rgb[2] * 0.086090f;
    float m = rgb[0] * 0.289388f + rgb[1] * 0.525395f + rgb[2] * 0.157481f;
    float s = rgb[0] * 0.091098f + rgb[1] * 0.147588f + rgb[2] * 0.734234f;

    float lPQ = inverseEotfSt2084(l, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float mPQ = inverseEotfSt2084(m, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float sPQ = inverseEotfSt2084(s, JZAZBZ_EXPONENT_SCALE_FACTOR);

    float iz = 0.5f * lPQ + 0.5f * mPQ;
    float3 jab;
    jab.r= (0.44f * iz) / (1.0f - 0.56f * iz) - 1.6295499532821566e-11f;
    jab.g = 3.524000f * lPQ - 4.066708f * mPQ + 0.542708f * sPQ;
    jab.b = 0.199076f * lPQ + 1.096799f * mPQ - 1.295875f * sPQ;

    return jab;
}

float3 ucsToRgb(float3 jab) 
{
    float jz = jab[0] + 1.6295499532821566e-11f;
    float iz = jz / (0.44f + 0.56f * jz);
    float a  = jab[1];
    float b  = jab[2];

    float l = iz + a * 1.386050432715393e-1f + b * 5.804731615611869e-2f;
    float m = iz + a * -1.386050432715393e-1f + b * -5.804731615611869e-2f;
    float s = iz + a * -9.601924202631895e-2f + b * -8.118918960560390e-1f;

    float lLin = eotfSt2084(l, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float mLin = eotfSt2084(m, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float sLin = eotfSt2084(s, JZAZBZ_EXPONENT_SCALE_FACTOR);

    float3 rgb;

    rgb.r = lLin * 2.990669f + mLin * -2.049742f + sLin * 0.088977f;
    rgb.g = lLin * -1.634525f + mLin * 3.145627f + sLin * -0.483037f;
    rgb.b = lLin * -0.042505f + mLin * -0.377983f + sLin * 1.448019f;

    return rgb;
}

float evaluateCurve(float x, float luminanceTarget) 
{ 
    float peakIntensity = luminanceTarget;
    float alpha         = 0.25f;
    float midPoint      = 0.538f;
    float linearSection = 0.444f;
    float toeStrength   = 1.280f;

    float k = (linearSection - 1.0f) / (alpha - 1.0f);
    float kA_     = peakIntensity * linearSection + peakIntensity * k;
    float kB_     = -peakIntensity * k * exp(linearSection / k);
    float kC_     = -1.0f / (k * peakIntensity);

    float weightLinear = smoothstep(0.0f, midPoint, x);
    float weightToe    = 1.0f - weightLinear;

    float shoulder = kA_ + kB_ * exp(x * kC_);
    float curvePeak = x < linearSection * peakIntensity ? 1 : 0;
    float toeMapped = midPoint * pow(x / midPoint, toeStrength);

    float mask = x < 0.0f ? 0 : 1; //if x == 0 return 0;
    return (1 - curvePeak ) * (weightToe * toeMapped + weightLinear * x) + curvePeak * shoulder * mask;

}

float3 applyToneMapping(float3 rgb) 
{    
    float sdrCorrectionFactor = 1.0f / physicalValueToFrameBufferValue(WHITE_LEVEL);
    float maxFramebufferValue = physicalValueToFrameBufferValue(WHITE_LEVEL);

    float3 white = float3( 
        maxFramebufferValue,
        maxFramebufferValue,
        maxFramebufferValue 
    );

    float3 whiteUcs = rgbToUcs(white);

    float maxLuminance = whiteUcs[0]; 

    float3 ucs = rgbToUcs(rgb);

    // Per-channel tone mapping ("skewed" color).
    float3 skewedRgb = float3( 
        evaluateCurve(rgb.r,maxFramebufferValue),
        evaluateCurve(rgb.g,maxFramebufferValue),
        evaluateCurve(rgb.b,maxFramebufferValue)
    );

    float3 skewedUcs = rgbToUcs(skewedRgb);

    float chromaScale =
        chromaCurve(ucs[0] / maxLuminance, 0.98f, 1.16f);

    float3 scaledUcs = float3(
        skewedUcs[0],       
        ucs[1] * chromaScale,
        ucs[2] * chromaScale
    );
    
    float3 scaledRgb  = ucsToRgb(scaledUcs);

    float3 out;

    for (int i = 0; i < 3; i++)
    {
        float blended = (1.0f - BLEND_RATIO) * skewedRgb[i] + BLEND_RATIO * scaledRgb[i];
        out[i] = sdrCorrectionFactor * min(blended, maxFramebufferValue);
    }

    return out;
}