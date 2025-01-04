#include <initguid.h>
#include <iostream>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <strsafe.h>
#include <windows.h>

// Link required libraries
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "gdi32.lib")

// Global variables
HWND g_hwnd = NULL;
IMFSourceReader *g_pReader = NULL;
BITMAPINFO g_bmpInfo;
BYTE *g_pFrameBuffer = NULL;

// Function prototypes
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void PrintGUID(const GUID &guid);
void EnumerateSupportedFormats(IMFSourceReader *pReader);
HRESULT SetVideoFormat(IMFSourceReader *pReader);
HRESULT InitWebcam();
void Cleanup();
HRESULT ProcessFrame();

// Helper function to print GUIDs
void PrintGUID(const GUID &guid) {
  wchar_t guidStr[39] = {};
  StringFromGUID2(guid, guidStr, ARRAYSIZE(guidStr));
  wprintf(L"%s\n", guidStr);
}

// Helper function to print errors
void PrintError(HRESULT hr) { wprintf(L"Error: 0x%08X\n", hr); }

// Initialize the webcam
HRESULT InitWebcam() {
  HRESULT hr = S_OK;
  IMFMediaSource *pSource = NULL;
  IMFAttributes *pAttributes = NULL;

  // Initialize Media Foundation
  hr = MFStartup(MF_VERSION);
  if (FAILED(hr)) {
    wprintf(L"Failed to initialize Media Foundation.\n");
    return hr;
  }

  // Create attributes for the source reader
  hr = MFCreateAttributes(&pAttributes, 1);
  if (SUCCEEDED(hr)) {
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                              MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  }

  // Activate the video capture device
  IMFActivate **ppDevices = NULL;
  UINT32 deviceCount = 0;
  if (SUCCEEDED(hr)) {
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &deviceCount);
  }

  if (SUCCEEDED(hr) && deviceCount > 0) {
    wprintf(L"Found %d video capture devices. Using the first one.\n",
            deviceCount);

    // Create media source for the first device
    hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));

    // Release device list
    for (UINT32 i = 0; i < deviceCount; i++) {
      ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
  } else {
    wprintf(L"No video capture devices found.\n");
    hr = E_FAIL;
  }

  // Create a source reader from the media source
  if (SUCCEEDED(hr)) {
    hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &g_pReader);
  }

  // Enumerate supported formats
  if (SUCCEEDED(hr)) {
    EnumerateSupportedFormats(g_pReader);
  }

  // Set video format
  if (SUCCEEDED(hr)) {
    hr = SetVideoFormat(g_pReader);
  }

  // Get video dimensions
  if (SUCCEEDED(hr)) {
    IMFMediaType *pType = NULL;
    hr = g_pReader->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pType);
    if (SUCCEEDED(hr)) {
      UINT32 width = 0, height = 0;
      MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
      wprintf(L"Video dimensions: %ux%u\n", width, height);

      // Initialize bitmap info for GDI rendering
      ZeroMemory(&g_bmpInfo, sizeof(g_bmpInfo));
      g_bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      g_bmpInfo.bmiHeader.biWidth = width;
      g_bmpInfo.bmiHeader.biHeight =
          -(LONG)height; // Negative to indicate top-down rows
      g_bmpInfo.bmiHeader.biPlanes = 1;
      g_bmpInfo.bmiHeader.biBitCount = 24; // RGB24
      g_bmpInfo.bmiHeader.biCompression = BI_RGB;
      g_bmpInfo.bmiHeader.biSizeImage = width * height * 3;

      // Allocate frame buffer
      g_pFrameBuffer = new BYTE[g_bmpInfo.bmiHeader.biSizeImage];
    }
    pType->Release();
  }

  if (pSource)
    pSource->Release();
  if (pAttributes)
    pAttributes->Release();

  return hr;
}

