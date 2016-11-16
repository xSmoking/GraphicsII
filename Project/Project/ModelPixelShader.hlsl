struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
	float3 normal : NORMAL;
	float4 world : WORLD;
};

cbuffer LIGHT
{
	float4				color;
	float4				position;
	float4				direction;
	float4				coneDirection;
	float4	camera;
};

texture2D env : register(t1);
texture2D envNormal : register(t2);
texture2D envSpecular : register(t3);
SamplerState envFilter : register(s1);

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 surfaceColor = env.Sample(envFilter, input.color);
	float alpha = surfaceColor.w;
	float4 result = surfaceColor;

	// Normal texture
	float4 normalColor = envNormal.Sample(envFilter, input.color);
	input.normal = float4(normalize(input.normal.xyz), 1);
	// End normal

	// Speceular texture
	float3 toCam = normalize(camera.xyz - input.world.xyz);

	float3 toLight = 0;
	if(position.w == 1)
		toLight = normalize(-direction.xyz - input.world.xyz);
	else
		toLight = normalize(position.xyz - input.world.xyz);

	float3 refVec = float3(reflect(-toLight.xyz, input.normal.xyz));
	float specPow = saturate(dot(normalize(refVec), toCam));
	specPow = pow(specPow, 256);
	float specIntensity = 0.5f;
	float4 spec = color * specPow * specIntensity;

	// End specular

	if (position.w == 1) // Direction Light
	{
		float lightRatio = saturate(dot(normalize(-direction.xyz), input.normal.xyz));
		result = lightRatio * color * surfaceColor;
		result = result + spec;
	}

	if (position.w == 2) // Point Light
	{
		float3 lightDir = normalize(position.xyz - input.world.xyz);
		float lightRatio = saturate(dot(lightDir, input.normal.xyz));
		result = lightRatio * color * surfaceColor;
		result = result + spec;
	}

	if (position.w == 3) // Spotlight
	{
		float3 lightDir = normalize(position.xyz - input.world.xyz);
		float surfaceRatio = saturate(dot(-lightDir.xyz , coneDirection.xyz));

		float spotFactor = (surfaceRatio > coneDirection.w) ? 1 : 0;
		float lightRatio = saturate(dot(lightDir.xyz, input.normal.xyz));
		result = spotFactor * lightRatio * color * surfaceColor;
		result = result + spec;
	}

	result.w = alpha;
	return result;
}