#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

static std::string base64Encode(const unsigned char* data, size_t len) {
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int b = (unsigned int)data[i] << 16;
        if (i + 1 < len) b |= (unsigned int)data[i+1] << 8;
        if (i + 2 < len) b |= (unsigned int)data[i+2];
        out += kTable[(b >> 18) & 0x3f];
        out += kTable[(b >> 12) & 0x3f];
        out += (i + 1 < len) ? kTable[(b >> 6) & 0x3f] : '=';
        out += (i + 2 < len) ? kTable[(b >> 0) & 0x3f] : '=';
    }
    return out;
}

int main() {
    const char* path = "D:\\ws\\Ecode\\libsixel-windows\\images\\map8.six";
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    std::string b64 = base64Encode(buf.data(), buf.size());

    // OSC 1337 inline image: ESC ] 1337 ; File=inline=1:<base64> BEL
    // ConPTY passes OSC through; Terminal.exe decodes and calls StoreImage/StoreSixelImage
    std::string osc;
    osc += "\x1b]1337;File=inline=1:";
    osc += b64;
    osc += "\x07"; // BEL terminates OSC

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written = 0;
    WriteFile(hOut, osc.data(), (DWORD)osc.size(), &written, nullptr);
    return 0;
}
