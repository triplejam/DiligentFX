"#include \"ScreenSpaceReflectionStructures.fxh\"\n"
"#include \"SSR_Common.fxh\"\n"
"\n"
"cbuffer cbScreenSpaceReflectionAttribs\n"
"{\n"
"    ScreenSpaceReflectionAttribs g_SSRAttribs;\n"
"}\n"
"\n"
"Texture2D<float>  g_TextureDepth;\n"
"Texture2D<float3> g_TextureNormal;\n"
"Texture2D<float>  g_TextureRoughness;\n"
"\n"
"Texture2D<float4> g_TextureRadiance;\n"
"Texture2D<float>  g_TextureVariance;\n"
"\n"
"float SampleDepth(int2 PixelCoord)\n"
"{\n"
"    return g_TextureDepth.Load(int3(PixelCoord, 0));\n"
"}\n"
"\n"
"float SampleRoughness(int2 PixelCoord)\n"
"{\n"
"    return g_TextureRoughness.Load(int3(PixelCoord, 0));\n"
"}\n"
"\n"
"float3 SampleNormalWS(int2 PixelCoord)\n"
"{\n"
"    return g_TextureNormal.Load(int3(PixelCoord, 0));\n"
"}\n"
"\n"
"float4 SampleRadiance(int2 PixelCoord)\n"
"{\n"
"    return g_TextureRadiance.Load(int3(PixelCoord, 0));\n"
"}\n"
"\n"
"float SampleVariance(int2 PixelCoord)\n"
"{\n"
"    return g_TextureVariance.Load(int3(PixelCoord, 0));\n"
"}\n"
"\n"
"bool IsReflectionSample(float Roughness, float Depth)\n"
"{\n"
"    return Roughness < g_SSRAttribs.RoughnessThreshold && !IsBackground(Depth);\n"
"}\n"
"\n"
"SSR_ATTRIBUTE_EARLY_DEPTH_STENCIL\n"
"float4 ComputeBilateralCleanupPS(in float4 Position : SV_Position) : SV_Target0\n"
"{\n"
"    float  Roughness   = SampleRoughness(int2(Position.xy));\n"
"    float  Variance    = SampleVariance(int2(Position.xy));\n"
"    float3 NormalWS    = SampleNormalWS(int2(Position.xy));\n"
"    float  LinearDepth = DepthToCameraZ(SampleDepth(int2(Position.xy)), g_SSRAttribs.ProjMatrix);\n"
"    float2 GradDepth   = float2(ddx(LinearDepth), ddy(LinearDepth));\n"
"\n"
"    float RoughnessTarget = saturate(float(SSR_BILATERAL_ROUGHNESS_FACTOR) * sqrt(Roughness));\n"
"    float Radius = lerp(0.0, Variance > SSS_BILATERAL_VARIANCE_ESTIMATE_THRESHOLD ? 2.0 : 0.0, RoughnessTarget);\n"
"    float Sigma = g_SSRAttribs.BilateralCleanupSpatialSigmaFactor;\n"
"    int EffectiveRadius = int(min(2.0 * Sigma, Radius));\n"
"    float4 RadianceResult = SampleRadiance(int2(Position.xy));\n"
"\n"
"    if (Variance > SSR_BILATERAL_VARIANCE_EXIT_THRESHOLD && EffectiveRadius > 0)\n"
"    {\n"
"        float4 ColorSum = float4(0.0, 0.0, 0.0, 0.0);\n"
"        float WeightSum = 0.0f;\n"
"        for (int x = -EffectiveRadius; x <= EffectiveRadius; x++)\n"
"        {\n"
"            for (int y = -EffectiveRadius; y <= EffectiveRadius; y++)\n"
"            {\n"
"                int2 Location = int2(Position.xy) + int2(x, y);\n"
"                if (IsInsideScreen(Location,g_SSRAttribs.RenderSize))\n"
"                {\n"
"                    float  SampledDepth     = SampleDepth(Location);\n"
"                    float  SampledRoughness = SampleRoughness(Location);\n"
"                    float4 SampledRadiance  = SampleRadiance(Location);\n"
"                    float3 SampledNormalWS  = SampleNormalWS(Location);\n"
"\n"
"                    if (IsReflectionSample(SampledRoughness, SampledDepth))\n"
"                    {\n"
"                        float SampledLinearDepth = DepthToCameraZ(SampledDepth, g_SSRAttribs.ProjMatrix);\n"
"                        float WeightS = exp(-0.5 * dot(float2(x, y), float2(x, y)) / (Sigma * Sigma));\n"
"                        float WeightZ = exp(-abs(LinearDepth - SampledLinearDepth) / (SSR_BILATERAL_SIGMA_DEPTH * abs(dot(float2(x, y), GradDepth) + 1.e-6)));\n"
"                        float WeightN = pow(max(0.0, dot(NormalWS, SampledNormalWS)), SSR_BILATERAL_SIGMA_NORMAL);\n"
"                        float Weight = WeightS * WeightN * WeightZ;\n"
"\n"
"                        WeightSum += Weight;\n"
"                        ColorSum  += Weight * SampledRadiance;\n"
"                    }\n"
"                }\n"
"            }\n"
"        }\n"
"\n"
"        RadianceResult = ColorSum / max(WeightSum, 1.0e-6f);\n"
"    }\n"
"\n"
"    return RadianceResult;\n"
"}\n"
