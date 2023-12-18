"// PBR shader based on the Khronos WebGL PBR implementation\n"
"// See https://github.com/KhronosGroup/glTF-WebGL-PBR\n"
"// Supports both metallic roughness and specular glossiness inputs\n"
"\n"
"#include \"BasicStructures.fxh\"\n"
"#include \"PBR_Shading.fxh\"\n"
"#include \"RenderPBR_Structures.fxh\"\n"
"\n"
"#if ENABLE_TONE_MAPPING\n"
"#   include \"ToneMapping.fxh\"\n"
"#endif\n"
"\n"
"#include \"VSOutputStruct.generated\"\n"
"// struct VSOutput\n"
"// {\n"
"//     float4 ClipPos  : SV_Position;\n"
"//     float3 WorldPos : WORLD_POS;\n"
"//     float4 Color    : COLOR;\n"
"//     float3 Normal   : NORMAL;\n"
"//     float2 UV0      : UV0;\n"
"//     float2 UV1      : UV1;\n"
"// };\n"
"\n"
"#ifndef USE_TEXTURE_ATLAS\n"
"#   define USE_TEXTURE_ATLAS 0\n"
"#endif\n"
"\n"
"#ifndef ALLOW_DEBUG_VIEW\n"
"#   define ALLOW_DEBUG_VIEW 0\n"
"#endif\n"
"\n"
"#include \"PBR_Textures.fxh\"\n"
"\n"
"cbuffer cbFrameAttribs\n"
"{\n"
"    PBRFrameAttribs g_Frame;\n"
"}\n"
"\n"
"cbuffer cbPrimitiveAttribs\n"
"{\n"
"    PBRPrimitiveAttribs g_Primitive;\n"
"}\n"
"\n"
"float3 ApplyLights(SurfaceReflectanceInfo SrfInfo, float3 Normal, float3 View)\n"
"{\n"
"    //#ifdef USE_PUNCTUAL\n"
"    //    for (int i = 0; i < LIGHT_COUNT; ++i)\n"
"    //    {\n"
"    //        Light light = u_Lights[i];\n"
"    //        if (light.type == LightType_Directional)\n"
"    //        {\n"
"    //            color += applyDirectionalLight(light, materialInfo, normal, view);\n"
"    //        }\n"
"    //        else if (light.type == LightType_Point)\n"
"    //        {\n"
"    //            color += applyPointLight(light, materialInfo, normal, view);\n"
"    //        }\n"
"    //        else if (light.type == LightType_Spot)\n"
"    //        {\n"
"    //            color += applySpotLight(light, materialInfo, normal, view);\n"
"    //        }\n"
"    //    }\n"
"    //#endif\n"
"\n"
"    return ApplyDirectionalLight(g_Frame.Light.Direction.xyz, g_Frame.Light.Intensity.rgb, SrfInfo, Normal, View);\n"
"}\n"
"\n"
"float4 ComputePbrSurfaceColor(in VSOutput VSOut,\n"
"                              in bool     IsFrontFace)\n"
"{\n"
"    float4 BaseColor = GetBaseColor(VSOut, g_Primitive.Material);\n"
"\n"
"    PBRMaterialTextureAttribs NormalTexAttribs = g_Primitive.Material.Textures[NormalTextureAttribId];\n"
"\n"
"    float2 NormalMapUV = SelectUV(VSOut, NormalTexAttribs.UVSelector);\n"
"\n"
"    // We have to compute gradients in uniform flow control to avoid issues with perturbed normal\n"
"    float3 dWorldPos_dx = ddx(VSOut.WorldPos);\n"
"    float3 dWorldPos_dy = ddy(VSOut.WorldPos);\n"
"#if ENABLE_TEXCOORD_TRANSFORM\n"
"    NormalMapUV = TransformUV(NormalMapUV, NormalTexAttribs);\n"
"#endif\n"
"    float2 SmoothNormalMapUV = NormalMapUV;\n"
"    float2 dNormalMapUV_dx = ddx(NormalMapUV);\n"
"    float2 dNormalMapUV_dy = ddy(NormalMapUV);\n"
"#if USE_TEXTURE_ATLAS\n"
"    {\n"
"        NormalMapUV = frac(NormalMapUV) * NormalTexAttribs.AtlasUVScaleAndBias.xy + NormalTexAttribs.AtlasUVScaleAndBias.zw;\n"
"        SmoothNormalMapUV *= NormalTexAttribs.AtlasUVScaleAndBias.xy;\n"
"        dNormalMapUV_dx   *= NormalTexAttribs.AtlasUVScaleAndBias.xy;\n"
"        dNormalMapUV_dy   *= NormalTexAttribs.AtlasUVScaleAndBias.xy;\n"
"    }\n"
"#endif\n"
"\n"
"    PBRMaterialBasicAttribs BasicAttribs = g_Primitive.Material.Basic;\n"
"    if (BasicAttribs.AlphaMode == PBR_ALPHA_MODE_MASK && BaseColor.a < BasicAttribs.AlphaMaskCutoff)\n"
"    {\n"
"        discard;\n"
"    }\n"
"\n"
"    float3 TSNormal     = GetMicroNormal(g_Primitive.Material, NormalMapUV, SmoothNormalMapUV, dNormalMapUV_dx, dNormalMapUV_dy);\n"
"    float  Occlusion    = GetOcclusion(VSOut, g_Primitive.Material);\n"
"    float3 Emissive     = GetEmissive(VSOut, g_Primitive.Material);\n"
"    float4 PhysicalDesc = GetPhysicalDesc(VSOut, g_Primitive.Material);\n"
"\n"
"    float  ClearCoatFactor    = GetClearcoatFactor(VSOut, g_Primitive.Material);\n"
"    float  ClearCoatRoughness = GetClearcoatRoughness(VSOut, g_Primitive.Material);\n"
"    float3 ClearCoatNormal    = GetClearcoatNormal(VSOut, g_Primitive.Material);\n"
"\n"
"    float3 SheenColor     = GetSheenColor(VSOut, g_Primitive.Material);\n"
"    float  SheenRoughness = GetSheenRoughness(VSOut, g_Primitive.Material);\n"
"\n"
"    float3 Anisotropy = GetAnisotropy(VSOut, g_Primitive.Material);\n"
"\n"
"    float Iridescence          = GetIridescence(VSOut, g_Primitive.Material);\n"
"    float IridescenceThickness = GetIridescenceThickness(VSOut, g_Primitive.Material);\n"
"\n"
"    float Transmission    = GetTransmission(VSOut, g_Primitive.Material);\n"
"    float VolumeThickness = GetVolumeThickness(VSOut, g_Primitive.Material);\n"
"\n"
"    if (BasicAttribs.Workflow == PBR_WORKFLOW_SPECULAR_GLOSINESS)\n"
"    {\n"
"        PhysicalDesc.rgb = TO_LINEAR(PhysicalDesc.rgb) * BasicAttribs.SpecularFactor.rgb;\n"
"        const float u_GlossinessFactor = 1.0;\n"
"        PhysicalDesc.a *= u_GlossinessFactor;\n"
"    }\n"
"    else if (BasicAttribs.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS)\n"
"    {\n"
"        // PhysicalDesc should already be in linear space\n"
"        PhysicalDesc.g = saturate(PhysicalDesc.g * BasicAttribs.RoughnessFactor);\n"
"        PhysicalDesc.b = saturate(PhysicalDesc.b * BasicAttribs.MetallicFactor);\n"
"    }\n"
"    float metallic = 0.0;\n"
"    SurfaceReflectanceInfo SrfInfo = GetSurfaceReflectance(BasicAttribs.Workflow, BaseColor, PhysicalDesc, metallic);\n"
"\n"
"    float3 view = normalize(g_Frame.Camera.f4Position.xyz - VSOut.WorldPos.xyz); // Direction from surface point to camera\n"
"\n"
"#if USE_VERTEX_NORMALS\n"
"    float3 MeshNormal = VSOut.Normal;\n"
"#else\n"
"    // PerturbNormal can handle zero-length mesh normals.\n"
"    float3 MeshNormal = float3(0.0, 0.0, 0.0);\n"
"#endif\n"
"\n"
"    float3 perturbedNormal;\n"
"    float3 DirectLighting;\n"
"    float3 color;\n"
"\n"
"    IBL_Contribution IBLContrib;\n"
"    IBLContrib.f3Diffuse  = float3(0.0, 0.0, 0.0);\n"
"    IBLContrib.f3Specular = float3(0.0, 0.0, 0.0);\n"
"\n"
"    if (BasicAttribs.Workflow != PBR_WORKFLOW_UNLIT)\n"
"    {\n"
"        perturbedNormal = PerturbNormal(dWorldPos_dx,\n"
"                                        dWorldPos_dy,\n"
"                                        dNormalMapUV_dx,\n"
"                                        dNormalMapUV_dy,\n"
"                                        MeshNormal,\n"
"                                        TSNormal,\n"
"                                        NormalTexAttribs.UVSelector >= 0.0,\n"
"                                        IsFrontFace);\n"
"\n"
"        DirectLighting = ApplyLights(SrfInfo, perturbedNormal, view);\n"
"        color = DirectLighting;\n"
"\n"
"        // Calculate lighting contribution from image based lighting source (IBL)\n"
"#       if USE_IBL\n"
"        {\n"
"            IBLContrib =\n"
"                GetIBLContribution(SrfInfo, perturbedNormal, view, float(g_Frame.Renderer.PrefilteredCubeMipLevels),\n"
"                                   g_BRDF_LUT,          g_BRDF_LUT_sampler,\n"
"                                   g_IrradianceMap,     g_IrradianceMap_sampler,\n"
"                                   g_PrefilteredEnvMap, g_PrefilteredEnvMap_sampler);\n"
"            color += (IBLContrib.f3Diffuse + IBLContrib.f3Specular) * g_Frame.Renderer.IBLScale;\n"
"        }\n"
"#       endif\n"
"    }\n"
"    else\n"
"    {\n"
"        perturbedNormal = float3(0.0, 0.0, 0.0);\n"
"        DirectLighting  = float3(0.0, 0.0, 0.0);\n"
"        color           = BaseColor.rgb;\n"
"    }\n"
"\n"
"#   if USE_AO_MAP\n"
"    {\n"
"        color = lerp(color, color * Occlusion, g_Frame.Renderer.OcclusionStrength);\n"
"    }\n"
"#   endif\n"
"\n"
"#   if USE_EMISSIVE_MAP\n"
"    {\n"
"        color += Emissive.rgb * g_Frame.Renderer.EmissionScale;\n"
"    }\n"
"#   endif\n"
"\n"
"    float3 ClearCoatColor           = float3(0.0, 0.0, 0.0);\n"
"    float3 ClearCoatPerturbedNormal = MeshNormal;\n"
"#   if ENABLE_CLEAR_COAT\n"
"    {\n"
"        // Clear coat layer is applied on top of everything\n"
"        float IOR = 1.5;\n"
"        SurfaceReflectanceInfo ClearCoatSrfInfo = GetSurfaceReflectanceClearCoat(ClearCoatRoughness, IOR);\n"
"\n"
"        ClearCoatColor += ApplyLights(ClearCoatSrfInfo, ClearCoatPerturbedNormal, view);\n"
"\n"
"#       if USE_IBL\n"
"        {\n"
"            float3 ClearCoatIBL =\n"
"                GetIBLRadianceGGX(ClearCoatSrfInfo, ClearCoatPerturbedNormal, view, float(g_Frame.Renderer.PrefilteredCubeMipLevels),\n"
"                                  g_BRDF_LUT,          g_BRDF_LUT_sampler,\n"
"                                  g_PrefilteredEnvMap, g_PrefilteredEnvMap_sampler) * g_Frame.Renderer.IBLScale;\n"
"#           if USE_AO_MAP\n"
"            {\n"
"                ClearCoatIBL = lerp(ClearCoatIBL, ClearCoatIBL * Occlusion, g_Frame.Renderer.OcclusionStrength);\n"
"            }\n"
"#           endif\n"
"\n"
"            ClearCoatColor += ClearCoatIBL;\n"
"\n"
"            float ClearcoatFresnel = SchlickReflection(saturate(dot(ClearCoatPerturbedNormal, view)), ClearCoatSrfInfo.Reflectance0.x, ClearCoatSrfInfo.Reflectance90.x);\n"
"            ClearCoatColor *= ClearCoatFactor;\n"
"            color = color * (1.0 - ClearCoatFactor * ClearcoatFresnel) + ClearCoatColor;\n"
"        }\n"
"#       endif\n"
"    }\n"
"#   endif\n"
"\n"
"#if ENABLE_TONE_MAPPING\n"
"    {\n"
"        // Perform tone mapping\n"
"        ToneMappingAttribs TMAttribs;\n"
"        TMAttribs.iToneMappingMode     = TONE_MAPPING_MODE;\n"
"        TMAttribs.bAutoExposure        = false;\n"
"        TMAttribs.fMiddleGray          = g_Frame.Renderer.MiddleGray;\n"
"        TMAttribs.bLightAdaptation     = false;\n"
"        TMAttribs.fWhitePoint          = g_Frame.Renderer.WhitePoint;\n"
"        TMAttribs.fLuminanceSaturation = 1.0;\n"
"        color = ToneMap(color, TMAttribs, g_Frame.Renderer.AverageLogLum);\n"
"    }\n"
"#endif\n"
"\n"
"    // Add highlight color\n"
"    color = lerp(color, g_Frame.Renderer.HighlightColor.rgb, g_Frame.Renderer.HighlightColor.a);\n"
"\n"
"    float4 OutColor = float4(color, BaseColor.a);\n"
"\n"
"    // Shader inputs debug visualization\n"
"#if (DEBUG_VIEW == DEBUG_VIEW_BASE_COLOR)\n"
"    {\n"
"        OutColor.rgba = BaseColor;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_TEXCOORD0 && USE_TEXCOORD0)\n"
"    {\n"
"        OutColor.rgb = float3(VSOut.UV0, 0.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_TEXCOORD1 && USE_TEXCOORD1)\n"
"    {\n"
"        OutColor.rgb = float3(VSOut.UV1, 0.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_TRANSPARENCY)\n"
"    {\n"
"        OutColor.rgba = float4(BaseColor.a, BaseColor.a, BaseColor.a, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_NORMAL_MAP)\n"
"    {\n"
"        OutColor.rgb = TSNormal.xyz;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_OCCLUSION)\n"
"    {\n"
"        OutColor.rgb = Occlusion * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_EMISSIVE)\n"
"    {\n"
"        OutColor.rgb = Emissive.rgb;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_METALLIC)\n"
"    {\n"
"        OutColor.rgb = metallic * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_ROUGHNESS)\n"
"    {\n"
"        OutColor.rgb = SrfInfo.PerceptualRoughness * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_CLEAR_COAT)\n"
"    {\n"
"        OutColor.rgb = ClearCoatColor.rgb;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_CLEAR_COAT_FACTOR)\n"
"    {\n"
"        OutColor.rgb = ClearCoatFactor * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_CLEAR_COAT_ROUGHNESS)\n"
"    {\n"
"        OutColor.rgb = ClearCoatRoughness * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_CLEAR_COAT_NORMAL)\n"
"    {\n"
"        OutColor.rgb = ClearCoatNormal.xyz * float3(0.5, 0.5, 0.5) + float3(0.5, 0.5, 0.5);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_SHEEN_COLOR)\n"
"    {\n"
"        OutColor.rgb = SheenColor.rgb;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_SHEEN_ROUGHNESS)\n"
"    {\n"
"        OutColor.rgb = SheenRoughness * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_ANISOTROPY)\n"
"    {\n"
"        OutColor.rgb = Anisotropy.xyz;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_IRIDESCENCE)\n"
"    {\n"
"        OutColor.rgb = Iridescence * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_IRIDESCENCE_THICKNESS)\n"
"    {\n"
"        OutColor.rgb = (IridescenceThickness - Material.Iridescence.ThicknessMinimum) / max(Material.Iridescence.ThicknessMaximum - Material.Iridescence.ThicknessMinimum, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_TRANSMISSION)\n"
"    {\n"
"        OutColor.rgb = Transmission * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_THICKNESS)\n"
"    {\n"
"        OutColor.rgb = VolumeThickness * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_DIFFUSE_COLOR)\n"
"    {\n"
"        OutColor.rgb  = SrfInfo.DiffuseColor;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_SPECULAR_COLOR)\n"
"    {\n"
"        OutColor.rgb  = SrfInfo.Reflectance0;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_REFLECTANCE90)\n"
"    {\n"
"        OutColor.rgb  = SrfInfo.Reflectance90;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_MESH_NORMAL)\n"
"    {\n"
"        OutColor.rgb  = abs(MeshNormal/ max(length(MeshNormal), 1e-3));\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_PERTURBED_NORMAL)\n"
"    {\n"
"        OutColor.rgb  = abs(perturbedNormal);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_NDOTV)\n"
"    {\n"
"        OutColor.rgb  = dot(perturbedNormal, view) * float3(1.0, 1.0, 1.0);\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_DIRECT_LIGHTING)\n"
"    {\n"
"        OutColor.rgb  = DirectLighting;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_DIFFUSE_IBL && USE_IBL)\n"
"    {\n"
"        OutColor.rgb  = IBLContrib.f3Diffuse;\n"
"    }\n"
"#elif (DEBUG_VIEW == DEBUG_VIEW_SPECULAR_IBL && USE_IBL)\n"
"    {\n"
"        OutColor.rgb  = IBLContrib.f3Specular;\n"
"    }\n"
"#endif\n"
"\n"
"\n"
"#if CONVERT_OUTPUT_TO_SRGB\n"
"    {\n"
"        OutColor.rgb = FastLinearToSRGB(OutColor.rgb);\n"
"    }\n"
"#endif\n"
"\n"
"    return OutColor;\n"
"}\n"
"\n"
"\n"
"#include \"PSMainGenerated.generated\"\n"
"// struct PSOutput\n"
"// {\n"
"//     float4 Color      : SV_Target0;\n"
"//     float4 CustomData : SV_Target1;\n"
"// };\n"
"//\n"
"// void main(in VSOutput VSOut,\n"
"//           in bool IsFrontFace : SV_IsFrontFace,\n"
"//           out PSOutput PSOut)\n"
"// {\n"
"//     PSOut.Color = ComputePbrSurfaceColor(VSOut, IsFrontFace);\n"
"//\n"
"// #if ENABLE_CUSTOM_DATA_OUTPUT\n"
"//     {\n"
"//         PSOut.CustomData = g_Primitive.CustomData;\n"
"//     }\n"
"// #endif\n"
"// }\n"
