#include "360anim.hpp"

#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <iterator>
#include <span>
#include <type_traits>

namespace {

    constexpr float kReferenceHeight = 720.0f;
    constexpr float kHeight          = 64.0f;
    constexpr float kWidth           = 422.0f;
    constexpr float kTop             = 36.0f;
    constexpr float kCenter          = 32.0f;
    constexpr float kCircleRadius    = 29.0f;
    constexpr float kQuarterSize     = 25.0f;
    constexpr float kLogoRadius      = 17.0f;
    constexpr float kImageRadius     = 15.0f;
    constexpr float kImageLeft       = 17.0f;
    constexpr float kImageSize       = 30.0f;
    constexpr float kTextLeft        = 85.0f;
    constexpr float kFontSize        = 25.6f;
    constexpr float kMinScale        = 0.5f;
    constexpr float kMaxScale        = 4.0f;

    constexpr float kFadeIn      = 0.3f;
    constexpr float kSlideStart  = 1.0f;
    constexpr float kSlideLength = 1.0f;
    constexpr float kFadeStart   = 6.0f;
    constexpr float kFadeLength  = 0.75f;
    constexpr float kMaxStep     = 0.1f;

    constexpr int  kSupersample   = 4;
    constexpr int  kIconSize      = 32;
    constexpr WORD kGroupIconType = 14;

    constexpr wchar_t kTitle[]   = L"Achievement unlocked";
    constexpr wchar_t kMessage[] = L"MWCheats loaded";
    constexpr wchar_t kFont[]    = L"Microsoft Sans Serif";
    constexpr wchar_t kChime[]   = L"ACHIEVEMENT";

    struct Color {
        float r, g, b;
    };

    constexpr Color Rgb(std::uint32_t value) {
        return { static_cast<float>(value >> 16 & 0xFF), static_cast<float>(value >> 8 & 0xFF),
                 static_cast<float>(value & 0xFF) };
    }

    constexpr Color kBackground = Rgb(0x4D4D4D);
    constexpr Color kCircle     = Rgb(0x1A1A1A);
    constexpr Color kRingLit    = Rgb(0x75AA1E);
    constexpr Color kRing       = Rgb(0x494949);
    constexpr Color kLogoBorder = Rgb(0x161616);
    constexpr Color kText       = Rgb(0xFFFFFF);

    struct Quarter {
        float x, y, left, top;
        Color color;
    };

    constexpr std::array<Quarter, 4> kQuarters{ {
        { 31.0f, 31.0f, 6.0f, 6.0f, kRingLit },
        { 33.0f, 31.0f, 33.0f, 6.0f, kRing },
        { 33.0f, 33.0f, 33.0f, 33.0f, kRing },
        { 31.0f, 33.0f, 6.0f, 33.0f, kRing },
    } };

    struct Texel {
        float r, g, b, a;
    };

    struct Vertex {
        float    x, y, z, rhw;
        D3DCOLOR color;
        float    u, v;
    };

    constexpr DWORD kVertexFormat = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

    struct RenderState {
        D3DRENDERSTATETYPE type;
        DWORD              value;
    };

    constexpr std::array<RenderState, 17> kRenderStates{ {
        { D3DRS_ZENABLE, D3DZB_FALSE },
        { D3DRS_ZWRITEENABLE, FALSE },
        { D3DRS_ALPHATESTENABLE, FALSE },
        { D3DRS_ALPHABLENDENABLE, TRUE },
        { D3DRS_SEPARATEALPHABLENDENABLE, FALSE },
        { D3DRS_BLENDOP, D3DBLENDOP_ADD },
        { D3DRS_SRCBLEND, D3DBLEND_ONE },
        { D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA },
        { D3DRS_CULLMODE, D3DCULL_NONE },
        { D3DRS_FILLMODE, D3DFILL_SOLID },
        { D3DRS_LIGHTING, FALSE },
        { D3DRS_FOGENABLE, FALSE },
        { D3DRS_STENCILENABLE, FALSE },
        { D3DRS_SCISSORTESTENABLE, FALSE },
        { D3DRS_CLIPPLANEENABLE, 0 },
        { D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE },
        { D3DRS_SRGBWRITEENABLE, FALSE },
    } };

