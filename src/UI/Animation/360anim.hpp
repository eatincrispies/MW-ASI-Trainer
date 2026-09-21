#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class Xbox360Anim {
public:
    explicit Xbox360Anim(HMODULE resources) noexcept;

    Xbox360Anim(const Xbox360Anim&)            = delete;
    Xbox360Anim& operator=(const Xbox360Anim&) = delete;

    void Trigger() noexcept;
    [[nodiscard]] bool Render(IDirect3DDevice9* device) noexcept;

private:
    struct ComRelease {
        void operator()(IUnknown* object) const noexcept { object->Release(); }
    };

    template <class T>
    using ComPtr = std::unique_ptr<T, ComRelease>;

    [[nodiscard]] bool Build(IDirect3DDevice9* device, UINT width, UINT height) noexcept;
    [[nodiscard]] bool Compose(int width) noexcept;
    [[nodiscard]] bool Finish() noexcept;
    void Advance() noexcept;
    void Draw(IDirect3DDevice9* device, IDirect3DSurface9* backBuffer, int width, float alpha) const noexcept;

    HMODULE                    resources_ = nullptr;
    std::atomic<bool>          triggered_{ false };
    bool                       finished_ = false;
    bool                       chimed_   = false;
    IDirect3DDevice9*          device_   = nullptr;
    ComPtr<IDirect3DTexture9>  texture_;
    std::vector<std::uint32_t> inner_;
    UINT                       screenWidth_   = 0;
    UINT                       screenHeight_  = 0;
    int                        height_        = 0;
    int                        expanded_      = 0;
    int                        top_           = 0;
    int                        textureWidth_  = 0;
    int                        textureHeight_ = 0;
    int                        composed_      = -1;
    float                      clock_         = 0.0f;
    double                     frequency_     = 1.0;
    std::int64_t               last_          = 0;
};
