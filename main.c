#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "noheadphones.h"
#include "headphones.h"

const CLSID CLSID_MMDeviceEnumerator = { 0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
const IID    IID_IMMDeviceEnumerator = { 0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };

bool init()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	return SUCCEEDED(hr);
}

void uninit()
{
	CoUninitialize();
}

IMMDeviceEnumerator* createDeviceEnum()
{
	IMMDeviceEnumerator* deviceEnum = NULL;
	HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, &deviceEnum);

	if (SUCCEEDED(hr))
		return deviceEnum;
	return NULL;
}

IMMDevice* getDefaultDevice(IMMDeviceEnumerator* deviceEnum)
{
	IMMDevice* defDevice = NULL;
	HRESULT hr = deviceEnum->lpVtbl->GetDefaultAudioEndpoint(deviceEnum, eRender, eConsole, &defDevice);

	if (SUCCEEDED(hr))
		return defDevice;
	return NULL;
}

wchar_t* getDeviceName(IMMDevice* device)
{
	IPropertyStore* store;
	PROPVARIANT prop = { 0 };
	HRESULT hr = device->lpVtbl->OpenPropertyStore(device, STGM_READ, &store);

	if (SUCCEEDED(hr))
	{
		hr = store->lpVtbl->GetValue(store, &PKEY_Device_FriendlyName, &prop);
		if (FAILED(hr))
			prop.pwszVal = NULL;

		store->lpVtbl->Release(store);
		return prop.pwszVal;
	}
	return NULL;
}


LRESULT APIENTRY msgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_APP + 1:
	{
		switch (lp)
		{
		case WM_RBUTTONUP:
		{
			bool* running = (bool*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
			*running = false;
			break;
		}
		}

		return 0;
	}

	default: return DefWindowProcW(hwnd, msg, wp, lp);
	}
}

const wchar_t* className = L"headphoneIcon";

HWND createMSGWindow()
{
	WNDCLASSEXW wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = msgWndProc;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.lpszClassName = className;
	if (RegisterClassExW(&wc))
	{
		HWND window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
		if (window)	return window;
		UnregisterClassW(className, GetModuleHandleW(NULL));
	}
	return NULL;
}

void updateWindow(HWND window)
{
	MSG msg = { 0 };
	while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

void addIcon(HWND window, NOTIFYICONDATAW* iconData)
{
	memset(iconData, 0, sizeof(*iconData));
	iconData->hWnd = window;
	iconData->cbSize = sizeof(*iconData);
	iconData->uID = 'EE';
	iconData->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	iconData->uCallbackMessage = WM_APP + 1;
	iconData->hIcon = NULL;
	Shell_NotifyIconW(NIM_ADD, iconData);
}

void swapIcon(NOTIFYICONDATAW* iconData, HICON icon)
{
	iconData->hIcon = icon;
	Shell_NotifyIconW(NIM_MODIFY, iconData);
}

void removeIcon(NOTIFYICONDATAW* iconData)
{
	Shell_NotifyIconW(NIM_DELETE, iconData);
}


wchar_t* loadHeadphoneNames()
{
	FILE* file = fopen("headphones.txt", "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		size_t length = ftell(file);
		fseek(file, 0, SEEK_SET);

		char* data = malloc(length + 1);
		if (data)
		{
			fread(data, 1, length, file);
			data[length] = 0;
			fclose(file);

			for (size_t i = 0; i < length; i++)
				if (data[i] == '\n')
					data[i] = 0;
			data[length] = 0;

			wchar_t* ret = malloc((length + 2) * sizeof(wchar_t));
			if (ret)
			{
				size_t pos = 0;
				for (size_t i = 0; i < length; i++)
				{
					if (data[i] != '\r')
					{
						ret[pos] = data[i];
						pos++;
					}
				}
				ret[pos] = 0;
				ret[pos + 1] = 0;

				free(data);
				return ret;
			}

			free(data);
		}
		fclose(file);
	}
	return NULL;
}


int main()
{
	if (init())
	{
		wchar_t* headphoneNames = loadHeadphoneNames();
		if (headphoneNames)
		{
			IMMDeviceEnumerator* deviceEnum = createDeviceEnum();
			if (deviceEnum)
			{
				HWND msgWindow = createMSGWindow();
				if (msgWindow)
				{
					HICON headphones = CreateIcon(GetModuleHandleW(NULL), 256, 256, 1, 32, &headphones_img.pixel_data, &headphones_img.pixel_data);
					HICON noheadphones = CreateIcon(GetModuleHandleW(NULL), 256, 256, 1, 32, &noheadphones_img.pixel_data, &noheadphones_img.pixel_data);

					NOTIFYICONDATAW iconData;
					addIcon(msgWindow, &iconData);

					updateWindow(msgWindow);

					wchar_t lastDeviceName[128] = { 0 };

					bool running = true;
					SetWindowLongPtrW(msgWindow, GWLP_USERDATA, (LONG_PTR)&running);

					while (running)
					{
						IMMDevice* defDevice = getDefaultDevice(deviceEnum);
						if (defDevice)
						{
							wchar_t* deviceName = getDeviceName(defDevice);
							if (lastDeviceName && deviceName)
							{
								if (lstrcmpW(deviceName, lastDeviceName) != 0)
								{
									size_t length = lstrlenW(deviceName);
									if (length > 127)
										length = 127;
									memcpy(lastDeviceName, deviceName, length * sizeof(wchar_t));
									lastDeviceName[length] = 0;

									memcpy(iconData.szTip, deviceName, sizeof(iconData.szTip));

									bool found = false;
									wchar_t* it = headphoneNames;
									while (*it)
									{
										if (lstrcmpW(it, deviceName) == 0)
										{
											found = true;
											break;
										}
										size_t len = lstrlenW(it);
										it += len + 1;
									}
									if (found)
										swapIcon(&iconData, headphones);
									else
										swapIcon(&iconData, noheadphones);
								}
							}

							//wprintf(L"\"%s\"\n", deviceName);

							defDevice->lpVtbl->Release(defDevice);
						}

						updateWindow(msgWindow);

						Sleep(100);
					}

					removeIcon(&iconData);
					DestroyWindow(msgWindow);
					UnregisterClassW(className, GetModuleHandleW(NULL));
				}
				deviceEnum->lpVtbl->Release(deviceEnum);
			}
			free(headphoneNames);
		}
		uninit();
	}

	return 0;
}