    struct StageState {
        DWORD                    stage;
        D3DTEXTURESTAGESTATETYPE type;
        DWORD                    value;
    };

    constexpr std::array<StageState, 10> kStageStates{ {
        { 0, D3DTSS_COLOROP, D3DTOP_MODULATE },
        { 0, D3DTSS_COLORARG1, D3DTA_TEXTURE },
        { 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE },
        { 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE },
        { 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE },
        { 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE },
        { 0, D3DTSS_TEXCOORDINDEX, 0 },
        { 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE },
        { 1, D3DTSS_COLOROP, D3DTOP_DISABLE },
        { 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE },
    } };

    struct SamplerState {
        D3DSAMPLERSTATETYPE type;
        DWORD               value;
    };

    constexpr std::array<SamplerState, 6> kSamplerStates{ {
        { D3DSAMP_MINFILTER, D3DTEXF_POINT },
        { D3DSAMP_MAGFILTER, D3DTEXF_POINT },
        { D3DSAMP_MIPFILTER, D3DTEXF_NONE },
        { D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP },
        { D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP },
        { D3DSAMP_SRGBTEXTURE, FALSE },
    } };

    struct DeleteDc {
        void operator()(HDC dc) const noexcept { DeleteDC(dc); }
    };

    struct DeleteGdiObject {
        void operator()(void* object) const noexcept { DeleteObject(static_cast<HGDIOBJ>(object)); }
    };

    struct DestroyIconHandle {
        void operator()(HICON icon) const noexcept { DestroyIcon(icon); }
    };

    using DcHandle   = std::unique_ptr<std::remove_pointer_t<HDC>, DeleteDc>;
    using GdiHandle  = std::unique_ptr<void, DeleteGdiObject>;
    using IconHandle = std::unique_ptr<std::remove_pointer_t<HICON>, DestroyIconHandle>;

    class Canvas {
    public:
        Canvas(int width, int height) noexcept {
            BITMAPINFO info{};
            info.bmiHeader.biSize        = sizeof(info.bmiHeader);
            info.bmiHeader.biWidth       = width;
            info.bmiHeader.biHeight      = -height;
            info.bmiHeader.biPlanes      = 1;
            info.bmiHeader.biBitCount    = 32;
            info.bmiHeader.biCompression = BI_RGB;

            void* bits = nullptr;
            bitmap_.reset(CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0));
            dc_.reset(CreateCompatibleDC(nullptr));
            if (!bitmap_ || !dc_ || bits == nullptr) return;

            SelectObject(dc_.get(), bitmap_.get());
            pixels_ = { static_cast<std::uint32_t*>(bits), static_cast<std::size_t>(width) * height };
            std::ranges::fill(pixels_, 0u);
        }

        [[nodiscard]] HDC Dc() const noexcept { return dc_.get(); }
        [[nodiscard]] std::span<std::uint32_t> Pixels() const noexcept { return pixels_; }
        [[nodiscard]] explicit operator bool() const noexcept { return !pixels_.empty(); }

