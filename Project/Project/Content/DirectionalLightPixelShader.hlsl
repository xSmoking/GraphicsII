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
	float4 surfaceColor = env.Sample(envFilter, input.color);
	if (surfaceColor.a < 0.5f)
		discard;

	float4 lightPos = float4(0, 20, 0, 0);
	float4 lightColor = float4(1, 1, 1, 0);
	
	float4 lightDir = normalize(lightPos - input.pos);
	float4 lightRatio = saturate(dot(-lightDir, input.normal));
	float4 result = lightRatio * lightColor * surfaceColor;
	return result;
}