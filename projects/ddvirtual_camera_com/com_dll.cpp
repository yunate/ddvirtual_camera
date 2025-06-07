
#include "ddvirtual_camera_com/stdafx.h"

#include "ddvc_factory.h"

#include <windows.h>


#pragma comment(lib, "ddbase.lib")
#pragma comment(lib,"rpcrt4.lib")
#pragma comment(lib,"Strmiids.lib")

static HMODULE g_hInstance = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
  switch (reason)
  {
  case DLL_PROCESS_ATTACH:
    (void)::CoInitialize(NULL);
    g_hInstance = hModule;
    break;
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
  case DLL_PROCESS_DETACH:
    break;
  }
  return TRUE;
}

STDAPI DllRegisterServer()
{
  wchar_t modulePath[MAX_PATH];
  (void)::GetModuleFileNameW(g_hInstance, modulePath, MAX_PATH);
  return ddvc_factory::get_instance().Register(modulePath);
}

STDAPI DllUnregisterServer()
{
  return ddvc_factory::get_instance().UnRegister();
}

_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
    if (riid != IID_IClassFactory &&
        riid != IID_IUnknown &&
        riid != ddvc_factory::get_instance().GetComDesc().comClsId) {
        return E_NOINTERFACE;
    }

    return ddvc_factory::get_instance().DllGetClassObject(rclsid, ppv);
}

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow(void)
{
    if (ddvc_factory::get_instance().GetLockCnt() == 0) {
        return S_OK;
    }
    return S_FALSE;
}

STDAPI DllInstall(bool install)
{
  HRESULT hr = S_FALSE;
  if (install) {
    hr = DllRegisterServer();
  } else {
    hr = DllUnregisterServer();
  }
  return hr;
}
