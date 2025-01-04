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

// Helper function to print GUIDs
void PrintGUID(const GUID &guid) {
  wchar_t guidStr[39] = {};
  StringFromGUID2(guid, guidStr, ARRAYSIZE(guidStr));
  wprintf(L"%s\n", guidStr);
}

// Function to enumerate and display supported formats
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
      if (SUCCEEDED(hr)) {
        wprintf(L"Successfully set fallback format.\n");
      } else {
        wprintf(L"Failed to set fallback format.\n");
      }
      fallbackType->Release();
    }
  }

  return hr;
}

// Main function
int wmain() {
  HRESULT hr = S_OK;
  IMFSourceReader *pReader = NULL;
  IMFMediaSource *pSource = NULL;
  IMFAttributes *pAttributes = NULL;

  // Initialize Media Foundation
  hr = MFStartup(MF_VERSION);
  if (FAILED(hr)) {
    wprintf(L"Failed to initialize Media Foundation.\n");
    return -1;
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
    hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);
  }

  // Enumerate supported formats
  if (SUCCEEDED(hr)) {
    EnumerateSupportedFormats(pReader);
  }

  // Set video format
  if (SUCCEEDED(hr)) {
    hr = SetVideoFormat(pReader);
  }

  // Main loop to read video frames
  if (SUCCEEDED(hr)) {
    wprintf(L"Starting video capture...\n");
    IMFSample *pSample = NULL;

    while (true) {
      DWORD streamIndex = 0;
      DWORD flags = 0;
      LONGLONG llTimestamp = 0;

      hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                               &streamIndex, &flags, &llTimestamp, &pSample);

      if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
        wprintf(L"End of stream or error encountered.\n");
        break;
      }

      if (pSample) {
        wprintf(L"Captured a frame.\n");
        pSample->Release();
      }
    }
  }

  // Cleanup
  if (pReader)
    pReader->Release();
  if (pSource)
    pSource->Release();
  if (pAttributes)
    pAttributes->Release();
  MFShutdown();

  wprintf(L"Exiting application.\n");
  return 0;
}