// Process a single frame from the webcam
HRESULT ProcessFrame() {
  HRESULT hr = S_OK;
  IMFSample *pSample = NULL;
  IMFMediaBuffer *pBuffer = NULL;
  BYTE *pData = NULL;
  DWORD maxLength = 0, currentLength = 0;

  hr = g_pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL,
                             NULL, &pSample);

  if (SUCCEEDED(hr) && pSample) {
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (SUCCEEDED(hr)) {
      hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
      if (SUCCEEDED(hr)) {
        // Copy the frame data to our buffer
        memcpy(g_pFrameBuffer, pData, currentLength);
        pBuffer->Unlock();
      }
    }
  }

  if (pBuffer)
    pBuffer->Release();
  if (pSample)
    pSample->Release();

  return hr;
}

// Render the frame using GDI
void RenderFrame(HDC hdc) {
  if (g_pFrameBuffer) {
    StretchDIBits(hdc, 0, 0, g_bmpInfo.bmiHeader.biWidth,
                  -g_bmpInfo.bmiHeader.biHeight, 0, 0,
                  g_bmpInfo.bmiHeader.biWidth, -g_bmpInfo.bmiHeader.biHeight,
                  g_pFrameBuffer, &g_bmpInfo, DIB_RGB_COLORS, SRCCOPY);
  }
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RenderFrame(hdc);
    EndPaint(hwnd, &ps);
    break;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return 0;
}

// Cleanup resources
void Cleanup() {
  if (g_pReader)
    g_pReader->Release();
  if (g_pFrameBuffer)
    delete[] g_pFrameBuffer;
  MFShutdown();
}

void EnumerateSupportedFormats(IMFSourceReader *pReader) {
  wprintf(L"Enumerating supported formats...\n");
  IMFMediaType *pType = NULL;
  DWORD i = 0;

  while (SUCCEEDED(pReader->GetNativeMediaType(
      (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType))) {
    GUID subtype = {0};
    if (SUCCEEDED(pType->GetGUID(MF_MT_SUBTYPE, &subtype))) {
      wprintf(L"Format %d: ", i);
      PrintGUID(subtype);
    }
    pType->Release();
    i++;
  }

  if (i == 0) {
    wprintf(L"No supported formats found.\n");
  }
}

// Function to set video format
HRESULT SetVideoFormat(IMFSourceReader *pReader) {
  HRESULT hr = S_OK;
  IMFMediaType *pType = NULL;

  // Try to set the video format to RGB24
  hr = MFCreateMediaType(&pType);
  if (SUCCEEDED(hr)) {
    hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) {
      hr = pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
      if (SUCCEEDED(hr)) {
        hr = pReader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
        if (SUCCEEDED(hr)) {
          wprintf(L"Successfully set video format to RGB24.\n");
        } else {
          wprintf(L"Failed to set video format to RGB24. Trying another "
                  L"format...\n");
        }
      }
    }
  }

  pType->Release();

  // If RGB24 fails, fallback to the first available format
  if (FAILED(hr)) {
    wprintf(L"Falling back to the first supported format.\n");
    IMFMediaType *fallbackType = NULL;
    hr = pReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                     0, &fallbackType);
    if (SUCCEEDED(hr)) {
      hr = pReader->SetCurrentMediaType(
          (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, fallbackType);
      fallbackType->Release();
    }
  }

  return hr;
}

// Main entry point
int main() {
  // Initialize the webcam
  HRESULT hr = InitWebcam();
  if (FAILED(hr)) {
    PrintError(hr);
    return -1;
  }

  // Create a simple window to render the video frames
  WNDCLASS wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = "WebcamWindow";

  if (!RegisterClass(&wc)) {
    wprintf(L"Failed to register window class.\n");
    Cleanup();
    return -1;
  }

  g_hwnd = CreateWindowEx(0, wc.lpszClassName, "Webcam Viewer",
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          640, 480, NULL, NULL, wc.hInstance, NULL);
  if (g_hwnd == NULL) {
    wprintf(L"Failed to create window.\n");
    Cleanup();
    return -1;
  }

  ShowWindow(g_hwnd, SW_SHOWNORMAL);

  // Main message loop
  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    // Process frame every 50 ms (approximately)
    hr = ProcessFrame();
    if (FAILED(hr)) {
      PrintError(hr);
      //break;
    }

    // Invalidate the window to trigger WM_PAINT and render the frame
    InvalidateRect(g_hwnd, NULL, TRUE);
    Sleep(50);
  }

  // Cleanup and shutdown
  Cleanup();
  return 0;
}
