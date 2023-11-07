"#ifndef _PBR_STRUCTURES_FXH_\n"
"#define _PBR_STRUCTURES_FXH_\n"
"\n"
"#ifdef __cplusplus\n"
"\n"
"#   ifndef CHECK_STRUCT_ALIGNMENT\n"
"        // Note that defining empty macros causes GL shader compilation error on Mac, because\n"
"        // it does not allow standalone semicolons outside of main.\n"
"        // On the other hand, adding semicolon at the end of the macro definition causes gcc error.\n"
"#       define CHECK_STRUCT_ALIGNMENT(s) static_assert( sizeof(s) % 16 == 0, \"sizeof(\" #s \") is not multiple of 16\" )\n"
"#   endif\n"
"\n"
"#endif\n"
"\n"
"#ifndef PBR_WORKFLOW_METALLIC_ROUGHNESS\n"
"#   define PBR_WORKFLOW_METALLIC_ROUGHNESS 0\n"
"#endif\n"
"\n"
"#ifndef PBR_WORKFLOW_SPECULAR_GLOSINESS\n"
"#   define PBR_WORKFLOW_SPECULAR_GLOSINESS 1\n"
"#endif\n"
"\n"
"\n"
"#ifndef PBR_ALPHA_MODE_OPAQUE\n"
"#   define PBR_ALPHA_MODE_OPAQUE 0\n"
"#endif\n"
"\n"
"#ifndef PBR_ALPHA_MODE_MASK\n"
"#   define PBR_ALPHA_MODE_MASK 1\n"
"#endif\n"
"\n"
"#ifndef PBR_ALPHA_MODE_BLEND\n"
"#   define PBR_ALPHA_MODE_BLEND 2\n"
"#endif\n"
"\n"
"\n"
"struct GLTFNodeShaderTransforms\n"
"{\n"
"	float4x4 NodeMatrix;\n"
"\n"
"	int      JointCount;\n"
"    float    Dummy0;\n"
"    float    Dummy1;\n"
"    float    Dummy2;\n"
"};\n"
"#ifdef CHECK_STRUCT_ALIGNMENT\n"
"	CHECK_STRUCT_ALIGNMENT(GLTFNodeShaderTransforms);\n"
"#endif\n"
"\n"
"\n"
"struct PBRRendererShaderParameters\n"
"{\n"
"	float AverageLogLum;\n"
"	float MiddleGray;\n"
"    float WhitePoint;\n"
"	float PrefilteredCubeMipLevels;\n"
"\n"
"	float IBLScale;\n"
"	int   DebugViewType;\n"
"    float OcclusionStrength;\n"
"    float EmissionScale;\n"
"\n"
"    float4 UnshadedColor;\n"
"    float4 HighlightColor;\n"
"\n"
"    float4 CustomData;\n"
"};\n"
"#ifdef CHECK_STRUCT_ALIGNMENT\n"
"	CHECK_STRUCT_ALIGNMENT(PBRRendererShaderParameters);\n"
"#endif\n"
"\n"
"\n"
"struct PBRMaterialShaderInfo\n"
"{\n"
"	float4  BaseColorFactor;\n"
"	float4  EmissiveFactor;\n"
"	float4  SpecularFactor;\n"
"\n"
"	int   Workflow;\n"
"    float UVSelector0;\n"
"    float UVSelector1;\n"
"    float UVSelector2;\n"
"\n"
"    float UVSelector3;\n"
"    float UVSelector4;\n"
"    float TextureSlice0;\n"
"    float TextureSlice1;\n"
"\n"
"    float TextureSlice2;\n"
"    float TextureSlice3;\n"
"    float TextureSlice4;\n"
"	float MetallicFactor;\n"
"\n"
"	float RoughnessFactor;\n"
"	int   AlphaMode;\n"
"	float AlphaMaskCutoff;\n"
"    float OcclusionFactor;\n"
"\n"
"    // When texture atlas is used, UV scale and bias applied to\n"
"    // each texture coordinate set\n"
"    float4 UVScaleBias0;\n"
"    float4 UVScaleBias1;\n"
"    float4 UVScaleBias2;\n"
"    float4 UVScaleBias3;\n"
"    float4 UVScaleBias4;\n"
"\n"
"	float4 CustomData;\n"
"};\n"
"#ifdef CHECK_STRUCT_ALIGNMENT\n"
"	CHECK_STRUCT_ALIGNMENT(PBRMaterialShaderInfo);\n"
"#endif\n"
"\n"
"\n"
"struct PBRShaderAttribs\n"
"{\n"
"    GLTFNodeShaderTransforms    Transforms;\n"
"    PBRMaterialShaderInfo       Material;\n"
"    PBRRendererShaderParameters Renderer;\n"
"};\n"
"#ifdef CHECK_STRUCT_ALIGNMENT\n"
"	CHECK_STRUCT_ALIGNMENT(PBRShaderAttribs);\n"
"#endif\n"
"\n"
"\n"
"#ifndef BaseColorTextureUVSelector\n"
"#   define BaseColorTextureUVSelector UVSelector0\n"
"#endif\n"
"\n"
"#ifndef PhysicalDescriptorTextureUVSelector\n"
"#   define PhysicalDescriptorTextureUVSelector UVSelector1\n"
"#endif\n"
"\n"
"#ifndef NormalTextureUVSelector\n"
"#   define NormalTextureUVSelector UVSelector2\n"
"#endif\n"
"\n"
"#ifndef OcclusionTextureUVSelector\n"
"#   define OcclusionTextureUVSelector UVSelector3\n"
"#endif\n"
"\n"
"#ifndef EmissiveTextureUVSelector\n"
"#   define EmissiveTextureUVSelector UVSelector4\n"
"#endif\n"
"\n"
"\n"
"#ifndef BaseColorSlice\n"
"#   define BaseColorSlice TextureSlice0\n"
"#endif\n"
"\n"
"#ifndef PhysicalDescriptorSlice\n"
"#   define PhysicalDescriptorSlice TextureSlice1\n"
"#endif\n"
"\n"
"#ifndef NormalSlice\n"
"#   define NormalSlice TextureSlice2\n"
"#endif\n"
"\n"
"#ifndef OcclusionSlice\n"
"#   define OcclusionSlice TextureSlice3\n"
"#endif\n"
"\n"
"#ifndef EmissiveSlice\n"
"#   define EmissiveSlice TextureSlice4\n"
"#endif\n"
"\n"
"\n"
"#ifndef BaseColorUVScaleBias\n"
"#   define BaseColorUVScaleBias UVScaleBias0\n"
"#endif\n"
"\n"
"#ifndef PhysicalDescriptorUVScaleBias\n"
"#   define PhysicalDescriptorUVScaleBias UVScaleBias1\n"
"#endif\n"
"\n"
"#ifndef NormalMapUVScaleBias\n"
"#   define NormalMapUVScaleBias UVScaleBias2\n"
"#endif\n"
"\n"
"#ifndef OcclusionUVScaleBias\n"
"#   define OcclusionUVScaleBias UVScaleBias3\n"
"#endif\n"
"\n"
"#ifndef EmissiveUVScaleBias\n"
"#   define EmissiveUVScaleBias UVScaleBias4\n"
"#endif\n"
"\n"
"#endif // _PBR_STRUCTURES_FXH_\n"
