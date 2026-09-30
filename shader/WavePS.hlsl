
#include "common.hlsl"


Texture2D		g_Texture : register(t0);
SamplerState	g_SamplerState : register(s0);

static float PI = 3.14159265358979323846;

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
	//視線ベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
	
	//法線ベクトル
    float3 normal = normalize(In.Normal.xyz);
	
	//反射ベクトル
    float3 reflectv = reflect(eyev, normal);

	//環境マッピングテクスチャ座標（パノラマ）
    float2 envTexCoord;
    envTexCoord.x = atan2(reflectv.x, reflectv.z) / (2.0 * PI) + 0.5;
    envTexCoord.y = acos(reflectv.y) / PI;
	
	if (Material.TextureEnable)
	{
		outDiffuse = g_Texture.Sample(g_SamplerState, envTexCoord);
	}
	else
	{
		outDiffuse = In.Diffuse;
	}	
}