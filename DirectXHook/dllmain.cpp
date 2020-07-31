#include <Windows.h>
#include "detours.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <stdio.h>

typedef HRESULT(WINAPI* f_EndScene)(IDirect3DDevice9 * pDevice);
typedef HRESULT(WINAPI* f_DrawIndexedPrimitive)(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE PrimType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount);

f_EndScene oEndScene;
f_DrawIndexedPrimitive oDrawIndexedPrimitive;

LPDIRECT3D9 pD3D;
LPDIRECT3DDEVICE9 pD3DDevice;
HWND hWndD3D;

DWORD getVF(DWORD classInst, DWORD funcIndex)
{
	DWORD VFTable = *((DWORD*)classInst);
	DWORD hookAddress = VFTable + funcIndex * sizeof(DWORD);
	return *((DWORD*)hookAddress);
}

BOOL getD3DDevice()
{
	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);

	hWndD3D = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 600, 600, GetDesktopWindow(), NULL, wc.hInstance, NULL);

	LPDIRECT3D9 pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!pD3D)
		return FALSE;

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = hWndD3D;

	HRESULT res = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWndD3D, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pD3DDevice);
	if (FAILED(res))
		return FALSE;

	return TRUE;
}

bool WorldToScreen(LPDIRECT3DDEVICE9 pDevice, D3DXVECTOR3* pos, D3DXVECTOR3* out) {
	D3DVIEWPORT9 viewPort;
	D3DXMATRIX view, projection, world;

	pDevice->GetViewport(&viewPort);
	pDevice->GetTransform(D3DTS_VIEW, &view);
	pDevice->GetTransform(D3DTS_PROJECTION, &projection);
	D3DXMatrixIdentity(&world);

	D3DXVec3Project(out, pos, &viewPort, &projection, &view, &world);

	if (0 < out->z && out->z < 1) {
		return true;
	}

	return false;
}

HRESULT WINAPI Hooked_EndScene(IDirect3DDevice9* pDevice)
{
	static ID3DXFont* pFont = nullptr;
	if (pFont == nullptr)
	{
		D3DXFONT_DESC DXFont_DESC;
		ZeroMemory(&DXFont_DESC, sizeof(D3DXFONT_DESC));

		DXFont_DESC.Height = 14;
		DXFont_DESC.Width = 7;
		DXFont_DESC.Weight = FW_NORMAL;
		DXFont_DESC.MipLevels = D3DX_DEFAULT;
		DXFont_DESC.Italic = false;
		DXFont_DESC.CharSet = DEFAULT_CHARSET;
		DXFont_DESC.OutputPrecision = OUT_DEFAULT_PRECIS;
		DXFont_DESC.Quality = DEFAULT_QUALITY;
		DXFont_DESC.PitchAndFamily = DEFAULT_PITCH;
		DXFont_DESC.FaceName, TEXT("µ¸¿òÃ¼");

		D3DXCreateFontIndirect(pDevice, &DXFont_DESC, &pFont);
	}

	// googooma entity is 528 bytes
	// VECTOR3 : 0x130
	// HP : 0x200

	PDWORD entity_array = (PDWORD)((DWORD)GetModuleHandleA("FPS.exe") + 0x8F90);

	for (int i = 0; i < 50; i++)
	{
		D3DXVECTOR3 gogooma;
		ReadProcessMemory(GetCurrentProcess(), (LPCVOID)((DWORD)entity_array + i * 528 + 0x130), &gogooma, sizeof(D3DXVECTOR3), NULL);

		DWORD dwHP;
		ReadProcessMemory(GetCurrentProcess(), (LPCVOID)((DWORD)entity_array + i * 528 + 0x200), &dwHP, sizeof(DWORD), NULL);

		CHAR buf[255];
		snprintf(buf, sizeof(buf), "%d/%d", i, dwHP);

		D3DXVECTOR3 screen;
		if (WorldToScreen(pDevice, &gogooma, &screen))
		{
			RECT gogooma_rt = { (long)screen.x, (long)screen.y, 1024, 768 };
			pFont->DrawTextA(0, buf, -1, &gogooma_rt, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 255, 0, 0));
		}
	}

	return oEndScene(pDevice);
}

HRESULT WINAPI Hooked_DrawIndexedPrimitive(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE PrimType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
	static LPDIRECT3DTEXTURE9 pGreen = NULL;
	static LPDIRECT3DTEXTURE9 pTexture = NULL;
	static D3DLOCKED_RECT d3d_rect = { NULL };

	if (pGreen == NULL)
	{
		if (pDevice->CreateTexture(8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pGreen, NULL) == S_OK)
		{
			if (pDevice->CreateTexture(8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pTexture, NULL) == S_OK)
			{
				if (pTexture->LockRect(0, &d3d_rect, 0, D3DLOCK_DONOTWAIT | D3DLOCK_NOSYSLOCK) == S_OK)
				{
					for (UINT xy = 0; xy < 8 * 8; xy++)
					{
						((PDWORD)d3d_rect.pBits)[xy] = 0xFF00FF00;
					}

					pTexture->UnlockRect(0);
					pDevice->UpdateTexture(pTexture, pGreen);
					pTexture->Release();
				}
			}
		}
	}

	if (NumVertices == 1688 && primCount == 824)
	{
		pDevice->SetTexture(0, pGreen);
		pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		HRESULT ret = oDrawIndexedPrimitive(pDevice, PrimType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
		pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
		return ret;
	}

	return oDrawIndexedPrimitive(pDevice, PrimType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

DWORD WINAPI MainThread(LPVOID param)
{
	getD3DDevice();

	oEndScene = (f_EndScene)DetourFunction((PBYTE)getVF((DWORD)pD3DDevice, 42), (PBYTE)Hooked_EndScene);
	oDrawIndexedPrimitive = (f_DrawIndexedPrimitive)DetourFunction((PBYTE)getVF((DWORD)pD3DDevice, 82), (PBYTE)Hooked_DrawIndexedPrimitive);

	return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, MainThread, hModule, 0, 0);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}