    private:
        GdiHandle                bitmap_;
        DcHandle                 dc_;
        std::span<std::uint32_t> pixels_;
    };

    float Disc(float x, float y, float centerX, float centerY, float radius) noexcept {
        const float dx = x - centerX;
        const float dy = y - centerY;
        return std::clamp(radius - std::sqrt(dx * dx + dy * dy) + 0.5f, 0.0f, 1.0f);
    }

    float Span(float position, float low, float high) noexcept {
        return std::clamp(std::min(position + 0.5f, high) - std::max(position - 0.5f, low), 0.0f, 1.0f);
    }

    float Pill(float x, float y, float width, float height) noexcept {
        const float radius = height * 0.5f;
        return Disc(x, y, std::clamp(x, radius, width - radius), radius, radius);
    }

    void Blend(Color& target, const Color& source, float alpha) noexcept {
        target.r += (source.r - target.r) * alpha;
        target.g += (source.g - target.g) * alpha;
        target.b += (source.b - target.b) * alpha;
    }

    std::uint32_t Channel(float value) noexcept {
        return static_cast<std::uint32_t>(std::clamp(value, 0.0f, 255.0f) + 0.5f);
    }

    std::uint32_t Premultiply(std::uint32_t rgb, float coverage) noexcept {
        const auto scaled = [coverage](std::uint32_t value) { return Channel(static_cast<float>(value) * coverage); };
        return scaled(255) << 24 | scaled(rgb >> 16 & 0xFF) << 16 | scaled(rgb >> 8 & 0xFF) << 8 | scaled(rgb & 0xFF);
    }

    float Bezier(float t, float first, float second) noexcept {
        const float u = 1.0f - t;
        return 3.0f * u * u * t * first + 3.0f * u * t * t * second + t * t * t;
    }

    float Ease(float progress) noexcept {
        float low  = 0.0f;
        float high = 1.0f;
        for (int step = 0; step < 24; ++step) {
            const float middle = (low + high) * 0.5f;
            (Bezier(middle, 0.25f, 0.25f) < progress ? low : high) = middle;
        }
        return Bezier((low + high) * 0.5f, 0.1f, 1.0f);
    }

    std::vector<float> PaintText(float scale, int left, int width, int height) {
        std::vector<float> coverage(static_cast<std::size_t>(width) * height, 0.0f);
        const int span = width - left;
        if (span <= 0) return coverage;

        const GdiHandle font{ CreateFontW(-static_cast<int>(std::lround(kFontSize * scale * kSupersample)), 0, 0, 0,
                                          FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, kFont) };
        const Canvas canvas{ span * kSupersample, height * kSupersample };
        if (!font || !canvas) return coverage;

        SelectObject(canvas.Dc(), font.get());
        SetBkMode(canvas.Dc(), TRANSPARENT);
        SetTextColor(canvas.Dc(), RGB(255, 255, 255));

        TEXTMETRICW metrics{};
        GetTextMetricsW(canvas.Dc(), &metrics);
        const int lead = metrics.tmExternalLeading / 2;
        const int line = metrics.tmHeight + metrics.tmExternalLeading;
        TextOutW(canvas.Dc(), 0, lead, kTitle, static_cast<int>(std::size(kTitle) - 1));
        TextOutW(canvas.Dc(), 0, lead + line, kMessage, static_cast<int>(std::size(kMessage) - 1));
        GdiFlush();

        const std::span<const std::uint32_t> pixels = canvas.Pixels();
        const std::size_t stride = static_cast<std::size_t>(span) * kSupersample;
        constexpr float samples = static_cast<float>(kSupersample * kSupersample) * 255.0f;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < span; ++x) {
                std::uint32_t sum = 0;
                for (int row = 0; row < kSupersample; ++row) {
                    const std::size_t base = (static_cast<std::size_t>(y) * kSupersample + row) * stride +
                                             static_cast<std::size_t>(x) * kSupersample;
                    for (int column = 0; column < kSupersample; ++column) sum += pixels[base + column] >> 8 & 0xFF;
                }
                coverage[static_cast<std::size_t>(y) * width + left + x] = static_cast<float>(sum) / samples;
            }
        }
        return coverage;
    }

    BOOL CALLBACK LoadFirstIcon(HMODULE module, LPCWSTR, LPWSTR name, LONG_PTR slot) {
        *reinterpret_cast<HICON*>(slot) =
            static_cast<HICON>(LoadImageW(module, name, IMAGE_ICON, kIconSize, kIconSize, LR_DEFAULTCOLOR));
        return FALSE;
    }

    std::vector<Texel> LoadGameIcon() {
        HICON loaded = nullptr;
        EnumResourceNamesW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(kGroupIconType), LoadFirstIcon,
                           reinterpret_cast<LONG_PTR>(&loaded));
        const IconHandle icon{ loaded };

        std::vector<Texel> texels;
        const Canvas dark{ kIconSize, kIconSize };
        const Canvas light{ kIconSize, kIconSize };
        if (!icon || !dark || !light) return texels;

        std::ranges::fill(light.Pixels(), 0xFFFFFFFFu);
        DrawIconEx(dark.Dc(), 0, 0, icon.get(), kIconSize, kIconSize, 0, nullptr, DI_NORMAL);
        DrawIconEx(light.Dc(), 0, 0, icon.get(), kIconSize, kIconSize, 0, nullptr, DI_NORMAL);
        GdiFlush();

        texels.reserve(dark.Pixels().size());
        for (std::size_t i = 0; i < dark.Pixels().size(); ++i) {
            const std::uint32_t over  = dark.Pixels()[i];
            const std::uint32_t under = light.Pixels()[i];
            const float lift = static_cast<float>(under >> 8 & 0xFF) - static_cast<float>(over >> 8 & 0xFF);
            texels.push_back({ static_cast<float>(over >> 16 & 0xFF), static_cast<float>(over >> 8 & 0xFF),
                               static_cast<float>(over & 0xFF), std::clamp(1.0f - lift / 255.0f, 0.0f, 1.0f) });
        }
        return texels;
    }

    Texel Sample(const std::vector<Texel>& texels, float u, float v) noexcept {
        const float x = std::clamp(u * kIconSize - 0.5f, 0.0f, static_cast<float>(kIconSize - 1));
        const float y = std::clamp(v * kIconSize - 0.5f, 0.0f, static_cast<float>(kIconSize - 1));
        const int   left   = static_cast<int>(x);
        const int   top    = static_cast<int>(y);
        const int   right  = std::min(left + 1, kIconSize - 1);
        const int   bottom = std::min(top + 1, kIconSize - 1);
        const float fx     = x - static_cast<float>(left);
        const float fy     = y - static_cast<float>(top);

        const auto at = [&texels](int column, int row) -> const Texel& {
            return texels[static_cast<std::size_t>(row) * kIconSize + column];
        };
        const auto mix = [](const Texel& a, const Texel& b, float t) {
            return Texel{ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
        };
        return mix(mix(at(left, top), at(right, top), fx), mix(at(left, bottom), at(right, bottom), fx), fy);
    }

    std::vector<std::uint32_t> Paint(float scale, int width, int height) {
        const std::vector<float> text = PaintText(scale, static_cast<int>(std::lround(kTextLeft * scale)), width, height);
        const std::vector<Texel> icon = LoadGameIcon();

        const float center = kCenter * scale;
        const float badge  = kHeight * scale;
        std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * height);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                const std::size_t index = static_cast<std::size_t>(y) * width + x;
                Color color = kBackground;

                if (px < badge) {
                    Blend(color, kCircle, Disc(px, py, center, center, kCircleRadius * scale));
                    for (const Quarter& quarter : kQuarters) {
                        const float coverage = Disc(px, py, quarter.x * scale, quarter.y * scale, kQuarterSize * scale) *
                                               Span(px, quarter.left * scale, (quarter.left + kQuarterSize) * scale) *
                                               Span(py, quarter.top * scale, (quarter.top + kQuarterSize) * scale);
                        Blend(color, quarter.color, coverage);
                    }
                    Blend(color, kLogoBorder, Disc(px, py, center, center, kLogoRadius * scale));

                    if (const float mask = Disc(px, py, center, center, kImageRadius * scale); mask > 0.0f && !icon.empty()) {
                        const Texel texel = Sample(icon, (px - kImageLeft * scale) / (kImageSize * scale),
                                                   (py - kImageLeft * scale) / (kImageSize * scale));
                        const float keep = 1.0f - texel.a * mask;
                        color = { texel.r * mask + color.r * keep, texel.g * mask + color.g * keep,
                                  texel.b * mask + color.b * keep };
                    }
                }

                Blend(color, kText, text[index]);
                pixels[index] = Channel(color.r) << 16 | Channel(color.g) << 8 | Channel(color.b);
            }
        }
        return pixels;
    }

    DWORD WINAPI PlayChime(LPVOID resources) {
        PlaySoundW(kChime, static_cast<HMODULE>(resources), SND_RESOURCE | SND_SYNC | SND_NODEFAULT);
        return 0;
    }

}

