#include "pch.h"
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
	// Initialize Light
	XMFLOAT4X4 camera;
	m_scene.camera.push_back(camera);
	m_scene.camera.push_back(camera);

	memset(m_kbuttons, 0, sizeof(m_kbuttons));
	m_currMousePos = nullptr;
	m_prevMousePos = nullptr;
	memset(&m_scene.camera[0], 0, sizeof(XMFLOAT4X4));
	memset(&m_scene.camera[1], 0, sizeof(XMFLOAT4X4));
	srand(unsigned int(time(0)));

	// Initialize Sampler State
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
	m_deviceResources->GetD3DDeviceContext()->PSSetSamplers(1, 1, &samplerState);
	m_deviceResources->GetD3DDeviceContext()->PSSetSamplers(0, 1, &samplerState);

	// Initialize Light
	D3D11_BUFFER_DESC lightBufferDesc;
	lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightBufferDesc.ByteWidth = sizeof(LIGHT);
	lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightBufferDesc.MiscFlags = 0;
	lightBufferDesc.StructureByteStride = 0;
	m_deviceResources->GetD3DDevice()->CreateBuffer(&lightBufferDesc, nullptr, &m_scene.lightBuffer);

	// Initialize Models
	MODEL Katarina, Ahri, Crate, Floor, MedievalHouse, Tree;
	Katarina.m_position = XMFLOAT3(-1.0f, 0.5f, 0);
	Ahri.m_position = XMFLOAT3(6.0f, 0.5f, 2.0f);
	Crate.m_position = XMFLOAT3(3.0f, 1.5f, 0.5f);
	MedievalHouse.m_position = XMFLOAT3(5.0f, 0.5f, 29.0f);
	Tree.m_position = XMFLOAT3(20.0f, 0, 35.0f);
	m_scene.models.push_back(Katarina);
	m_scene.models.push_back(Ahri);
	m_scene.models.push_back(Crate);
	m_scene.models.push_back(Floor);
	m_scene.models.push_back(MedievalHouse);
	m_scene.models.push_back(Tree);

	CreateViewports();
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources(void)
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = (outputSize.Width / 2) / outputSize.Height;
	float fovAngleY = 50.0f * XM_PI / 180.0f;

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
	XMStoreFloat4x4(&m_scene.camera[0], XMMatrixInverse(nullptr, XMMatrixLookAtLH(eye, at, up)));

	static const XMVECTORF32 eye2 = { 0.0f, 7.0f, -1.5f, 0.0f };
	static const XMVECTORF32 at2 = { 0.0f, -0.1f, 0.0f, 0.0f };
	static const XMVECTORF32 up2 = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMStoreFloat4x4(&m_scene.camera[1], XMMatrixInverse(nullptr, XMMatrixLookAtLH(eye2, at2, up2)));

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));
	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtLH(eye2, at2, up2)));
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

		// Rotate Light
		float radius = 7.0f;
		float offset = 0;
		XMFLOAT4 lightPos = XMFLOAT4(std::sin((float)totalRotation + offset) * radius, 7.0f, std::cos((float)totalRotation + offset) * radius, 1.0f);
		XMVECTOR lightDir = XMVectorSet(-lightPos.x, -lightPos.y, -lightPos.z, 1.0f);
		lightDir = XMVector3Normalize(lightDir);
		XMStoreFloat4(&m_lightDirection, lightDir);
	}
	
	Translate(m_scene.models[0].m_constantBufferData, m_scene.models[0].m_position);
	Translate(m_scene.models[1].m_constantBufferData, m_scene.models[1].m_position);
	Translate(m_scene.models[2].m_constantBufferData, m_scene.models[2].m_position);
	Translate(m_scene.models[3].m_constantBufferData, m_scene.models[3].m_position);
	TranslateAndRotate(m_scene.models[4].m_constantBufferData, m_scene.models[4].m_position, 3.05433f);
	Translate(m_scene.models[5].m_constantBufferData, m_scene.models[5].m_position);

	StaticSkybox(m_scene.skybox.m_constantBufferData, XMFLOAT3(m_scene.camera[0]._41, m_scene.camera[0]._42, m_scene.camera[0]._43));

	// Update or move camera here
	UpdateCamera(m_scene.camera[0], timer, 5.0f, 0.75f);
	//UpdateCamera(m_scene.camera[0], timer, 5.0f, 0.75f);
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

