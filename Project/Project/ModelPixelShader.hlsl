struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
	float3 normal : NORMAL;
};

cbuffer LIGHT
{
	float4	color;
	float4	position;
	float4	coneDirection;
};

texture2D env : register(t1);
SamplerState envFilter : register(s1);

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 surfaceColor = env.Sample(envFilter, input.color) * float4(0.8f, 0.8f, 0.8f, 1);
	if (surfaceColor.a < 0.5f)
		discard;

	if (position.w == 1) // Direction Light
	{
		float4 lightDir = normalize(position - input.pos);
		float4 lightRatio = clamp(dot(-lightDir, input.normal), 0, 1);
		float4 result = lightRatio * color * surfaceColor;
		return result;
	}
	
	if (position.w == 2) // Point Light
	{
		float4 lightDir = normalize(position - input.pos);
		float4 lightRatio = clamp(dot(lightDir, input.normal), 0, 1);
		float4 result = lightRatio * color * surfaceColor;
		return result;
	}

	if (position.w == 3) // Spotlight
	{
		float4 lightDir = normalize(position - input.pos);
		float4 surfaceRatio = clamp(dot(-lightDir, coneDirection), 0, 1);

		float4 spotFactor = (surfaceRatio > coneDirection.w) ? 1 : 0;
		float4 lightRatio = saturate(dot(lightDir, input.normal));
		float4 result = spotFactor * lightRatio * color * surfaceColor;
		return result;
	}

	return surfaceColor;
}