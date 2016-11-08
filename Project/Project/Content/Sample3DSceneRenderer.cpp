﻿#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "..\Common\DirectXHelper.h"
#include "..\Common\DDSTextureLoader.h"

using namespace DX11UWA;

using namespace DirectX;
using namespace Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources)
{
	memset(m_kbuttons, 0, sizeof(m_kbuttons));
	m_currMousePos = nullptr;
	m_prevMousePos = nullptr;
	memset(&m_camera, 0, sizeof(XMFLOAT4X4));
	srand(unsigned int(time(0)));

	ID3D11SamplerState *samplerState;
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxAnisotropy = (UINT)1.0f;
	samplerDesc.MipLODBias = 1.0f;
	m_deviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc, &samplerState);
	m_deviceResources->GetD3DDeviceContext()->PSSetSamplers(0, 1, &samplerState);

	// Initialize Models
	MODEL Katarina, Ahri;
	MODEL cathedral_floor;
	Katarina.m_position = XMFLOAT3(-2.0f, 0, 0);
	Ahri.m_position = XMFLOAT3(2.0f, 0, 0);

	m_scene.models.push_back(Katarina);
	m_scene.models.push_back(Ahri);
	m_scene.models.push_back(cathedral_floor);

	// Initialize Lights
	m_scene.directionLight.color = XMFLOAT4(1, 1, 1, 0);
	m_scene.directionLight.position = XMFLOAT4(2.0f, 0, 0, 0);
	m_scene.pointLight.color = XMFLOAT4(1, 0, 0, 0);
	m_scene.pointLight.position = XMFLOAT4(0, 0, 0, 0);

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources(void)
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, 0.01f, 100.0f);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();

	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(&m_constantBufferData.projection, XMMatrixTranspose(perspectiveMatrix * orientationMatrix));

	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	static const XMVECTORF32 eye = { 0.0f, 0.7f, -1.5f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, -0.1f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&m_camera, XMMatrixInverse(nullptr, XMMatrixLookAtLH(eye, at, up)));
	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));
}

XMFLOAT4 MatrixByVector(XMFLOAT4X4 matrix, XMFLOAT4 vector)
{
	XMFLOAT4 out;
	out.x = matrix._11 * vector.x + matrix._12 * vector.y + matrix._13 * vector.z + matrix._14 * vector.w;
	out.y = matrix._21 * vector.x + matrix._22 * vector.y + matrix._23 * vector.z + matrix._24 * vector.w;
	out.z = matrix._31 * vector.x + matrix._32 * vector.y + matrix._33 * vector.z + matrix._34 * vector.w;
	out.w = matrix._41 * vector.x + matrix._42 * vector.y + matrix._43 * vector.z + matrix._44 * vector.w;
	return out;
}

XMFLOAT4 MatrixByVector(XMFLOAT4X4 matrix, XMFLOAT3 vector)
{
	XMFLOAT4 out;
	out.x = matrix._11 * vector.x + matrix._12 * vector.y + matrix._13 * vector.z;
	out.y = matrix._21 * vector.x + matrix._22 * vector.y + matrix._23 * vector.z;
	out.z = matrix._31 * vector.x + matrix._32 * vector.y + matrix._33 * vector.z;
	return out;
}

XMFLOAT4 LookAt(XMFLOAT4 position, XMFLOAT4 look)
{
	XMFLOAT4 out;
	out.x = look.x - position.x;
	out.y = look.y - position.y;
	out.z = look.z - position.z;
	out.w = look.w - position.w;
	return out;
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (!m_tracking)
	{
		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
		double totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));

		Translate(m_scene.models[0].m_constantBufferData, m_scene.models[0].m_position);
		Translate(m_scene.models[1].m_constantBufferData, m_scene.models[1].m_position);
		Translate(m_scene.models[2].m_constantBufferData, m_scene.models[2].m_position);
	}

	
	StaticSkybox(m_scene.skybox.m_constantBufferData, XMFLOAT3(m_camera._41, m_camera._42, m_camera._43));

	// Update or move camera here
	UpdateCamera(timer, 5.0f, 0.75f);
}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::TranslateAndRotate(ModelViewProjectionConstantBuffer &objectM, XMFLOAT3 pos, float radians)
{
	XMStoreFloat4x4(&objectM.model, XMMatrixTranspose(XMMatrixRotationY(radians) * XMMatrixTranslation(pos.x, pos.y, pos.z)));
}

void Sample3DSceneRenderer::Rotate(ModelViewProjectionConstantBuffer &objectM, float radians)
{
	XMStoreFloat4x4(&objectM.model, XMMatrixTranspose(XMMatrixRotationY(radians)));
}

