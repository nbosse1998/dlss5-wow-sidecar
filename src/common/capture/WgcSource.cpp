#include "capture/WgcSource.h"

#include <unknwn.h>  // Classic COM GUID support must precede C++/WinRT.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <d3d11_4.h>
#include <dwmapi.h>
#include <wrl/client.h>

#include "gpu/DeviceBridge.h"
#include "core/Log.h"

using Microsoft::WRL::ComPtr;
namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgdx = winrt::Windows::Graphics::DirectX;

namespace sidecar {

struct WgcSource::Impl {
  wgc::GraphicsCaptureItem item{nullptr};
  wgc::Direct3D11CaptureFramePool pool{nullptr};
  wgc::GraphicsCaptureSession session{nullptr};
  winrt::event_token frameToken{};
  winrt::event_token closedToken{};
  DeviceBridge* bridge = nullptr;
  WgcSource::DropCallback onDrop;
  WgcSource* owner = nullptr;
  bool clientAreaOnly = false;
  UINT width = 0, height = 0;
  ComPtr<ID3D11Texture2D> clientTexture;
};

namespace {

wgdx::Direct3D11::IDirect3DDevice WrapDevice(ID3D11Device* dev) {
  ComPtr<IDXGIDevice> dxgi;
  if (FAILED(dev->QueryInterface(IID_PPV_ARGS(&dxgi)))) return nullptr;
  winrt::com_ptr<::IInspectable> inspectable;
  if (FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), inspectable.put()))) return nullptr;
  return inspectable.as<wgdx::Direct3D11::IDirect3DDevice>();
}

ComPtr<ID3D11Texture2D> SurfaceToTexture(
    const wgdx::Direct3D11::IDirect3DSurface& surface) {
  auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  ComPtr<ID3D11Texture2D> tex;
  access->GetInterface(IID_PPV_ARGS(&tex));
  return tex;
}

}  // namespace