void Sample3DSceneRenderer::UpdateCamera(XMFLOAT4X4 camera, DX::StepTimer const& timer, float const moveSpd, float const rotSpd)
{
	const float delta_time = (float)timer.GetElapsedSeconds();

	if (m_kbuttons['W'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, 0.0f, moveSpd * delta_time);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[0]);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_scene.camera[0], result);
	}
	if (m_kbuttons['S'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, 0.0f, -moveSpd * delta_time);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[0]);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_scene.camera[0], result);
	}
	if (m_kbuttons['A'])
	{
		XMMATRIX translation = XMMatrixTranslation(-moveSpd * delta_time, 0.0f, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[0]);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_scene.camera[0], result);
	}
	if (m_kbuttons['D'])
	{
		XMMATRIX translation = XMMatrixTranslation(moveSpd * delta_time, 0.0f, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[0]);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_scene.camera[0], result);
	}
	if (m_kbuttons['X'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, -moveSpd * delta_time, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[0]);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_scene.camera[0], result);
	}
	if (m_kbuttons[VK_SPACE])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, moveSpd * delta_time, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[0]);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_scene.camera[0], result);
	}
	if (m_kbuttons['1'])
	{
		m_scene.lightType = 1;
	}
	if (m_kbuttons['2'])
	{
		m_scene.lightType = 2;
	}
	if (m_kbuttons['3'])
	{
		m_scene.lightType = 3;
	}
	if(m_kbuttons['4'])
	{
		m_scene.lightType = 4;
	}

	if (m_currMousePos)
	{
		if (m_currMousePos->Properties->IsRightButtonPressed && m_prevMousePos)
		{
			if(m_currMousePos->Position.X >= 0 && m_currMousePos->Position.X <= m_deviceResources->GetOutputSize().Width / 2)
			{
				float dx = m_currMousePos->Position.X - m_prevMousePos->Position.X;
				float dy = m_currMousePos->Position.Y - m_prevMousePos->Position.Y;

				XMFLOAT4 pos = XMFLOAT4(m_scene.camera[0]._41, m_scene.camera[0]._42, m_scene.camera[0]._43, m_scene.camera[0]._44);

				m_scene.camera[0]._41 = 0;
				m_scene.camera[0]._42 = 0;
				m_scene.camera[0]._43 = 0;

				XMMATRIX rotX = XMMatrixRotationX(dy * rotSpd * delta_time);
				XMMATRIX rotY = XMMatrixRotationY(dx * rotSpd * delta_time);

				XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[0]);
				temp_camera = XMMatrixMultiply(rotX, temp_camera);
				temp_camera = XMMatrixMultiply(temp_camera, rotY);

				XMStoreFloat4x4(&m_scene.camera[0], temp_camera);

				m_scene.camera[0]._41 = pos.x;
				m_scene.camera[0]._42 = pos.y;
				m_scene.camera[0]._43 = pos.z;
			}
			else if(m_currMousePos->Position.X > m_deviceResources->GetOutputSize().Width / 2 && m_currMousePos->Position.X <= m_deviceResources->GetOutputSize().Width)
			{
				float dx = m_currMousePos->Position.X - m_prevMousePos->Position.X;
				float dy = m_currMousePos->Position.Y - m_prevMousePos->Position.Y;

				XMFLOAT4 pos = XMFLOAT4(m_scene.camera[1]._41, m_scene.camera[1]._42, m_scene.camera[1]._43, m_scene.camera[1]._44);

				m_scene.camera[1]._41 = 0;
				m_scene.camera[1]._42 = 0;
				m_scene.camera[1]._43 = 0;

				XMMATRIX rotX = XMMatrixRotationX(dy * rotSpd * delta_time);
				XMMATRIX rotY = XMMatrixRotationY(dx * rotSpd * delta_time);

				XMMATRIX temp_camera = XMLoadFloat4x4(&m_scene.camera[1]);
				temp_camera = XMMatrixMultiply(rotX, temp_camera);
				temp_camera = XMMatrixMultiply(temp_camera, rotY);

				XMStoreFloat4x4(&m_scene.camera[1], temp_camera);

				m_scene.camera[1]._41 = pos.x;
				m_scene.camera[1]._42 = pos.y;
				m_scene.camera[1]._43 = pos.z;
			}
			
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

	for(size_t camera = 0; camera < m_scene.camera.size(); ++camera)
	{
		XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_scene.camera[camera]))));

		for (size_t i = 0; i < m_scene.models.size(); ++i)
			XMStoreFloat4x4(&m_scene.models[i].m_constantBufferData.view, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_scene.camera[camera]))));

		XMStoreFloat4x4(&m_scene.skybox.m_constantBufferData.view, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_scene.camera[camera]))));
		m_deviceResources->GetD3DDeviceContext()->RSSetViewports(1, &m_scene.viewports[camera]);
		DrawScene();
	}
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