void Sample3DSceneRenderer::Translate(ModelViewProjectionConstantBuffer &objectM, XMFLOAT3 pos)
{
	XMStoreFloat4x4(&objectM.model, XMMatrixTranspose(XMMatrixTranslation(pos.x, pos.y, pos.z)));
}

void Sample3DSceneRenderer::StaticSkybox(ModelViewProjectionConstantBuffer &objectM, XMFLOAT3 pos)
{
	XMStoreFloat4x4(&objectM.model, XMMatrixTranspose(XMMatrixTranslation(pos.x, pos.y, pos.z)) * XMMatrixScaling(110, 110, 110));
}

void Sample3DSceneRenderer::Orbit(ModelViewProjectionConstantBuffer &objectM, XMFLOAT3 radians, XMFLOAT3 orbitpos, XMFLOAT3 orbitness)
{
	XMStoreFloat4x4(&objectM.model, XMMatrixTranspose(XMMatrixTranslation(orbitness.x, orbitness.y, orbitness.z) * (XMMatrixRotationX(radians.x) * XMMatrixRotationY(radians.y) * XMMatrixRotationZ(radians.z)) * XMMatrixTranslation(orbitpos.x, orbitpos.y, orbitpos.z)));
}

void Sample3DSceneRenderer::UpdateCamera(DX::StepTimer const& timer, float const moveSpd, float const rotSpd)
{
	const float delta_time = (float)timer.GetElapsedSeconds();

	if (m_kbuttons['W'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, 0.0f, moveSpd * delta_time);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['S'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, 0.0f, -moveSpd * delta_time);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['A'])
	{
		XMMATRIX translation = XMMatrixTranslation(-moveSpd * delta_time, 0.0f, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['D'])
	{
		XMMATRIX translation = XMMatrixTranslation(moveSpd * delta_time, 0.0f, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['X'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, -moveSpd * delta_time, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons[VK_SPACE])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, moveSpd * delta_time, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}

	if (m_currMousePos)
	{
		if (m_currMousePos->Properties->IsRightButtonPressed && m_prevMousePos)
		{
			float dx = m_currMousePos->Position.X - m_prevMousePos->Position.X;
			float dy = m_currMousePos->Position.Y - m_prevMousePos->Position.Y;

			XMFLOAT4 pos = XMFLOAT4(m_camera._41, m_camera._42, m_camera._43, m_camera._44);

			m_camera._41 = 0;
			m_camera._42 = 0;
			m_camera._43 = 0;

			XMMATRIX rotX = XMMatrixRotationX(dy * rotSpd * delta_time);
			XMMATRIX rotY = XMMatrixRotationY(dx * rotSpd * delta_time);

			XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
			temp_camera = XMMatrixMultiply(rotX, temp_camera);
			temp_camera = XMMatrixMultiply(temp_camera, rotY);

			XMStoreFloat4x4(&m_camera, temp_camera);

			m_camera._41 = pos.x;
			m_camera._42 = pos.y;
			m_camera._43 = pos.z;
		}
		m_prevMousePos = m_currMousePos;
	}
}

void Sample3DSceneRenderer::SetKeyboardButtons(const char* list)
{
	memcpy_s(m_kbuttons, sizeof(m_kbuttons), list, sizeof(m_kbuttons));
}

void Sample3DSceneRenderer::SetMousePosition(const Windows::UI::Input::PointerPoint^ pos)
{
	m_currMousePos = const_cast<Windows::UI::Input::PointerPoint^>(pos);
}

void Sample3DSceneRenderer::SetInputDeviceData(const char* kb, const Windows::UI::Input::PointerPoint^ pos)
{
	SetKeyboardButtons(kb);
	SetMousePosition(pos);
}

void DX11UWA::Sample3DSceneRenderer::StartTracking(void)
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void Sample3DSceneRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
		float radians = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
		//Rotate(radians);
	}
}

void Sample3DSceneRenderer::StopTracking(void)
{
	m_tracking = false;
}

