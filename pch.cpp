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



void avx2_memcpy(BYTE* dst, BYTE* src, size_t size)
{
	if (dst != src) {
#define isAligned(address) ((address&0x1F)==0)
		if (isAligned((uint64_t)src) && isAligned((uint64_t)dst)) {
			const __m256i* s = reinterpret_cast<const __m256i*>(src);
			__m256i* dest = reinterpret_cast<__m256i*>(dst);
			int64_t vectors = size / sizeof(*s);
			int64_t residual = size % sizeof(*s);
			uint64_t vectors_copied = 0;
			for (; vectors > 0; vectors--, s++, dest++) {
				const __m256i loaded = _mm256_stream_load_si256(s);
				_mm256_stream_si256(dest, loaded);
				vectors_copied++;
			}
			if (residual != 0) {
				uint64_t offset = vectors_copied * sizeof(*s);
				memcpy(dst + offset, src + offset, size - offset);
			}
			_mm_sfence();
		}
		else {
			memcpy(dst, src, size);
		}
	}
}

class SimpleDXGI {
public:
	SimpleDXGI(HWND);
	~SimpleDXGI();
	BYTE* CaptureWindow(BYTE*, int, int, int, int);
	BYTE* grabByRegion(BYTE* buffer, UINT left, UINT top, UINT width, UINT height);
private:
	winrt::Windows::Graphics::Capture::GraphicsCaptureSession session = nullptr;
	// Create Direct 3D Device
	winrt::com_ptr<IDXGIFactory2> factory;
	winrt::com_ptr<IDXGIAdapter> adapter;
	winrt::com_ptr<ID3D11Device> d3dDevice = nullptr;
	winrt::com_ptr<ID3D11DeviceContext> d3dContext = nullptr;

	winrt::com_ptr<ID3D11Texture2D> texture = nullptr;
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
	winrt::init_apartment(winrt::apartment_type::multi_threaded);
	//SetProcessDPIAware();


	winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr, 0, D3D11_SDK_VERSION, d3dDevice.put(), nullptr, nullptr));

	winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device;
	const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
	{
		winrt::com_ptr<::IInspectable> inspectable;
		winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
		inspectable.as(device);
	}

	auto idxgiDevice2 = dxgiDevice.as<IDXGIDevice2>();
	winrt::check_hresult(idxgiDevice2->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
	winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

	d3dDevice->GetImmediateContext(d3dContext.put());

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
	winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem(nullptr);
	
	interopFactory->CreateForWindow(hwndTarget,
		winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
		winrt::put_abi(captureItem));

	session = m_framePool.CreateCaptureSession(captureItem);
	m_framePool.FrameArrived([&](auto& framePool, auto&) {
		//if (isFrameArrived) return;
		auto frame = framePool.TryGetNextFrame();
		auto access = frame.Surface().as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();

		//texture = nullptr;
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

BYTE* SimpleDXGI::CaptureWindow(BYTE* buffer, int left, int top, int width, int height)
{	
	while (!isFrameArrived || nullptr == texture)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	isFrameArrived = false;
	auto pData = grabByRegion(buffer, left, top, width, height);
	return pData;
}

bool safe_get_texture_desc(winrt::com_ptr<ID3D11Texture2D>* texture, D3D11_TEXTURE2D_DESC* desc) {
	__try {
		(*texture)->GetDesc(desc);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
	return true;
}

BYTE* SimpleDXGI::grabByRegion(BYTE* buffer, UINT left, UINT top, UINT width, UINT height)
{
	D3D11_TEXTURE2D_DESC capturedTextureDesc;
	while (!safe_get_texture_desc(&texture, &capturedTextureDesc)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	capturedTextureDesc.Usage = D3D11_USAGE_STAGING;
	capturedTextureDesc.BindFlags = 0;
	capturedTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	capturedTextureDesc.MiscFlags = 0;
	capturedTextureDesc.Width = width;
	capturedTextureDesc.Height = height;


	winrt::com_ptr<ID3D11Texture2D> userTexture = nullptr;
	winrt::check_hresult(d3dDevice->CreateTexture2D(&capturedTextureDesc, NULL, userTexture.put()));

	D3D11_BOX sourceRegion;
	sourceRegion.left = left;
	sourceRegion.top = top;
	sourceRegion.right = left + width;
	sourceRegion.bottom = top + height;
	sourceRegion.front = 0;
	sourceRegion.back = 1;
	d3dContext->CopySubresourceRegion(userTexture.get(), 0, 0, 0, 0, texture.get(), 0, &sourceRegion);

	D3D11_MAPPED_SUBRESOURCE resource;
	winrt::check_hresult(d3dContext->Map(userTexture.get(), NULL, D3D11_MAP_READ, 0, &resource));
	d3dContext->Unmap(userTexture.get(), NULL);

	UINT lBmpRowPitch = width * 4;
	auto sptr = static_cast<BYTE*>(resource.pData);
	auto dptr = static_cast<BYTE*>(buffer);

	UINT lRowPitch = std::min<UINT>(lBmpRowPitch, resource.RowPitch);

	for (size_t h = 0; h < height; ++h)
	{
		avx2_memcpy(dptr, sptr, lRowPitch);
		sptr += resource.RowPitch;
		dptr += lBmpRowPitch;
	}

	return buffer;
}



SimpleDXGI* dxgi;

void init_dxgi(HWND hwnd)
{
	dxgi = new SimpleDXGI(hwnd);
}

BYTE* grab(BYTE* buffer, int left, int top, int width, int height)
{
	if (dxgi) {
		return dxgi->CaptureWindow(buffer, left, top, width, height);
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
