#include "MockAIServer.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cstdlib>

// ── serve mode ─────────────────────────────────────────────────────
// Invoked with --serve [port] to keep the server running indefinitely.
// This is the mode used by the Prompts Electron app (MockHTTPProvider).
// The server prints its port on stdout and blocks until killed.
//
// Example:
//   test_mock_ai_server.exe --serve 8765
//   test_mock_ai_server.exe --serve          (random port)
static int serveMode(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
    MockHTTPAIServer server;
    if (!server.Start(port)) {
        std::cerr << "Failed to start MockHTTPAIServer on port " << port << "\n";
        WSACleanup();
        return 1;
    }
    std::cout << "MockAIServer ready on port " << server.GetPort() << std::endl;
    std::cout.flush();

    // Block until the process is killed.
    // SetConsoleCtrlHandler would allow graceful CTRL+C, but for test tooling
    // a simple Sleep loop is sufficient.
    while (true) { Sleep(1000); }

    server.Stop();
    WSACleanup();
    return 0;
}

// ── self-test mode ──────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // --serve [port]  →  run as a long-lived HTTP server
    if (argc >= 2 && std::string(argv[1]) == "--serve") {
        int port = (argc >= 3) ? std::atoi(argv[2]) : 8765;
        return serveMode(port);
    }

    // No arguments → run the built-in recipe self-tests and exit.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    MockHTTPAIServer server;
    if (!server.Start()) {
        std::cerr << "Failed to start MockHTTPAIServer\n";
        WSACleanup();
        return 1;
    }
    std::cout << "[MockHTTPAIServer] Started on port " << server.GetPort() << "\n";

    MockAIHTTPClient client(server.GetPort());

    // Recipe 1: text-to-text
    std::cout << "Testing Recipe 1: text-to-text...\n";
    std::string r1 = client.TextToText("Hello World & Antigravity!");
    assert(r1 == "Response to: Hello World & Antigravity!");
    std::cout << "  Result: " << r1 << "\n";
    std::cout << "Recipe 1 PASSED\n\n";

    // Recipe 2: image-to-image (roundtrip — output bytes == input bytes)
    std::cout << "Testing Recipe 2: image-to-image...\n";
    std::string dummyImg(std::string("DUMMY_IMAGE_DATA_") + '\x00' + '\x01' + '\x02' + '\xFF');
    std::string r2 = client.ImageToImage(dummyImg);
    assert(r2 == dummyImg);
    std::cout << "Recipe 2 PASSED\n\n";

    // Recipe 3: multi-image-to-image
    std::cout << "Testing Recipe 3: multi-image-to-image...\n";
    std::string fixedImg = "FIXED_IMAGE";
    std::vector<std::string> inputImgs = {"INPUT_1", "INPUT_2"};
    std::string r3 = client.MultiImageToImage(fixedImg, inputImgs);
    std::string expectedB64 = MockAI_Base64Encode(fixedImg) + "_" + MockAI_Base64Encode(inputImgs[0]);
    std::string expected = MockAI_Base64Decode(expectedB64);
    assert(r3 == expected);
    std::cout << "Recipe 3 PASSED\n\n";

    server.Stop();
    WSACleanup();
    std::cout << "=== ALL Mock AI Tests PASSED ===\n";
    return 0;
}