void Sample3DSceneRenderer::CreateViewports(void)
{
	D3D11_VIEWPORT viewport;
	viewport.Width = m_deviceResources->GetOutputSize().Width / 2;
	viewport.Height = m_deviceResources->GetOutputSize().Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	m_scene.viewports.push_back(viewport);

	D3D11_VIEWPORT viewport2;
	viewport2.Width = m_deviceResources->GetOutputSize().Width / 2;
	viewport2.Height = m_deviceResources->GetOutputSize().Height;
	viewport2.MinDepth = 0.0f;
	viewport2.MaxDepth = 1.0f;
	viewport2.TopLeftX = m_deviceResources->GetOutputSize().Width / 2;
	viewport2.TopLeftY = 0;
	m_scene.viewports.push_back(viewport2);
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

	auto createVSModel = loadVSModel.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, &m_scene.vertexShader));

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "INST_POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		};

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &fileData[0], fileData.size(), &m_scene.inputLayout));

		D3D11_RENDER_TARGET_BLEND_DESC renderBlendDesc = { true, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL };
		D3D11_BLEND_DESC blendDesc = { true, true, renderBlendDesc };
		m_deviceResources->GetD3DDevice()->CreateBlendState(&blendDesc, &m_scene.blendState);
	});

	auto createPSModel = loadPSModel.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_scene.pixelShader));

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_scene.constantBuffer));
	});

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
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/katarina_normal.dds", nullptr, &m_scene.models[0].m_textureNormal));

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
		}
	});

	auto createAhri = (createPSModel && createVSModel).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/ahri.obj", verts, inds, 30))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/ahri.dds", nullptr, &m_scene.models[1].m_texture));
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/ahri_normal.dds", nullptr, &m_scene.models[1].m_textureNormal));

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

	auto createCubeTeste = (createPSTask && createVSTask).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/Crate.obj", verts, inds))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/TransparentGreen.dds", nullptr, &m_scene.models[2].m_texture));

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

	auto createFloor = (createPSTask && createVSTask).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/CobbleStones.obj", verts, inds))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/CobbleStones.dds", nullptr, &m_scene.models[3].m_texture));
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/CobbleStones_normal.dds", nullptr, &m_scene.models[3].m_textureNormal));

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = verts.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionUVNormal) * verts.size(), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_scene.models[3].m_vertexBuffer));

			m_scene.models[3].m_indexCount = inds.size();

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = inds.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * inds.size(), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_scene.models[3].m_indexBuffer));

			m_scene.models[3].m_instantiate = true;
			m_scene.models[3].m_instanceCount = 25;

			std::vector<INSTANCE> instances;
			float posX = 0;
			float posZ = 0;
			for(size_t x = 0; x < 5; ++x)
			{
				for (size_t z = 0; z < 5; ++z)
				{
					INSTANCE instance;
					instance.position = XMFLOAT3(posX, 0, posZ);
					instances.push_back(instance);
					posZ += 9.0f;
				}
				posZ = 0.0f;
				posX += 9.0f;
			}

			D3D11_BUFFER_DESC instanceDesc;
			instanceDesc.Usage = D3D11_USAGE_DEFAULT;
			instanceDesc.ByteWidth = sizeof(INSTANCE) * instances.size();
			instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			instanceDesc.CPUAccessFlags = 0;
			instanceDesc.MiscFlags = 0;
			instanceDesc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA instanceSubresource;
			instanceSubresource.pSysMem = instances.data();
			instanceSubresource.SysMemPitch = 0;
			instanceSubresource.SysMemSlicePitch = 0;

			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&instanceDesc, &instanceSubresource, &m_scene.models[3].m_instanceBuffer));
		}
	});

	auto createMedievalHouse = (createPSTask && createVSTask).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/Medieval_House.obj", verts, inds, 20))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/Medieval_House.dds", nullptr, &m_scene.models[4].m_texture));
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/Medieval_House_normal.dds", nullptr, &m_scene.models[4].m_textureNormal));

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = verts.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionUVNormal) * verts.size(), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_scene.models[4].m_vertexBuffer));

			m_scene.models[4].m_indexCount = inds.size();

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = inds.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * inds.size(), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_scene.models[4].m_indexBuffer));
		}
	});

	auto createTree = (createPSTask && createVSTask).then([this]()
	{
		std::vector<VertexPositionUVNormal> verts;
		std::vector<unsigned int> inds;

		if (LoadObject("Assets/tree.obj", verts, inds))
		{
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/tree.dds", nullptr, &m_scene.models[5].m_texture));
			DX::ThrowIfFailed(CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"Assets/tree_normal.dds", nullptr, &m_scene.models[5].m_textureNormal));

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = verts.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionUVNormal) * verts.size(), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_scene.models[5].m_vertexBuffer));

			m_scene.models[5].m_indexCount = inds.size();

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = inds.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * inds.size(), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_scene.models[5].m_indexBuffer));
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

