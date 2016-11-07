#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"
#include <vector>
#include <atlbase.h>

namespace DX11UWA
{
	// This sample renderer instantiates a basic rendering pipeline.
	class Sample3DSceneRenderer
	{
	public:
		Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);

		bool LoadObject(const char *_path, std::vector<VertexPositionUVNormal> &_verts, std::vector<unsigned int> &_inds, const float resizeFactor);

		void CreateDeviceDependentResources(void);
		void CreateWindowSizeDependentResources(void);
		void ReleaseDeviceDependentResources(void);
		void Update(DX::StepTimer const& timer);
		void Render(void);
		void StartTracking(void);
		void TrackingUpdate(float positionX);
		void StopTracking(void);
		inline bool IsTracking(void) { return m_tracking; }

		// Helper functions for keyboard and mouse input
		void SetKeyboardButtons(const char* list);
		void SetMousePosition(const Windows::UI::Input::PointerPoint^ pos);
		void SetInputDeviceData(const char* kb, const Windows::UI::Input::PointerPoint^ pos);


	private:
		void TranslateAndRotate(ModelViewProjectionConstantBuffer &objectM, DirectX::XMFLOAT3 pos, float radians);
		void Rotate(ModelViewProjectionConstantBuffer &objectM, float radians);
		void Static(ModelViewProjectionConstantBuffer &objectM, DirectX::XMFLOAT3 pos);
		void StaticSkybox(ModelViewProjectionConstantBuffer &objectM, DirectX::XMFLOAT3 pos);
		void Orbit(ModelViewProjectionConstantBuffer &objectM, DirectX::XMFLOAT3 radians, DirectX::XMFLOAT3 orbitpos, DirectX::XMFLOAT3 orbitness);
		void UpdateCamera(DX::StepTimer const& timer, float const moveSpd, float const rotSpd);

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		struct INSTANCE
		{
			DirectX::XMFLOAT4 position;
		};
		uint32 m_instanceCount;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_instanceBuffer;

		struct MODEL
		{
			ModelViewProjectionConstantBuffer		m_constantBufferData;
			Microsoft::WRL::ComPtr<ID3D11Buffer>	m_vertexBuffer;
			Microsoft::WRL::ComPtr<ID3D11Buffer>	m_indexBuffer;
			CComPtr<ID3D11ShaderResourceView>		m_texture;
			uint32									m_indexCount;
			DirectX::XMFLOAT3						m_position;
			bool									m_render;

			MODEL()
			{
				m_position = DirectX::XMFLOAT3(0, 0, 0);
				m_render = true;
			}
		};
		std::vector<MODEL> models;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_modelInputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_modelVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_modelPixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_modelConstantBuffer;

		// SKYBOX VARIABLES
		ModelViewProjectionConstantBuffer			m_skyboxConstantBufferData;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_skyboxVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_skyboxIndexBuffer;
		CComPtr<ID3D11ShaderResourceView>			m_skyboxTexture;
		uint32										m_skyboxIndexCount;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_skyboxVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_skyboxPixelShader;
		// END SKYBOX

		// CUBE
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_constantBuffer;
		// END CUBE

		// Texture variables
		CComPtr<ID3D11ShaderResourceView>	m_texture;

		// System resources for cube geometry.
		ModelViewProjectionConstantBuffer	m_constantBufferData;
		uint32	m_indexCount;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		float	m_degreesPerSecond;
		bool	m_tracking;

		// Data members for keyboard and mouse input
		char	m_kbuttons[256];
		Windows::UI::Input::PointerPoint^ m_currMousePos;
		Windows::UI::Input::PointerPoint^ m_prevMousePos;

		// Matrix data member for the camera
		DirectX::XMFLOAT4X4 m_camera;
	};
}