Xbox360Anim::Xbox360Anim(HMODULE resources) noexcept : resources_(resources) {
    LARGE_INTEGER frequency{};
    if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
        frequency_ = static_cast<double>(frequency.QuadPart);
    }
}

void Xbox360Anim::Trigger() noexcept {
    triggered_.store(true, std::memory_order_release);
}

bool Xbox360Anim::Render(IDirect3DDevice9* device) noexcept {
    if (finished_) return false;
    if (device == nullptr || !triggered_.load(std::memory_order_acquire)) return true;

    if (device_ != nullptr && device != device_) {
        static_cast<void>(texture_.release());
        return Finish();
    }

    IDirect3DSurface9* surface = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface))) return true;
    const ComPtr<IDirect3DSurface9> backBuffer{ surface };

    D3DSURFACE_DESC desc{};
    if (FAILED(backBuffer->GetDesc(&desc))) return true;
    if ((desc.Width != screenWidth_ || desc.Height != screenHeight_) && !Build(device, desc.Width, desc.Height)) {
        return Finish();
    }

    Advance();
    if (clock_ >= kFadeStart + kFadeLength) return Finish();

    if (!chimed_ && clock_ >= kSlideStart) {
        chimed_ = true;
        if (const HANDLE thread = CreateThread(nullptr, 0, PlayChime, resources_, 0, nullptr)) CloseHandle(thread);
    }

    const float progress = std::clamp((clock_ - kSlideStart) / kSlideLength, 0.0f, 1.0f);
    const int   width    = height_ + static_cast<int>(std::lround(static_cast<float>(expanded_ - height_) * Ease(progress)));
    if (width != composed_ && !Compose(width)) return Finish();

    const float fadeOut = std::clamp((clock_ - kFadeStart) / kFadeLength, 0.0f, 1.0f);
    Draw(device, backBuffer.get(), width, std::min(clock_ / kFadeIn, 1.0f) * (1.0f - fadeOut));
    return true;
}