// Renders one frame using the vertex and pixel shaders.
void Sample3DSceneRenderer::Render(void)
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();
	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_camera))));

	// CUBE STUFF
	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource1(m_constantBuffer.Get(), 0, NULL, &m_constantBufferData, 0, 0, 0);
	// Each vertex is one instance of the VertexPositionColor struct.
	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	// Each index is one 16-bit unsigned integer (short).
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());
	// Attach our vertex shader.
	context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	// Send the constant buffer to the graphics device.
	context->VSSetConstantBuffers1(0, 1, m_constantBuffer.GetAddressOf(), nullptr, nullptr);
	// Attach our pixel shader.
	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
	// Draw the objects.
	//context->DrawIndexed(m_indexCount, 0, 0);

	// MY STUFF
	for(size_t i = 0; i < m_scene.models.size(); ++i)
	{
		ID3D11ShaderResourceView *shaderResourceView[] = { m_scene.models[i].m_texture };
		context->PSSetShaderResources(0, 1, shaderResourceView);

		m_scene.models[i].m_constantBufferData.view = m_constantBufferData.view;
		m_scene.models[i].m_constantBufferData.projection = m_constantBufferData.projection;

		context->UpdateSubresource1(m_scene.constantBuffer.Get(), 0, NULL, &m_scene.models[i].m_constantBufferData, 0, 0, 0);

		stride = sizeof(VertexPositionUVNormal);
		offset = 0;
		context->IASetVertexBuffers(0, 1, m_scene.models[i].m_vertexBuffer.GetAddressOf(), &stride, &offset);

		context->IASetIndexBuffer(m_scene.models[i].m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(m_scene.inputLayout.Get());

		context->VSSetShader(m_scene.vertexShader.Get(), nullptr, 0);
		context->VSSetConstantBuffers1(0, 1, m_scene.constantBuffer.GetAddressOf(), nullptr, nullptr);
		context->PSSetShader(m_scene.pixelShader.Get(), nullptr, 0);

		if(m_scene.models[i].m_render)
			context->DrawIndexed(m_scene.models[i].m_indexCount, 0, 0);
	}

	// INSTANCE STUFF
	ID3D11ShaderResourceView *instaceShaderResourceView[] = { m_scene.models[0].m_texture };
	context->PSSetShaderResources(0, 1, instaceShaderResourceView);
	UINT strides[2];
	UINT offsets[2];
	Microsoft::WRL::ComPtr<ID3D11Buffer> bufferPointers[2];
	strides[0] = sizeof(VertexPositionUVNormal);
	strides[1] = sizeof(INSTANCE);
	offsets[0] = 0;
	offsets[1] = 0;

	bufferPointers[0] = m_scene.models[0].m_vertexBuffer;
	bufferPointers[1] = m_instanceBuffer;

	context->UpdateSubresource1(m_scene.constantBuffer.Get(), 0, NULL, &m_scene.models[0].m_constantBufferData, 0, 0, 0);
	context->IASetVertexBuffers(0, 2, bufferPointers->GetAddressOf(), strides, offsets);
	context->IASetIndexBuffer(m_scene.models[0].m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_scene.inputLayout.Get());
	context->DrawIndexedInstanced(m_scene.models[0].m_indexCount, m_instanceCount, 0, 0, 0);
	// END INSTANCE BUFFER

	ID3D11ShaderResourceView *shaderResourceView[] = { m_scene.skybox.m_texture };
	context->PSSetShaderResources(0, 1, shaderResourceView);

	m_scene.skybox.m_constantBufferData.view = m_constantBufferData.view;
	m_scene.skybox.m_constantBufferData.projection = m_constantBufferData.projection;

	context->UpdateSubresource1(m_scene.constantBuffer.Get(), 0, NULL, &m_scene.skybox.m_constantBufferData, 0, 0, 0);

	stride = sizeof(VertexPositionColor);
	offset = 0;
	context->IASetVertexBuffers(0, 1, m_scene.skybox.m_vertexBuffer.GetAddressOf(), &stride, &offset);

	context->IASetIndexBuffer(m_scene.skybox.m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_scene.inputLayout.Get());

	context->VSSetShader(m_scene.skybox.m_vertexShader.Get(), nullptr, 0);
	context->VSSetConstantBuffers1(0, 1, m_scene.constantBuffer.GetAddressOf(), nullptr, nullptr);
	context->PSSetShader(m_scene.skybox.m_pixelShader.Get(), nullptr, 0);

	context->DrawIndexed(m_scene.skybox.m_indexCount, 0, 0);
}