std::unique_ptr<WgcSource> WgcSource::CreateForWindow(HWND target,
                                                      DeviceBridge& bridge,
                                                      DropCallback onDrop, bool clientAreaOnly) {
  if (!wgc::GraphicsCaptureSession::IsSupported()) return nullptr;

  auto interop = winrt::get_activation_factory<wgc::GraphicsCaptureItem>()
                     .as<IGraphicsCaptureItemInterop>();
  wgc::GraphicsCaptureItem item{nullptr};
  if (FAILED(interop->CreateForWindow(
          target, winrt::guid_of<wgc::GraphicsCaptureItem>(), winrt::put_abi(item)))) {
    return nullptr;
  }

  auto device = WrapDevice(bridge.D3d11());
  if (!device) return nullptr;

  std::unique_ptr<WgcSource> s(new WgcSource());
  s->target_ = target;
  s->clientAreaOnly_ = clientAreaOnly;
  s->impl_ = std::make_unique<Impl>();
  s->impl_->owner = s.get();
  s->impl_->bridge = &bridge;
  s->impl_->onDrop = std::move(onDrop);
  s->impl_->item = item;
  s->impl_->clientAreaOnly = clientAreaOnly;
  RECT client{};
  if (!GetClientRect(target, &client)) return nullptr;
  s->impl_->width = static_cast<UINT>(client.right);
  s->impl_->height = static_cast<UINT>(client.bottom);

  // Free-threaded so frames arrive on a pool thread rather than needing a
  // message loop on the capture thread.
  s->impl_->pool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
      device, wgdx::DirectXPixelFormat::B8G8R8A8UIntNormalized,
      static_cast<int32_t>(DeviceBridge::kRingDepth), item.Size());

  Impl* impl = s->impl_.get();
  impl->frameToken = impl->pool.FrameArrived(
      [impl](const wgc::Direct3D11CaptureFramePool& pool, auto&&) {
        if (impl->owner->closed_.load(std::memory_order_acquire)) return;
        auto frame = pool.TryGetNextFrame();
        if (!frame) return;
        impl->owner->lastFrameTick_.store(GetTickCount64(), std::memory_order_relaxed);
        auto tex = SurfaceToTexture(frame.Surface());
        if (!tex) return;
        if (impl->clientAreaOnly) {
          // WGC captures the DWM window bounds, including a windowed projector's
          // caption. Crop to its client area before the existing fixed-size ring.
          // Size changes stop capture; restarting creates all GPU resources anew.
          RECT client{}, bounds{};
          POINT origin{};
          const auto size = frame.ContentSize();
          D3D11_TEXTURE2D_DESC desc{};
          tex->GetDesc(&desc);
          if (IsIconic(impl->owner->target_) ||
              !GetClientRect(impl->owner->target_, &client) ||
              !ClientToScreen(impl->owner->target_, &origin) ||
              FAILED(DwmGetWindowAttribute(impl->owner->target_, DWMWA_EXTENDED_FRAME_BOUNDS,
                                           &bounds, sizeof(bounds))) ||
              client.right != static_cast<LONG>(impl->width) ||
              client.bottom != static_cast<LONG>(impl->height) ||
              size.Width != bounds.right - bounds.left ||
              size.Height != bounds.bottom - bounds.top) {
            GlobalLog().Warn("OBS projector geometry changed or is unavailable; restart capture after restoring it");
            impl->owner->closed_.store(true, std::memory_order_release);
            return;
          }
          const LONG x = origin.x - bounds.left;
          const LONG y = origin.y - bounds.top;
          if (x < 0 || y < 0 || x + client.right > size.Width ||
              y + client.bottom > size.Height || size.Width > static_cast<LONG>(desc.Width) ||
              size.Height > static_cast<LONG>(desc.Height)) {
            GlobalLog().Warn("OBS client crop is outside the captured frame; restart or try a fullscreen projector");
            impl->owner->closed_.store(true, std::memory_order_release);
            return;
          }
          if (!impl->clientTexture) {
            desc.Width = impl->width;
            desc.Height = impl->height;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            if (FAILED(impl->bridge->D3d11()->CreateTexture2D(&desc, nullptr,
                                                           &impl->clientTexture))) {
              GlobalLog().Warn("could not allocate OBS client-area capture texture");
              impl->owner->closed_.store(true, std::memory_order_release);
              return;
            }
          }
          D3D11_BOX box{static_cast<UINT>(x), static_cast<UINT>(y), 0,
                        static_cast<UINT>(x + client.right),
                        static_cast<UINT>(y + client.bottom), 1};
          impl->bridge->D3d11Context()->CopySubresourceRegion(impl->clientTexture.Get(), 0, 0, 0, 0,
                                         tex.Get(), 0, &box);
          tex = impl->clientTexture;
        }
        const bool dropped = impl->bridge->Publish(tex.Get());
        impl->owner->delivered_.fetch_add(1, std::memory_order_relaxed);
        if (dropped && impl->onDrop) impl->onDrop();
      });

  impl->closedToken = item.Closed([impl](auto&&, auto&&) {
    impl->owner->closed_.store(true, std::memory_order_release);
  });

  s->impl_->session = s->impl_->pool.CreateCaptureSession(item);
  s->impl_->session.IsCursorCaptureEnabled(false);   // WoW draws its own cursor
  s->impl_->session.IsBorderRequired(false);         // no yellow capture border
  return s;
}

WgcSource::~WgcSource() { Stop(); }

bool WgcSource::IsClosed() const {
  if (closed_.load(std::memory_order_acquire)) return true;
  if (clientAreaOnly_ && GetTickCount64() - lastFrameTick_.load(std::memory_order_relaxed) > 5000)
    return true;
  // WGC raises Closed when a window is destroyed normally, but not when the
  // owning process is terminated -- the frames simply stop. Ask the window
  // manager directly rather than waiting for an event that will not arrive.
  return target_ != nullptr && !IsWindow(target_);
}

void WgcSource::Start() {
  lastFrameTick_.store(GetTickCount64(), std::memory_order_relaxed);
  if (impl_ && impl_->session) impl_->session.StartCapture();
}

void WgcSource::Stop() {
  if (!impl_) return;
  if (impl_->pool && impl_->frameToken) {
    impl_->pool.FrameArrived(impl_->frameToken);
    impl_->frameToken = {};
  }
  if (impl_->item && impl_->closedToken) {
    impl_->item.Closed(impl_->closedToken);
    impl_->closedToken = {};
  }
  if (impl_->session) { impl_->session.Close(); impl_->session = nullptr; }
  if (impl_->pool) { impl_->pool.Close(); impl_->pool = nullptr; }
}

}  // namespace sidecar
