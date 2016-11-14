// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
};

// Per-vertex data used as input to the vertex shader.
struct GeometryShaderInput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float4 normal : NORMAL;
};

// Per-vertex data passed to the rasterizer.
struct GeometryShaderOutput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float4 normal : NORMAL;
};

// This geometry shader is a pass-through that leaves the geometry unmodified 
// and sets the render target array index.
[maxvertexcount(3)]
void main(line float4 input[2] : SV_POSITION, inout TriangleStream<GeometryShaderOutput> output)
{
	// red green and blue vertex
	GeometryShaderOutput verts[3] =
	{
		float4(1,0,0,1), float4(0,0,0,1), float4(1,0,0,0),
		float4(0,1,0,1), float4(0,0,0,1), float4(1,0,0,0),
		float4(0,0,1,1), float4(0,0,0,1), float4(1,0,0,0)
	};

	// bottom left
	verts[0].pos.xyz = input[0].xyz;
	verts[0].pos.x -= 0.5f;

	// bottom right
	verts[2].pos = verts[0].pos;
	verts[2].pos.x += 1.0f;

	// top center
	verts[1].pos.xyz = input[1].xyz;

	// prep triangle for rasterization
	float4x4 mVP = mul(view, projection);
	for (uint i = 0; i < 3; ++i)
		verts[i].pos = mul(verts[i].pos, mVP);

	// send verts to the rasterizer
	output.Append(verts[0]);
	output.Append(verts[1]);
	output.Append(verts[2]);

	// do not connect to other triangles
	output.RestartStrip();
}
