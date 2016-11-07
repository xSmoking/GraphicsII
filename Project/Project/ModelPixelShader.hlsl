struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float4 normal : NORMAL;
};

texture2D env : register(t0);
SamplerState envFilter : register(s0);

float4 main(PixelShaderInput input) : SV_TARGET
{
	return env.Sample(envFilter, input.color);
}