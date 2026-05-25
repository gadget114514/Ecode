// IME inline composition state globals
#include <windows.h>
#include <vector>
#include <string>

std::wstring g_imeComposition;
std::vector<BYTE> g_imeCompAttr;
bool g_imeComposing = false;
std::string g_imeCompUtf8;
size_t g_imeCompViewOffset = 0;
size_t g_imeCompViewLen = 0;