bool Xbox360Anim::Build(IDirect3DDevice9* device, UINT width, UINT height) noexcept {
    texture_.reset();

    const float scale = std::clamp(static_cast<float>(height) / kReferenceHeight, kMinScale, kMaxScale);
    height_ = std::max(1, static_cast<int>(std::lround(kHeight * scale)));

    const float fitted = static_cast<float>(height_) / kHeight;
    expanded_      = static_cast<int>(std::lround(kWidth * fitted));
    top_           = static_cast<int>(std::lround(kTop * fitted));
    textureWidth_  = static_cast<int>(std::bit_ceil(static_cast<unsigned>(expanded_)));
    textureHeight_ = static_cast<int>(std::bit_ceil(static_cast<unsigned>(height_)));
    inner_         = Paint(fitted, expanded_, height_);

    IDirect3DTexture9* texture = nullptr;
    if (FAILED(device->CreateTexture(static_cast<UINT>(textureWidth_), static_cast<UINT>(textureHeight_), 1, 0,
                                     D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr))) {
        return false;
    }
    texture_.reset(texture);

    D3DLOCKED_RECT locked{};
    if (FAILED(texture_->LockRect(0, &locked, nullptr, 0))) return false;
    for (int y = 0; y < textureHeight_; ++y) {
        std::memset(static_cast<std::uint8_t*>(locked.pBits) + static_cast<std::ptrdiff_t>(y) * locked.Pitch, 0,
                    static_cast<std::size_t>(textureWidth_) * sizeof(std::uint32_t));
    }
    texture_->UnlockRect(0);

    device_       = device;
    screenWidth_  = width;
    screenHeight_ = height;
    composed_     = -1;
    return true;
}

