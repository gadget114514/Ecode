#include "../include/ImageManager.h"
#include <cassert>

#pragma comment(lib, "windowscodecs.lib")

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------
ImageManager::ImageManager(ID2D1Factory* d2dFactory)
    : d2dFactory_(d2dFactory)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
}

ImageManager::~ImageManager() {
    Clear();
    if (wicFactory_) { wicFactory_->Release(); wicFactory_ = nullptr; }
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
bool ImageManager::EnsureWicFactory() {
    if (wicFactory_) return true;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory,
        reinterpret_cast<void**>(&wicFactory_));
    return SUCCEEDED(hr);
}

int ImageManager::ScaleDimension(int orig, int requested, int /*cellBased*/) const {
    if (requested <= 0) return orig;
    return requested;
}

ImageManager::ImageEntry* ImageManager::FindEntry(uint64_t imageId) {
    auto it = cache_.find(imageId);
    return (it != cache_.end()) ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// StoreImage
// ---------------------------------------------------------------------------
uint64_t ImageManager::StoreImage(const std::vector<uint8_t>& data,
                                  int requestedWidthPx,
                                  int requestedHeightPx,
                                  bool preserveAspectRatio)
{
    if (data.empty()) return 0;

    // Decode to get original dimensions (and validate the image)
    if (!EnsureWicFactory()) return 0;

    // Create stream from memory
    IWICStream* stream = nullptr;
    HRESULT hr = wicFactory_->CreateStream(&stream);
    if (FAILED(hr)) return 0;

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(data.data()),
        (DWORD)data.size());
    if (FAILED(hr)) { stream->Release(); return 0; }

    // Create decoder
    IWICBitmapDecoder* decoder = nullptr;
    hr = wicFactory_->CreateDecoderFromStream(
        stream, nullptr,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    stream->Release();
    if (FAILED(hr)) return 0;

    // Get first frame
    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { decoder->Release(); return 0; }

    // Get original dimensions
    UINT origW = 0, origH = 0;
    frame->GetSize(&origW, &origH);

    // Create a WIC bitmap and convert to 32bpp PBGRA
    IWICFormatConverter* converter = nullptr;
    hr = wicFactory_->CreateFormatConverter(&converter);
    if (FAILED(hr)) { frame->Release(); decoder->Release(); return 0; }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr, 0.0f,
        WICBitmapPaletteTypeMedianCut);
    frame->Release();
    decoder->Release();
    if (FAILED(hr)) { converter->Release(); return 0; }

    // Compute display dimensions
    int dispW = origW, dispH = origH;
    if (requestedWidthPx > 0 || requestedHeightPx > 0) {
        if (requestedWidthPx > 0) dispW = requestedWidthPx;
        if (requestedHeightPx > 0) dispH = requestedHeightPx;

        if (preserveAspectRatio && origW > 0 && origH > 0) {
            float scaleX = (float)dispW / origW;
            float scaleY = (float)dispH / origH;
            float scale = (scaleX < scaleY) ? scaleX : scaleY;
            dispW = (int)(origW * scale);
            dispH = (int)(origH * scale);
        }
    }
    if (dispW < 1) dispW = 1;
    if (dispH < 1) dispH = 1;

    // Read pixel data into raw buffer
    UINT stride = ((dispW * 32 + 31) / 32) * 4;
    UINT bufSize = stride * dispH;
    std::vector<uint8_t> pixels(bufSize);
    IWICBitmapScaler* scaler = nullptr;

    // Scale if requested size differs from original
    if (dispW != (int)origW || dispH != (int)origH) {
        hr = wicFactory_->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) {
            hr = scaler->Initialize(
                converter,
                dispW, dispH,
                WICBitmapInterpolationModeFant);
        }
    }

    if (scaler && SUCCEEDED(hr)) {
        hr = scaler->CopyPixels(nullptr, stride, (UINT)pixels.size(), pixels.data());
        scaler->Release();
    } else {
        // Fallback: convert at original size
        hr = converter->CopyPixels(nullptr, stride, (UINT)pixels.size(), pixels.data());
    }
    converter->Release();

    if (FAILED(hr)) return 0;

    // Store rawData as the scaled pixel data for re-creation
    uint64_t id = nextId_++;

    ImageEntry entry;
    entry.origWidth   = (int)origW;
    entry.origHeight  = (int)origH;
    entry.dispWidth   = dispW;
    entry.dispHeight  = dispH;
    entry.rawData     = std::move(pixels);
    entry.bitmap      = nullptr;
    cache_[id] = std::move(entry);

    return id;
}

// ---------------------------------------------------------------------------
// GetBitmap — lazily create from stored raw pixel data
// ---------------------------------------------------------------------------
ID2D1Bitmap* ImageManager::GetBitmap(uint64_t imageId, ID2D1RenderTarget* rt) {
    if (imageId == 0 || !rt) return nullptr;

    ImageEntry* entry = FindEntry(imageId);
    if (!entry) return nullptr;

    if (!entry->bitmap) {
        if (!CreateBitmapFromEntry(*entry, rt)) return nullptr;
    }
    return entry->bitmap;
}

bool ImageManager::CreateBitmapFromEntry(ImageEntry& entry, ID2D1RenderTarget* rt) {
    if (entry.rawData.empty()) return false;

    D2D1_SIZE_U size = D2D1::SizeU((UINT32)entry.dispWidth, (UINT32)entry.dispHeight);
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    HRESULT hr = rt->CreateBitmap(
        size,
        entry.rawData.data(),
        ((entry.dispWidth * 32 + 31) / 32) * 4,
        props,
        &entry.bitmap);
    return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------
// GetDimensions
// ---------------------------------------------------------------------------
void ImageManager::GetDimensions(uint64_t imageId, int& w, int& h) const {
    auto it = cache_.find(imageId);
    if (it != cache_.end()) {
        w = it->second.origWidth;
        h = it->second.origHeight;
    } else {
        w = h = 0;
    }
}

// ---------------------------------------------------------------------------
// Remove / Clear / RecreateAllBitmaps
// ---------------------------------------------------------------------------
void ImageManager::Remove(uint64_t imageId) {
    auto it = cache_.find(imageId);
    if (it != cache_.end()) {
        if (it->second.bitmap) {
            it->second.bitmap->Release();
        }
        cache_.erase(it);
    }
}

void ImageManager::Clear() {
    for (auto& pair : cache_) {
        if (pair.second.bitmap) {
            pair.second.bitmap->Release();
        }
    }
    cache_.clear();
}

void ImageManager::RecreateAllBitmaps(ID2D1RenderTarget* rt) {
    for (auto& pair : cache_) {
        if (pair.second.bitmap) {
            pair.second.bitmap->Release();
            pair.second.bitmap = nullptr;
        }
        if (!pair.second.rawData.empty()) {
            CreateBitmapFromEntry(pair.second, rt);
        }
    }
}
