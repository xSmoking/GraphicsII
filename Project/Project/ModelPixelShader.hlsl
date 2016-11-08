struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float4 normal : NORMAL;
};

struct LIGHT
{
	float4 color : COLOR;
	float4 position : POSITION;
	float4 direction : DIRECTION;
	float4 radius : RADIUS;
};

texture2D env : register(t0);
SamplerState envFilter : register(s0);

float4 main(PixelShaderInput input, LIGHT light) : SV_TARGET
{
	float4 surfaceColor = env.Sample(envFilter, input.color);
	if (surfaceColor.a < 0.5f)
		discard;

	if (light.position.w == 1) // Direction Light
	{
		float4 lightPos = light.position;
		float4 lightColor = light.color;

		float4 lightDir = normalize(lightPos - input.pos);
		float4 lightRatio = saturate(dot(-lightDir, input.normal));
		float4 result = lightRatio * lightColor * surfaceColor;
		return result;
	}
	else if (light.position.w == 2) // Point Light
	{
		float4 lightPos = light.position;
		float4 lightColor = light.color;

		float4 lightDir = normalize(lightPos - input.pos);
		float4 lightRatio = saturate(dot(lightDir, input.normal));
		float4 result = lightRatio * lightColor * surfaceColor;
		return result;
	}
	else if (light.position.w == 3) // Spotlight
	{
		float4 lightPos = light.position;
		float4 lightColor = light.color;
		float4 coneDir = light.direction;

		float4 lightDir = normalize(lightPos - input.pos);
		float4 surfaceRatio = saturate(dot(-lightDir, coneDir));
		float4 spotFactor = (surfaceRatio > 0.5) ? 1 : 0;
		float4 lightRatio = saturate(dot(lightDir, input.normal));
		float4 result = lightRatio * lightColor * surfaceColor;
		return result;
	}

	float4 lightPos = float4(-5.0f, 6.0f, 0, 0);
	float4 lightColor = float4(1, 1, 1, 0);
	float4 coneDir = float4(0, 0, 0, 0);

	float4 lightDir = normalize(lightPos - input.pos);
	float4 surfaceRatio = saturate(dot(-lightDir, coneDir));
	float4 spotFactor = (surfaceRatio > 0.5) ? 1 : 0;
	float4 lightRatio = saturate(dot(lightDir, input.normal));
	float4 result = lightRatio * lightColor * surfaceColor;
	return result;

	//return surfaceColor;
}