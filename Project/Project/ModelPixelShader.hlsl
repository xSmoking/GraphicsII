Texture2D shaderTexture;
SamplerState SampleType;

cbuffer LightBuffer
{
	float4 ambientColor;
	float4 diffuseColor;
	float3 lightDirection;
	float padding;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
	float3 normal : NORMAL;
};

texture2D env : register(t0);
SamplerState envFilter : register(s0);

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 textureColor;
	float3 lightDir;
	float lightIntensity;
	float4 color;

	textureColor = shaderTexture.Sample(SampleType, input.color);
	color = ambientColor;
	lightDir = -lightDirection;
	lightIntensity = saturate(dot(input.normal, lightDir));

	if (lightIntensity > 0.0f)
		color += (diffuseColor * lightIntensity);

	color = saturate(color);
	color = color * textureColor;

	return env.Sample(envFilter, input.color.xy);
}