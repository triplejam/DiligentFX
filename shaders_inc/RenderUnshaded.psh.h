"#include \"PBR_Structures.fxh\"\n"
"#include \"VSOutputStruct.generated\"\n"
"\n"
"cbuffer cbPBRAttribs\n"
"{\n"
"    PBRShaderAttribs g_PBRAttribs;\n"
"}\n"
"\n"
"#include \"PSMainGenerated.generated\"\n"
"//void main(in  VSOutput VSOut,\n"
"//          out float4   OutColor : SV_Target)\n"
"//{\n"
"//    OutColor = g_PBRAttribs.Renderer.WireframeColor;\n"
"//}\n"
