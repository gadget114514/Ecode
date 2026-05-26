#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// ImageManager
//
// Decodes raw image bytes via WIC, stores ID2D1Bitmap for rendering,
// and manages a cache keyed by imageId.
// ---------------------------------------------------------------------------
class ImageManager {
public:
    explicit ImageManager(ID2D1Factory* d2dFactory);
    ~ImageManager();

    // Decode raw bytes and store in cache. Returns a unique imageId.
    // If the image data is invalid, returns 0.
    uint64_t StoreImage(const std::vector<uint8_t>& data,
                        int requestedWidthPx,
                        int requestedHeightPx,
                        bool preserveAspectRatio);

    // Decode sixel data (via libsixel) and store in cache.
    // Returns a unique imageId, or 0 on failure.
    uint64_t StoreSixelImage(const std::vector<uint8_t>& sixelData);

    // Get (or lazily create) a D2D bitmap for the given imageId.
    // Returns nullptr if imageId is 0 or not found.
    ID2D1Bitmap* GetBitmap(uint64_t imageId, ID2D1RenderTarget* rt);

    // Get original pixel dimensions.
    void GetDimensions(uint64_t imageId, int& w, int& h) const;

    // Remove a single image from cache.
    void Remove(uint64_t imageId);

    // Clear all cached images and release all resources.
    void Clear();

    // Re-create all D2D bitmaps (call on D2DERR_RECREATE_TARGET).
    void RecreateAllBitmaps(ID2D1RenderTarget* rt);

private:
    struct ImageEntry {
        ID2D1Bitmap* bitmap = nullptr;         // lazily created
        int origWidth  = 0;                     // original pixel width
        int origHeight = 0;                     // original pixel height
        int dispWidth  = 0;                     // display pixel width
        int dispHeight = 0;                     // display pixel height
        std::vector<uint8_t> rawData;           // for lazy re-creation
        bool bitmapsNeedRecreate = false;       // true after device loss
    };

    ID2D1Factory* d2dFactory_;
    IWICImagingFactory* wicFactory_ = nullptr;
    std::unordered_map<uint64_t, ImageEntry> cache_;
    uint64_t nextId_ = 1;

    bool EnsureWicFactory();
    ImageEntry* FindEntry(uint64_t imageId);
    bool CreateBitmapFromEntry(ImageEntry& entry, ID2D1RenderTarget* rt);
    int ScaleDimension(int orig, int requested, int cellBased) const;
};
