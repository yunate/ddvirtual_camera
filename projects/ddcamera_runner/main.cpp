#include "ddcamera_runner/stdafx.h"

#include <windows.h>
#include <dshow.h>
#include <atlbase.h>
#include <vector>
#include <string>
#include <iostream>

#pragma comment(lib, "strmiids.lib")

// 简单窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 创建简单窗口用于预览视频
HWND CreatePreviewWindow(HINSTANCE hInstance)
{
    const wchar_t CLASS_NAME[] = L"PreviewWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    return CreateWindowEx(
        0, CLASS_NAME, L"Camera Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
        nullptr, nullptr, hInstance, nullptr);
}

// 获取设备友好名称
std::wstring GetFriendlyName(IMoniker* pMoniker)
{
    CComPtr<IPropertyBag> pPropBag;
    if (FAILED(pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag))))
        return L"<Unknown>";

    VARIANT varName;
    VariantInit(&varName);

    if (SUCCEEDED(pPropBag->Read(L"FriendlyName", &varName, nullptr))) {
        std::wstring name(varName.bstrVal, SysStringLen(varName.bstrVal));
        VariantClear(&varName);
        return name;
    }

    return L"<Unnamed>";
}

int main()
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM\n";
        return -1;
    }

    std::vector<CComPtr<IMoniker>> devices;

    // 枚举视频设备
    CComPtr<ICreateDevEnum> pDevEnum;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(hr)) {
        CComPtr<IEnumMoniker> pEnum;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (hr == S_OK) {
            CComPtr<IMoniker> pMoniker;
            ULONG fetched;
            while (pEnum->Next(1, &pMoniker, &fetched) == S_OK) {
                devices.push_back(pMoniker);
                std::wcout << devices.size() << L": " << GetFriendlyName(pMoniker) << std::endl;
                pMoniker.Release();
            }
        }
    }

    if (devices.empty()) {
        std::cout << "No camera devices found.\n";
        CoUninitialize();
        return 0;
    }

    // 选择设备
    std::cout << "Enter device index: ";
    int index = 0;
    std::cin >> index;
    if (index < 1 || index >(int)devices.size()) {
        std::cout << "Invalid index.\n";
        CoUninitialize();
        return 0;
    }

    // 绑定设备
    CComPtr<IBaseFilter> pCapFilter;
    hr = devices[index - 1]->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&pCapFilter));
    if (FAILED(hr)) {
        std::cerr << "Failed to bind selected camera.\n";
        CoUninitialize();
        return -1;
    }

    // 创建 Filter Graph
    CComPtr<IGraphBuilder> pGraph;
    CComPtr<ICaptureGraphBuilder2> pBuilder;
    CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraph));
    CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pBuilder));
    pBuilder->SetFiltergraph(pGraph);
    pGraph->AddFilter(pCapFilter, L"Video Capture");

    // 创建预览窗口
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    HWND hwnd = CreatePreviewWindow(hInstance);
    ShowWindow(hwnd, SW_SHOW);

    // 设置视频窗口属性
    hr = pBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pCapFilter, nullptr, nullptr);
    if (FAILED(hr)) {
        std::cerr << "RenderStream failed\n";
        CoUninitialize();
        return -1;
    }

    // 设置显示目标窗口
    CComPtr<IVideoWindow> pVidWin;
    pGraph->QueryInterface(&pVidWin);
    if (pVidWin) {
        pVidWin->put_Owner((OAHWND)hwnd);
        pVidWin->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);
        pVidWin->SetWindowPosition(0, 0, 640, 480);
        pVidWin->put_Visible(OATRUE);
    }

    // 开始播放
    CComPtr<IMediaControl> pMediaControl;
    pGraph->QueryInterface(&pMediaControl);
    pMediaControl->Run();

    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    pMediaControl->Stop();
    CoUninitialize();
    return 0;
}

