#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <iostream>

// Link necessary libraries
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "d3d9.lib")

// Global variables
HWND hwnd = nullptr;
bool running = true;

// Window procedure callback
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) {
        running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Helper function to initialize the window
HWND CreateSimpleWindow(HINSTANCE hInstance) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "WebcamWindow";

    RegisterClass(&wc);

    return CreateWindowEx(
        0,                            // Optional window styles
        "WebcamWindow",               // Window class name
        "Webcam Viewer",              // Window title
        WS_OVERLAPPEDWINDOW,          // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, // Position
        640, 480,                     // Size
        nullptr, nullptr,             // Parent/menus
        hInstance, nullptr            // Instance/app data
    );
}

int main() {
    // Initialize Media Foundation
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize Media Foundation." << std::endl;
        return -1;
    }

    // Enumerate video capture devices
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 deviceCount = 0;

    hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) {
        hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    }

    if (SUCCEEDED(hr)) {
        hr = MFEnumDeviceSources(pAttributes, &ppDevices, &deviceCount);
    }

    if (FAILED(hr) || deviceCount == 0) {
        std::cerr << "No video capture devices found." << std::endl;
        if (pAttributes) pAttributes->Release();
        MFShutdown();
        return -1;
    }

    // Use the first available video capture device
    IMFMediaSource* pSource = nullptr;
    hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
    for (UINT32 i = 0; i < deviceCount; ++i) {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    pAttributes->Release();

    if (FAILED(hr)) {
        std::cerr << "Failed to activate video capture device." << std::endl;
        MFShutdown();
        return -1;
    }

    // Create a source reader for the video capture device
    IMFSourceReader* pReader = nullptr;
    hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &pReader);
    pSource->Release();

    if (FAILED(hr)) {
        std::cerr << "Failed to create source reader." << std::endl;
        MFShutdown();
        return -1;
    }

    // Create a simple window
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    hwnd = CreateSimpleWindow(hInstance);
    ShowWindow(hwnd, SW_SHOW);

    // Main loop: capture frames and process
    while (running) {
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        IMFSample* pSample = nullptr;
        DWORD streamIndex, flags;
        LONGLONG timestamp;

        hr = pReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM, // Video stream index
            0,                                   // Flags
            &streamIndex,                        // Output: stream index
            &flags,                              // Output: flags
            &timestamp,                          // Output: timestamp
            &pSample                             // Output: sample
        );

        if (SUCCEEDED(hr) && pSample) {
            // Process the sample (e.g., display it)
            // For simplicity, this demo doesn't render the frames.
            pSample->Release();
        }

        // Simulate 30 FPS frame rate
        Sleep(33);
    }

    // Cleanup
    pReader->Release();
    MFShutdown();

    return 0;
}