bool Sample3DSceneRenderer::LoadObject(const char *_path, std::vector<VertexPositionUVNormal> &_verts, std::vector<unsigned int> &_inds, const float resizeFactor = 1.0f)
{
	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<XMFLOAT3> vertices_vector;
	std::vector<XMFLOAT3> uvs_vector;
	std::vector<XMFLOAT3> normals_vector;

	std::ifstream file;
	file.open(_path);

	if (file.is_open())
	{
		while (true)
		{
			std::string buffer;
			std::getline(file, buffer);
			if (file.eof()) break;
			std::stringstream stream(buffer);

			std::string type;
			stream >> type;

			if (type == "v")
			{
				XMFLOAT3 vertex;
				stream >> vertex.x >> vertex.y >> vertex.z;
				vertex.x /= resizeFactor;
				vertex.y /= resizeFactor;
				vertex.z /= resizeFactor;

				//vertex.x = -vertex.x;
				vertices_vector.push_back(vertex);
			}
			else if (type == "vt")
			{
				XMFLOAT3 uv;
				stream >> uv.x >> uv.y;
				uv.y = 1 - uv.y;
				uvs_vector.push_back(uv);
			}
			else if (type == "vn")
			{
				XMFLOAT3 normal;
				stream >> normal.x >> normal.y >> normal.z;
				//normal.x = -normal.x;
				normals_vector.push_back(normal);
			}
			else if (type == "f")
			{
				std::string buffer2;
				int count = 0;
				
				std::string teste;
				std::string teste1;
				std::string teste2;
				
				std::string teste3;
				std::string teste4;
				std::string teste5;

				std::string teste6;
				std::string teste7;
				std::string teste8;

				std::getline(stream, teste, '/');
				std::getline(stream, teste1, '/');
				std::getline(stream, teste2, ' ');

				std::getline(stream, teste3, '/');
				std::getline(stream, teste4, '/');
				std::getline(stream, teste5, ' ');

				std::getline(stream, teste6, '/');
				std::getline(stream, teste7, '/');
				std::getline(stream, teste8, '\n');

				vertexIndices.push_back(stoi(teste));
				vertexIndices.push_back(stoi(teste3));
				vertexIndices.push_back(stoi(teste6));

				uvIndices.push_back(stoi(teste1));
				uvIndices.push_back(stoi(teste4));
				uvIndices.push_back(stoi(teste7));

				normalIndices.push_back(stoi(teste2));
				normalIndices.push_back(stoi(teste5));
				normalIndices.push_back(stoi(teste8));
			}
		}

		file.close();
	}
	else
		return false;

	for (unsigned int i = 0; i < vertexIndices.size(); ++i)
	{
		VertexPositionUVNormal temp;
		temp.pos = vertices_vector[vertexIndices[i] - 1];
		temp.uv = uvs_vector[uvIndices[i] - 1];
		temp.normal = normals_vector[normalIndices[i] - 1];

		_verts.push_back(temp);
		_inds.push_back(i);
	}

	return true;
}

