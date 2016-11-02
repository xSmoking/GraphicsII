// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
	float3 normal : NORMAL;
};

//// A pass-through function for the (interpolated) color data.
//float4 main(PixelShaderInput input) : SV_TARGET
//{
//	return float4(input.color, 1.0f);
//}

texture2D env : register(t0);
SamplerState envFilter : register(s0);

float4 main(PixelShaderInput psInput) : SV_TARGET
{
	return env.Sample(envFilter, psInput.color.xy);
}