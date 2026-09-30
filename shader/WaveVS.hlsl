#include "common.hlsl"


void main(in VS_IN In, out PS_IN Out)
{
	matrix wvp;
	wvp = mul(World, View);
	wvp = mul(wvp, Projection);

	Out.Position = mul(In.Position, wvp);
	Out.TexCoord = In.TexCoord;
	Out.Diffuse = In.Diffuse * Material.Diffuse;
    Out.WorldPosition = mul(In.Position, World);

    float4 normal = In.Normal;
    normal.w = 0.0f;
    normal = mul(normal, World);
    normal = normalize(normal);
    Out.Normal = normal;
	
    float3 lightDirection = Light.Direction.xyz;
    lightDirection = normalize(lightDirection);
	
    float light = -dot(normal.xyz, lightDirection); // ランバート拡散証明
    light = saturate(light); // 0.0～1.0の範囲に制限
	
    Out.Diffuse.rgb *= light;
}