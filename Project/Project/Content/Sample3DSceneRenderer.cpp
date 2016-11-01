#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "..\Common\DirectXHelper.h"

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
	float Oradians = 1;
	if (!m_tracking)
	{
		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
		double totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));
		//Oradians = radians;
		//Orbit(m_constantBufferData, XMFLOAT3(0, 0, radians), XMFLOAT3(0, 0, 0), XMFLOAT3(3, 8, 0));

		Rotate(radians);
	}

	// Update or move camera here
	UpdateCamera(timer, 1.0f, 0.75f);

}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::Rotate(float radians)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radians)));
}

void Sample3DSceneRenderer::Static(ModelViewProjectionConstantBuffer &objectM, XMFLOAT3 pos)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&objectM.model, XMMatrixTranspose(XMMatrixTranslation(pos.x, pos.y, pos.z)));
}

void Sample3DSceneRenderer::Orbit(ModelViewProjectionConstantBuffer &objectM, XMFLOAT3 radians, XMFLOAT3 orbitpos, XMFLOAT3 orbitness)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&objectM.model, XMMatrixTranspose(XMMatrixTranslation(orbitness.x, orbitness.y, orbitness.z) * (XMMatrixRotationX(radians.x) * XMMatrixRotationY(radians.y) * XMMatrixRotationZ(radians.z)) * XMMatrixTranslation(orbitpos.x, orbitpos.y, orbitpos.z)));
}

void Sample3DSceneRenderer::UpdateCamera(DX::StepTimer const& timer, float const moveSpd, float const rotSpd)
{
	const float delta_time = (float)timer.GetElapsedSeconds();

	float moveSpeed = moveSpd;

	if (m_kbuttons['W'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, 0.0f, moveSpeed * delta_time);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['S'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, 0.0f, -moveSpeed * delta_time);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['A'])
	{
		XMMATRIX translation = XMMatrixTranslation(-moveSpeed * delta_time, 0.0f, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['D'])
	{
		XMMATRIX translation = XMMatrixTranslation(moveSpeed * delta_time, 0.0f, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons['X'])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, -moveSpeed * delta_time, 0.0f);
		XMMATRIX temp_camera = XMLoadFloat4x4(&m_camera);
		XMMATRIX result = XMMatrixMultiply(translation, temp_camera);
		XMStoreFloat4x4(&m_camera, result);
	}
	if (m_kbuttons[VK_SPACE])
	{
		XMMATRIX translation = XMMatrixTranslation(0.0f, moveSpeed * delta_time, 0.0f);
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
		Rotate(radians);
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
	context->DrawIndexed(m_indexCount, 0, 0);

	for (unsigned int i = 0; i < 2; ++i)
	{
		m_models[i].m_constantBufferData.view = m_constantBufferData.view;
		m_models[i].m_constantBufferData.projection = m_constantBufferData.projection;
		//m_models[i].m_constantBufferData.eyepos = m_constantBufferData.eyepos;
		context->UpdateSubresource1(m_constantBuffer.Get(), 0, NULL, &m_models[i].m_constantBufferData, 0, 0, 0);

		stride = sizeof(VertexPositionColor);
		offset = 0;
		context->IASetVertexBuffers(0, 1, m_models[i].m_vertexBuffer.GetAddressOf(), &stride, &offset);

		context->IASetIndexBuffer(m_models[i].m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->VSSetConstantBuffers1(0, 1, m_constantBuffer.GetAddressOf(), nullptr, nullptr);
		//context->PSSetShaderResources(0, 1, m_models[i].m_texture.GetAddressOf());
		//context->PSSetShaderResources(1, 1, m_models[i].m_normalMap.GetAddressOf());

		context->DrawIndexed(m_models[i].m_indexCount, 0, 0);
	}
}

bool Sample3DSceneRenderer::LoadObject(const char *_path, std::vector<XMFLOAT3> &_vertices, std::vector<XMFLOAT2> &_uvs, std::vector<XMFLOAT3> &_normals, std::vector<unsigned short> &_vertexIndices, std::vector<unsigned short> &_uvIndices, std::vector<unsigned short> &_normalIndices)
{
	std::vector<unsigned short> vertexIndices, uvIndices, normalIndices;
	std::vector<XMFLOAT3> vertices_vector;
	std::vector<XMFLOAT2> uvs_vector;
	std::vector<XMFLOAT3> normals_vector;

	std::ifstream file;
	file.open(_path);

	if (file.is_open())
	{
		while (true)
		{
			//char buffer[128];
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
				vertices_vector.push_back(vertex);
				XMFLOAT3 color(0, 1, 1);
				vertices_vector.push_back(color);
			}
			else if (type == "vt")
			{
				XMFLOAT2 uv;
				stream >> uv.x >> uv.y;
				uvs_vector.push_back(uv);
			}
			else if (type == "vn")
			{
				XMFLOAT3 normal;
				stream >> normal.x >> normal.y >> normal.z;
				normals_vector.push_back(normal);
			}
			else if (type == "f")
			{
				std::string buffer2;
				int count = 0;
				for (size_t i = 0; i < buffer.length(); ++i)
				{
					if (buffer[i] != 'f' && buffer[i] != '/' &&buffer[i] != ' ')
					{
						buffer2 += buffer[i];
					}
					else
					{
						if (buffer2.length() > 0)
						{
							if (count == 0)
								vertexIndices.push_back(stoi(buffer2));
							else if (count == 1)
								uvIndices.push_back(stoi(buffer2));
							else if (count == 2)
								normalIndices.push_back(stoi(buffer2));

							count++;
							if (count == 3)
								count = 0;
							buffer2.erase();
						}
					}
				}
			}
		}

		file.close();
	}
	else
		return false;

	vertexIndices.resize(vertexIndices.size());
	for (unsigned int i = 0; i < vertexIndices.size(); i++)
	{
		unsigned int vIndex = vertexIndices[i];
		unsigned int vertex = vIndex - 1;
		vertexIndices[i] = vertex;
	}

	_vertices = vertices_vector;
	_uvs = uvs_vector;
	_normals = normals_vector;
	_vertexIndices = vertexIndices;
	_uvIndices = uvIndices;
	_normalIndices = normalIndices;

	return true;
}

void Sample3DSceneRenderer::CreateDeviceDependentResources(void)
{
	// Load shaders asynchronously.
	auto loadVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso");

	// After the vertex shader file is loaded, create the shader and input layout.
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

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, &m_pixelShader));

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer));
	});

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this]()
	{
		std::string str;
		std::vector<unsigned short> vertexIndices, uvIndices, normalIndices;
		std::vector<XMFLOAT3> vertices_vector;
		std::vector<XMFLOAT2> uvs_vector;
		std::vector<XMFLOAT3> normals_vector;

		if (LoadObject("Assets/1911.obj", vertices_vector, uvs_vector, normals_vector, vertexIndices, uvIndices, normalIndices))
		{
			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = vertices_vector.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(XMFLOAT3) * vertices_vector.size(), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_vertexBuffer));

			m_indexCount = vertexIndices.size();

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = vertexIndices.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * vertexIndices.size(), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_indexBuffer));
		}
	});

	// Once the cube is loaded, the object is ready to be rendered.
	createCubeTask.then([this]()
	{
		m_loadingComplete = true;
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