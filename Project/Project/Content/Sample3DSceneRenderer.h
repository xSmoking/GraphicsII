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

		void CreateViewports(void);
		void CreateDeviceDependentResources(void);
		void CreateWindowSizeDependentResources(void);
		void ReleaseDeviceDependentResources(void);
		void DrawScene(void);
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
		void Translate(ModelViewProjectionConstantBuffer &objectM, DirectX::XMFLOAT3 pos);
		void StaticSkybox(ModelViewProjectionConstantBuffer &objectM, DirectX::XMFLOAT3 pos);
		void Orbit(ModelViewProjectionConstantBuffer &objectM, DirectX::XMFLOAT3 radians, DirectX::XMFLOAT3 orbitpos, DirectX::XMFLOAT3 orbitness);
		void UpdateCamera(DirectX::XMFLOAT4X4 camera, DX::StepTimer const& timer, float const moveSpd, float const rotSpd);

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		struct LIGHT
		{
			DirectX::XMFLOAT4	color;
			DirectX::XMFLOAT4	position;
			DirectX::XMFLOAT4	coneDirection;
		};

		struct INSTANCE
		{
			DirectX::XMFLOAT3 position;
		};

		struct MODEL
		{
			ModelViewProjectionConstantBuffer		m_constantBufferData;
			Microsoft::WRL::ComPtr<ID3D11Buffer>	m_vertexBuffer;
			Microsoft::WRL::ComPtr<ID3D11Buffer>	m_indexBuffer;
			CComPtr<ID3D11ShaderResourceView>		m_texture;
			uint32									m_indexCount;
			DirectX::XMFLOAT3						m_position;
			bool									m_render;
			bool									m_instantiate;
			uint32									m_instanceCount;
			Microsoft::WRL::ComPtr<ID3D11Buffer>	m_instanceBuffer;

			MODEL()
			{
				m_position = DirectX::XMFLOAT3(0, 0, 0);
				m_render = true;
				m_instantiate = false;
			}
		};
		
		struct SKYBOX
		{
			ModelViewProjectionConstantBuffer			m_constantBufferData;
			Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
			Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
			CComPtr<ID3D11ShaderResourceView>			m_texture;
			uint32										m_indexCount;
			Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
			Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
		};

		struct SCENE
		{
			std::vector<MODEL>							models;
			SKYBOX										skybox;
			Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
			Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
			Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
			Microsoft::WRL::ComPtr<ID3D11Buffer>		constantBuffer;
			std::vector<D3D11_VIEWPORT>					viewports;
			float										lightType;
			LIGHT										*light;
			ID3D11Buffer								*lightBuffer;
			std::vector<DirectX::XMFLOAT4X4>			camera;
		};
		SCENE m_scene;
		
		// CUBE
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_constantBuffer;
		ModelViewProjectionConstantBuffer			m_constantBufferData;
		uint32										m_indexCount;
		// END CUBE

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