bool Xbox360Anim::Compose(int width) noexcept {
    const int  cap   = height_ / 2 + 1;
    const int  from  = composed_ < 0 ? 0 : std::max(0, std::min(width, composed_) - cap);
    const int  to    = composed_ < 0 ? expanded_ : std::min(expanded_, std::max(width, composed_));
    const RECT dirty{ from, 0, to, height_ };

    D3DLOCKED_RECT locked{};
    if (FAILED(texture_->LockRect(0, &locked, &dirty, 0))) return false;

    for (int y = 0; y < height_; ++y) {
        auto* const row = reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(locked.pBits) +
                                                           static_cast<std::ptrdiff_t>(y) * locked.Pitch);
        for (int x = from; x < to; ++x) {
            const float coverage = x < width ? Pill(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f,
                                                    static_cast<float>(width), static_cast<float>(height_))
                                             : 0.0f;
            row[x - from] = Premultiply(inner_[static_cast<std::size_t>(y) * expanded_ + x], coverage);
        }
    }

    texture_->UnlockRect(0);
    composed_ = width;
    return true;
}

bool Xbox360Anim::Finish() noexcept {
    texture_.reset();
    inner_    = {};
    finished_ = true;
    return false;
}

void Xbox360Anim::Advance() noexcept {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (last_ != 0) {
        const auto elapsed = static_cast<float>(static_cast<double>(now.QuadPart - last_) / frequency_);
        clock_ += std::clamp(elapsed, 0.0f, kMaxStep);
    }
    last_ = now.QuadPart;
}

void Xbox360Anim::Draw(IDirect3DDevice9* device, IDirect3DSurface9* backBuffer, int width, float alpha) const noexcept {
    IDirect3DStateBlock9* block = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &block))) return;
    const ComPtr<IDirect3DStateBlock9> state{ block };

    IDirect3DSurface9* surface = nullptr;
    device->GetRenderTarget(0, &surface);
    const ComPtr<IDirect3DSurface9> target{ surface };

    surface = nullptr;
    device->GetDepthStencilSurface(&surface);
    const ComPtr<IDirect3DSurface9> depth{ surface };

    D3DVIEWPORT9 viewport{};
    RECT         scissor{};
    device->GetViewport(&viewport);
    device->GetScissorRect(&scissor);

    const bool retarget = target.get() != backBuffer;
    if (retarget) {
        device->SetRenderTarget(0, backBuffer);
        device->SetDepthStencilSurface(nullptr);
    }

    const D3DVIEWPORT9 screen{ 0, 0, screenWidth_, screenHeight_, 0.0f, 1.0f };
    device->SetViewport(&screen);
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetFVF(kVertexFormat);
    device->SetTexture(0, texture_.get());
    for (const RenderState& render : kRenderStates) device->SetRenderState(render.type, render.value);
    for (const StageState& stage : kStageStates) device->SetTextureStageState(stage.stage, stage.type, stage.value);
    for (const SamplerState& sampler : kSamplerStates) device->SetSamplerState(0, sampler.type, sampler.value);

    const float left   = static_cast<float>((static_cast<int>(screenWidth_) - width) / 2) - 0.5f;
    const float top    = static_cast<float>(top_) - 0.5f;
    const float right  = left + static_cast<float>(width);
    const float bottom = top + static_cast<float>(height_);
    const float u      = static_cast<float>(width) / static_cast<float>(textureWidth_);
    const float v      = static_cast<float>(height_) / static_cast<float>(textureHeight_);

    const auto     shade = Channel(alpha * 255.0f);
    const D3DCOLOR color = D3DCOLOR_ARGB(shade, shade, shade, shade);

    const std::array<Vertex, 4> quad{ {
        { left, top, 0.0f, 1.0f, color, 0.0f, 0.0f },
        { right, top, 0.0f, 1.0f, color, u, 0.0f },
        { left, bottom, 0.0f, 1.0f, color, 0.0f, v },
        { right, bottom, 0.0f, 1.0f, color, u, v },
    } };
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad.data(), sizeof(Vertex));

    if (retarget) {
        device->SetRenderTarget(0, target.get());
        device->SetDepthStencilSurface(depth.get());
    }
    state->Apply();
    device->SetViewport(&viewport);
    device->SetScissorRect(&scissor);
}