void Sample3DSceneRenderer::CreateDeviceDependentResources(void)
{
	// CUBE LOADING
	auto loadVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso");

	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, &m_vertexShader));

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &fileData[0], fileData.size(), &m_inputLayout));
	});

	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_pixelShader));

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer));
	});

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this]()
	{
		static const VertexPositionColor cubeVertices[] =
		{
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
			{ XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
			{ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		};

		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		vertexBufferData.pSysMem = cubeVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_vertexBuffer));

		// Load mesh indices. Each trio of indices represents
		// a triangle to be rendered on the screen.
		// For example: 0,2,1 means that the vertices with indexes
		// 0, 2 and 1 from the vertex buffer compose the
		// first triangle of this mesh.
		static const unsigned short cubeIndices[] =
		{
			0,1,2, // -x
			1,3,2,

			4,6,5, // +x
			5,6,7,

			0,5,1, // -y
			0,4,5,

			2,7,6, // +y
			2,3,7,

			0,6,4, // -z
			0,2,6,

			1,7,3, // +z
			1,5,7,
		};

		m_indexCount = ARRAYSIZE(cubeIndices);

		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		indexBufferData.pSysMem = cubeIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_indexBuffer));
	});

	createCubeTask.then([this]()
	{
		m_loadingComplete = true;
	});

	// MODEL LOADING
	auto loadVSModel = DX::ReadDataAsync(L"ModelVertexShader.cso"); // Model Vertex Shader
	auto loadPSModel = DX::ReadDataAsync(L"ModelPixelShader.cso"); // MOdel Pixel Shader
	auto loadVSSkyboxTask = DX::ReadDataAsync(L"SkyboxVertexShader.cso"); // Skybox Vertex Shader
	auto loadPSSkyboxTask = DX::ReadDataAsync(L"SkyboxPixelShader.cso"); // Skybox Pixel Shader
	auto loadPSDirectionalLight = DX::ReadDataAsync(L"DirectionalLightPixelShader.cso"); // Directional Light PS
	auto loadPSPointLight = DX::ReadDataAsync(L"PointLightPixelShader.cso"); // Directional Light PS
	auto loadPSSpotLight = DX::ReadDataAsync(L"SpotLightPixelShader.cso"); // Directional Light PS

	auto createVSModel = loadVSModel.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, &m_scene.vertexShader));

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		};

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &fileData[0], fileData.size(), &m_scene.inputLayout));
	});

	auto createPSModel = loadPSModel.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_scene.pixelShader));

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_scene.constantBuffer));
	});

	auto createPSDirectionalLight = loadPSDirectionalLight.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_scene.pixelShader));
	});

	//auto createPSPointLight = loadPSPointLight.then([this](const std::vector<byte>& fileData)
	//{
	//	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_scene.pixelShader));
	//});

	//auto createPSSpotLight = loadPSSpotLight.then([this](const std::vector<byte>& fileData)
	//{
	//	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_scene.pixelShader));
	//});

	auto createVSSkyboxTask = loadVSSkyboxTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, &m_scene.skybox.m_vertexShader));
	});

	auto createPSSkyboxTask = loadPSSkyboxTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_scene.skybox.m_pixelShader));
	});

	auto createKatarina = (createPSModel && createVSModel).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/katarina.obj", verts, inds))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/katarina.dds", nullptr, &m_scene.models[0].m_texture));

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = verts.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionUVNormal) * verts.size(), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_scene.models[0].m_vertexBuffer));

			m_scene.models[0].m_indexCount = inds.size();

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = inds.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * inds.size(), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_scene.models[0].m_indexBuffer));

			m_instanceCount = 3;
			std::vector<INSTANCE> instances;
			for(size_t i = 0; i < m_instanceCount; ++i)
			{
				INSTANCE instance;
				instance.position.x = float((rand() % 5) + 1);
				instance.position.y = float((rand() % 5) + 1);
				instance.position.z = float((rand() % 5) + 1);
				instance.position.w = 1;
				instances.push_back(instance);
			}

			D3D11_SUBRESOURCE_DATA instanceSubresource;
			instanceSubresource.pSysMem = instances.data();
			instanceSubresource.SysMemPitch = 0;
			instanceSubresource.SysMemSlicePitch = 0;

			D3D11_BUFFER_DESC instanceDesc;
			instanceDesc.Usage = D3D11_USAGE_DEFAULT;
			instanceDesc.ByteWidth = sizeof(INSTANCE) * instances.size();
			instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			instanceDesc.CPUAccessFlags = 0;
			instanceDesc.MiscFlags = 0;
			instanceDesc.StructureByteStride = 0;

			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&instanceDesc, &instanceSubresource, &m_instanceBuffer));
		}
	});

	auto createAhri = (createPSModel && createVSModel).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/ahri.obj", verts, inds, 30))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/ahri.dds", nullptr, &m_scene.models[1].m_texture));

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = verts.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionUVNormal) * verts.size(), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_scene.models[1].m_vertexBuffer));

			m_scene.models[1].m_indexCount = inds.size();

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = inds.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * inds.size(), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_scene.models[1].m_indexBuffer));
		}
	});

	auto createCathedralFloor = (createPSModel && createVSModel).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/cathedral/floor.obj", verts, inds, 30))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/cathedral/floor.dds", nullptr, &m_scene.models[2].m_texture));

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = verts.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionUVNormal) * verts.size(), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_scene.models[2].m_vertexBuffer));

			m_scene.models[2].m_indexCount = inds.size();

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = inds.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * inds.size(), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_scene.models[2].m_indexBuffer));
		}
	});

	auto createSkyBox = (createPSSkyboxTask && createVSSkyboxTask).then([this]()
	{
		static const VertexPositionColor skyboxVertices[] =
		{
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
			{ XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
			{ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		};

		static const unsigned short skyboxIndices[] =
		{
			2,1,0, // -x
			2,3,1,

			5,6,4, // +x
			7,6,5,

			1,5,0, // -y
			5,4,0,

			6,7,2, // +y
			7,3,2,

			4,6,0, // -z
			6,2,0,

			3,7,1, // +z
			7,5,1,
		};

		DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/IceFlow.dds", nullptr, &m_scene.skybox.m_texture));

		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		vertexBufferData.pSysMem = skyboxVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(skyboxVertices), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_scene.skybox.m_vertexBuffer));

		m_scene.skybox.m_indexCount = ARRAYSIZE(skyboxIndices);

		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		indexBufferData.pSysMem = skyboxIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(skyboxIndices), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_scene.skybox.m_indexBuffer));
	});
}

void Sample3DSceneRenderer::ReleaseDeviceDependentResources(void)
{
	m_loadingComplete = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}