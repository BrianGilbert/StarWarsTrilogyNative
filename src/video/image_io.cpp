// Windows: png_load (image_io.h) through Windows Imaging Component. macOS and
// Linux have their own: src/platform/mac/image_io_mac.mm, src/platform/linux/image_io_linux.cpp.
#include "image_io.h"

#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>

namespace {

IWICImagingFactory* wic() {
    static IWICImagingFactory* factory = [] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);   // fine if COM is already set up
        IWICImagingFactory* f = nullptr;
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
        return f;
    }();
    return factory;
}

// Path for the Win32 wide-character file APIs (the project's paths are ASCII)
std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

}  // namespace

bool png_load(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba) {
    IWICImagingFactory* f = wic();
    if (!f) return false;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapSource* conv = nullptr;
    bool ok = false;
    if (SUCCEEDED(f->CreateDecoderFromFilename(widen(path).c_str(), nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)) &&
        SUCCEEDED(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame, &conv))) {
        UINT uw = 0, uh = 0;
        conv->GetSize(&uw, &uh);
        w = int(uw);
        h = int(uh);
        rgba.resize(size_t(uw) * uh * 4);
        ok = w > 0 && h > 0 && SUCCEEDED(conv->CopyPixels(nullptr, uw * 4, UINT(rgba.size()), rgba.data()));
    }
    if (conv) conv->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    return ok;
}
#endif
