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
texture2D envNormal : register(t2);
texture2D envSpecular : register(t3);
SamplerState envFilter : register(s1);

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 surfaceColor = env.Sample(envFilter, input.color) * (0.8f, 0.8f, 0.8f, 1);
	float alpha = surfaceColor.w;
	float4 result = surfaceColor;
	
	float4 normalColor = envNormal.Sample(envFilter, input.color);
	input.normal = float4(normalize(input.normal.xyz - normalColor.xyz), 1);

	//float4 specularColor = envSpecular.Sample(envFilter, input.color);
	//float4 halfVector = position - input.pos;
	//input.normal = max(clamp(dot(input.normal.xyz, normalize(position)), 0, 1) * 32, 0);

	if (position.w == 1) // Direction Light
	{
		//float4 lightDir = normalize(position - input.pos);
		float4 lightDir = -position;
		float4 lightRatio = clamp(dot(-lightDir, input.normal), 0, 1);
		result = lightRatio * color * surfaceColor;
	}
	
	if (position.w == 2) // Point Light
	{
		//float4 lightDir = normalize(position - input.pos);
		float4 lightDir = -position;
		float4 lightRatio = clamp(dot(lightDir, input.normal), 0, 1);
		result = lightRatio * color * surfaceColor;
	}

	if (position.w == 3) // Spotlight
	{
		float4 lightDir = normalize(position - input.pos);
		//float4 lightDir = -position;
		float4 surfaceRatio = clamp(dot(-lightDir, coneDirection), 0, 1);

		float4 spotFactor = (surfaceRatio > coneDirection.w) ? 1 : 0;
		float4 lightRatio = saturate(dot(lightDir, input.normal));
		result = spotFactor * lightRatio * color * surfaceColor;
	}

	result.w = alpha;
	return result;
}