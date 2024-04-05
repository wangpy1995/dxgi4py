// pch.cpp: source file corresponding to the pre-compiled header

#include "pch.h"

// When you are using pre-compiled headers, this source file is necessary for compilation to succeed.
#include <dxgi1_2.h>
#include <d3d11.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <roerrorapi.h>
#include <shlobj_core.h>
#include <dwmapi.h>
#pragma comment(lib,"Dwmapi.lib")
#pragma comment(lib,"windowsapp.lib")

class SimpleDXGI {
public:
	SimpleDXGI(HWND);
	~SimpleDXGI();
	BYTE* CaptureWindow(BYTE*, int, int, int, int);
	BYTE* grabByRegion(BYTE* buffer, int left, int top, int right, int bottom);
private:
	winrt::Windows::Graphics::Capture::GraphicsCaptureSession session = nullptr;
	// Create Direct 3D Device
	winrt::com_ptr<ID3D11Device> d3dDevice;
	ID3D11DeviceContext* d3dContext = nullptr;
	winrt::com_ptr<ID3D11Texture2D> texture;
	bool isFrameArrived = false;
	HWND hwndTarget;
	void InitSession();
	void CloseSession();
};

SimpleDXGI::SimpleDXGI(HWND hwnd)
{
	hwndTarget = hwnd;
	isFrameArrived = false;
	InitSession();
}

SimpleDXGI::~SimpleDXGI()
{
	CloseSession();
}

void SimpleDXGI::InitSession()
{
	// Init COM
	winrt::init_apartment(winrt::apartment_type::single_threaded);
	//SetProcessDPIAware();


	winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr, 0, D3D11_SDK_VERSION, d3dDevice.put(), nullptr, nullptr));

	winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device;
	const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
	{
		winrt::com_ptr<::IInspectable> inspectable;
		winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
		device = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
	}

	auto idxgiDevice2 = dxgiDevice.as<IDXGIDevice2>();
	winrt::com_ptr<IDXGIAdapter> adapter;
	winrt::check_hresult(idxgiDevice2->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
	winrt::com_ptr<IDXGIFactory2> factory;
	winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

	d3dDevice->GetImmediateContext(&d3dContext);

	RECT rect{};
	DwmGetWindowAttribute(hwndTarget, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));
	const auto size = winrt::Windows::Graphics::SizeInt32{ rect.right - rect.left, rect.bottom - rect.top };

	winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool =
		winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
			device,
			winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
			1,
			size);

	const auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
	auto interopFactory = activationFactory.as<IGraphicsCaptureItemInterop>();
	winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem = { nullptr };
	interopFactory->CreateForWindow(hwndTarget,
		winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
		reinterpret_cast<void**>(winrt::put_abi(captureItem)));

	session = m_framePool.CreateCaptureSession(captureItem);
	m_framePool.FrameArrived([&](winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool, auto&)
		{
			//if (isFrameArrived) return;
			
			winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame = framePool.TryGetNextFrame();

			struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
				IDirect3DDxgiInterfaceAccess : ::IUnknown
			{
				virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
			};
			auto access = frame.Surface().as<IDirect3DDxgiInterfaceAccess>();
			texture = nullptr;
			access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), texture.put_void());
			isFrameArrived = true;
			return;
		});


	session.IsCursorCaptureEnabled(false);
	//session.IsBorderRequired(false);
	session.StartCapture();
}

void SimpleDXGI::CloseSession()
{
	if (session != nullptr) {
		session.Close();
		session = nullptr;
	}
}

BYTE* SimpleDXGI::CaptureWindow(BYTE* buffer, int left, int top, int right, int bottom)
{
	// Message pump
	MSG msg;
	while (!isFrameArrived)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
			DispatchMessage(&msg);
		}
		Sleep(1);
	}
	//isFrameArrived = false;
	auto pData = grabByRegion(buffer, left, top, right, bottom);
	return pData;
}

BYTE* SimpleDXGI::grabByRegion(BYTE* buffer, int left, int top, int right, int bottom)
{
	D3D11_TEXTURE2D_DESC capturedTextureDesc;
	texture->GetDesc(&capturedTextureDesc);
	//LONG width = std::min<UINT>(capturedTextureDesc.Width, right - left);
	//LONG height = std::min<UINT>(capturedTextureDesc.Height, bottom - top);

	capturedTextureDesc.Usage = D3D11_USAGE_STAGING;
	capturedTextureDesc.BindFlags = 0;
	capturedTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	capturedTextureDesc.MiscFlags = 0;
	capturedTextureDesc.Width = right - left;
	capturedTextureDesc.Height = bottom - top;


	winrt::com_ptr<ID3D11Texture2D> userTexture = nullptr;
	winrt::check_hresult(d3dDevice->CreateTexture2D(&capturedTextureDesc, NULL, userTexture.put()));


	LONG width = capturedTextureDesc.Width;
	LONG height = capturedTextureDesc.Height;


	D3D11_BOX sourceRegion;
	sourceRegion.left = left;
	sourceRegion.top = top;
	sourceRegion.right = right;
	sourceRegion.bottom = bottom;
	sourceRegion.front = 0;
	sourceRegion.back = 1;
	d3dContext->CopySubresourceRegion(userTexture.get(), 0, 0, 0, 0, texture.get(), 0, &sourceRegion);

	D3D11_MAPPED_SUBRESOURCE resource;
	winrt::check_hresult(d3dContext->Map(userTexture.get(), NULL, D3D11_MAP_READ, 0, &resource));

	//D3D11_QUERY_DESC queryDesc;
	//queryDesc.Query = D3D11_QUERY_EVENT;
	//queryDesc.MiscFlags = NULL;
	//ID3D11Query* pQuery;
	//winrt::check_hresult(d3dDevice->CreateQuery(&queryDesc, &pQuery));
	//d3dContext->Flush();
	//d3dContext->End(pQuery);
	//BOOL queryData;
	//while (S_OK != winrt::check_hresult(d3dContext->GetData(pQuery, &queryData, sizeof(BOOL), 0)))
	//{

	//}

	UINT lBmpRowPitch = width * 4;
	auto sptr = static_cast<BYTE*>(resource.pData);
	auto dptr = static_cast<BYTE*>(buffer);

	UINT lRowPitch = std::min<UINT>(lBmpRowPitch, resource.RowPitch);

	for (size_t h = 0; h < height; ++h)
	{
		memcpy_s(dptr, lBmpRowPitch, sptr, lRowPitch);
		sptr += resource.RowPitch;
		//sptr -= resource.RowPitch;
		//dptr -= lBmpRowPitch;
		dptr += lBmpRowPitch;
	}
	d3dContext->Unmap(userTexture.get(), NULL);

	return buffer;
}



SimpleDXGI* dxgi;

void init_dxgi(HWND hwnd)
{
	dxgi = new SimpleDXGI(hwnd);
}

BYTE* grab(BYTE* buffer, int left, int top, int right, int bottom)
{
	if (dxgi) {
		return dxgi->CaptureWindow(buffer, left, top, right, bottom);
	}
	return  nullptr;
}

void destroy()
{
	if (dxgi) {
		delete dxgi;
		dxgi = nullptr;
	}
}
