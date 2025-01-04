#include <d3d9.h>
#include <iostream>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include <windows.h>


// Link necessary libraries
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "d3d9.lib")

// Global variables
HWND hwnd = nullptr;
bool running = true;
BITMAPINFO bmi = {};
std::vector<BYTE> frameBuffer;

// Window procedure callback
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
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

  return CreateWindowEx(0,                            // Optional window styles
                        "WebcamWindow",               // Window class name
                        "Webcam Viewer",              // Window title
                        WS_OVERLAPPEDWINDOW,          // Window style
                        CW_USEDEFAULT, CW_USEDEFAULT, // Position
                        640, 480,                     // Size
                        nullptr, nullptr,             // Parent/menus
                        hInstance, nullptr            // Instance/app data
  );
}

// Function to initialize the BITMAPINFO structure
void InitBitmapInfo(int width, int height) {
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height; // Negative for top-down bitmap
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 24; // 24-bit RGB
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter = 0;
  bmi.bmiHeader.biYPelsPerMeter = 0;
  bmi.bmiHeader.biClrUsed = 0;
  bmi.bmiHeader.biClrImportant = 0;
  frameBuffer.resize(width * height * 3); // Allocate buffer for RGB data
}

int main() {
  // Initialize Media Foundation
  HRESULT hr = MFStartup(MF_VERSION);
  if (FAILED(hr)) {
    std::cerr << "Failed to initialize Media Foundation." << std::endl;
    return -1;
  }

  // Enumerate video capture devices
  IMFAttributes *pAttributes = nullptr;
  IMFActivate **ppDevices = nullptr;
  UINT32 deviceCount = 0;

  hr = MFCreateAttributes(&pAttributes, 1);
  if (SUCCEEDED(hr)) {
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                              MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  }

  if (SUCCEEDED(hr)) {
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &deviceCount);
  }

  if (FAILED(hr) || deviceCount == 0) {
    std::cerr << "No video capture devices found." << std::endl;
    if (pAttributes)
      pAttributes->Release();
    MFShutdown();
    return -1;
  }

  // Use the first available video capture device
  IMFMediaSource *pSource = nullptr;
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
  IMFSourceReader *pReader = nullptr;
  hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &pReader);
  pSource->Release();

  if (FAILED(hr)) {
    std::cerr << "Failed to create source reader." << std::endl;
    MFShutdown();
    return -1;
  }

  // Set the video output format to RGB24
  IMFMediaType *pVideoType = nullptr;
  MFCreateMediaType(&pVideoType);
  pVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  pVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
  hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                    nullptr, pVideoType);
  pVideoType->Release();

  if (FAILED(hr)) {
    std::cerr << "Failed to set video format to RGB24." << std::endl;
    pReader->Release();
    MFShutdown();
    return -1;
  }

  // Get video frame dimensions
  IMFMediaType *pNativeType = nullptr;
  UINT32 width = 0, height = 0;
  hr = pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                   &pNativeType);
  if (SUCCEEDED(hr)) {
    MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &width, &height);
    pNativeType->Release();
  }
  InitBitmapInfo(width, height);

  // Create a simple window
  HINSTANCE hInstance = GetModuleHandle(nullptr);
  hwnd = CreateSimpleWindow(hInstance);
  ShowWindow(hwnd, SW_SHOW);

  // Main loop: capture frames and render
  while (running) {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    IMFSample *pSample = nullptr;
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
      IMFMediaBuffer *pBuffer = nullptr;
      hr = pSample->ConvertToContiguousBuffer(&pBuffer);
      if (SUCCEEDED(hr)) {
        BYTE *pData = nullptr;
        DWORD maxLength = 0, currentLength = 0;
        hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
        if (SUCCEEDED(hr)) {
          memcpy(frameBuffer.data(), pData, currentLength);
          pBuffer->Unlock();
        }
        pBuffer->Release();
      }
      pSample->Release();

      // Render the frame
      HDC hdc = GetDC(hwnd);
      StretchDIBits(hdc, 0, 0, width, height, 0, 0, width, height,
                    frameBuffer.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
      ReleaseDC(hwnd, hdc);
    }

    // Simulate 30 FPS frame rate
    Sleep(33);
  }

  // Cleanup
  pReader->Release();
  MFShutdown();

  return 0;
}