void Sample3DSceneRenderer::DrawScene(void)
{
	auto context = m_deviceResources->GetD3DDeviceContext();
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
	float blendFac[] = { 1, 1, 1, 1 };
	context->OMSetBlendState(m_scene.blendState.Get(), blendFac, 0xFFFFFFFF);

	// Light mapping
	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	m_deviceResources->GetD3DDeviceContext()->Map(m_scene.lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	LIGHT *light;
	light = (LIGHT*)mappedSubresource.pData;
	light->position = XMFLOAT4(m_lightDirection.x, m_lightDirection.y, m_lightDirection.z, m_scene.lightType);
	light->color = XMFLOAT4(1, 1, 1, 0);
	light->coneDirection = XMFLOAT4(-1, 3.0f, 1.0f, 0.8f);
	m_deviceResources->GetD3DDeviceContext()->Unmap(m_scene.lightBuffer, 0);
	m_deviceResources->GetD3DDeviceContext()->PSSetConstantBuffers(0, 1, &m_scene.lightBuffer);

	//D3D11_MAPPED_SUBRESOURCE mappedSubresource2;
	//m_deviceResources->GetD3DDeviceContext()->Map(m_scene.lightBuffer2, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource2);
	//LIGHT *light2;
	//light2 = (LIGHT*)mappedSubresource2.pData;
	//light2->position = XMFLOAT4(0, 0, 0, 4.0f);
	//light2->color = XMFLOAT4(1, 1, 1, 0);
	//light2->coneDirection = XMFLOAT4(-1, 3.0f, 1.0f, 0.8f);
	//m_deviceResources->GetD3DDeviceContext()->Unmap(m_scene.lightBuffer2, 0);
	//m_deviceResources->GetD3DDeviceContext()->PSSetConstantBuffers(0, 1, &m_scene.lightBuffer2);

	// Models renderer
	for (size_t i = 0; i < m_scene.models.size(); ++i)
	{
		ID3D11ShaderResourceView *shaderResourceView[] = { m_scene.models[i].m_texture, m_scene.models[i].m_textureNormal };
		context->PSSetShaderResources(1, 2, shaderResourceView);

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

		if (m_scene.models[i].m_render)
			context->DrawIndexed(m_scene.models[i].m_indexCount, 0, 0);

		if (m_scene.models[i].m_instantiate)
		{
			ID3D11ShaderResourceView *instaceShaderResourceView[] = { m_scene.models[i].m_texture };
			context->PSSetShaderResources(1, 1, instaceShaderResourceView);
			UINT strides[2];
			UINT offsets[2];
			Microsoft::WRL::ComPtr<ID3D11Buffer> bufferPointers[2];
			strides[0] = sizeof(VertexPositionUVNormal);
			strides[1] = sizeof(INSTANCE);
			offsets[0] = 0;
			offsets[1] = 0;

			bufferPointers[0] = m_scene.models[i].m_vertexBuffer;
			bufferPointers[1] = m_scene.models[i].m_instanceBuffer;

			context->UpdateSubresource1(m_scene.constantBuffer.Get(), 0, NULL, &m_scene.models[i].m_constantBufferData, 0, 0, 0);
			context->IASetVertexBuffers(0, 2, bufferPointers->GetAddressOf(), strides, offsets);
			context->IASetIndexBuffer(m_scene.models[i].m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			context->IASetInputLayout(m_scene.inputLayout.Get());
			context->DrawIndexedInstanced(m_scene.models[i].m_indexCount, m_scene.models[i].m_instanceCount, 0, 0, 0);
		}
	}

	// Skybox renderer
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