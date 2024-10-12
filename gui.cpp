#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <tlhelp32.h>
#include <stdio.h>
#include <string>
#include <iomanip>

#define START_ADDRESS 0x00831483
char start_addr[256] = "";
bool flag = false;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(LPCWSTR windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = L"class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		L"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

// The 8 possible operand values
const char modrm_value[8][4] = {
	"eax",
	"ecx",
	"edx",
	"ebx",
	"esp",
	"ebp",
	"esi",
	"edi"
};

// Table 2-2 in the reference document describes how to retrieve the operands from a ModR/M value
int decode_operand(unsigned char* buffer, int location) {
	if (buffer[location] >= 0xC0 && buffer[location] <= 0xFF) {
		ImGui::SameLine();
		ImGui::TextColored(ImColor(255, 0 ,0), "%s, %s", modrm_value[(buffer[location] >> 3) % 8], modrm_value[buffer[location] % 8]);
		return 1;
	}
	else if (buffer[location] >= 0x80 && buffer[location] <= 0xBF) {
		DWORD displacement = buffer[location + 1] | (buffer[location + 2] << 8) | (buffer[location + 3] << 16) | (buffer[location + 4] << 24);
		ImGui::SameLine();
		ImGui::TextColored(ImColor(255, 0, 0), "[%s+%x], %s ", modrm_value[(buffer[location] >> 3) % 8], displacement, modrm_value[buffer[location] % 8]);
		return 5;
	}
	else if (buffer[location] >= 0x40 && buffer[location] <= 0x7F) {
		ImGui::SameLine();
		ImGui::TextColored(ImColor(255, 0, 0), "%s, [%s+ %x]", modrm_value[(buffer[location] >> 3) % 8], modrm_value[buffer[location] % 8], buffer[location + 1]);
		return 2;
	}


	return 1;
}

void runDisAsm(bool Ready, int addr) {
	HANDLE process_snapshot = 0;
	HANDLE module_snapshot = 0;
	PROCESSENTRY32 pe32 = { 0 };
	MODULEENTRY32 me32;

	DWORD exitCode = 0;

	pe32.dwSize = sizeof(PROCESSENTRY32);
	me32.dwSize = sizeof(MODULEENTRY32);

	//https://docs.microsoft.com/en-us/windows/win32/toolhelp/taking-a-snapshot-and-viewing-processes
	process_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	Process32First(process_snapshot, &pe32);
	
	


	do {
		if (wcscmp(pe32.szExeFile, L"test_prog.exe") == 0) {
			module_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe32.th32ProcessID);

			HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, true, pe32.th32ProcessID);

			Module32First(module_snapshot, &me32);
			do {
				if (wcscmp(me32.szModule, L"test_prog.exe") == 0) {
					unsigned char* buffer = (unsigned char*)calloc(1, me32.modBaseSize);
					DWORD bytes_read = 0;
					ReadProcessMemory(process, (void*)me32.modBaseAddr, buffer, me32.modBaseSize, &bytes_read);

					DWORD loc = 0;
					unsigned int i = addr - (DWORD)me32.modBaseAddr;

					while (i < addr + 0x50 - (DWORD)me32.modBaseAddr) {
						ImGui::Text("%x:\t", i + (DWORD)me32.modBaseAddr);
						ImGui::SameLine();
						switch (buffer[i]) {
						case 0x1:
							ImGui::Text("ADD ");
							i++;
							i += decode_operand(buffer, i);
							break;
						case 0x29:
							ImGui::Text("SUB ");
							i++;
							i += decode_operand(buffer, i);
							break;
						case 0x74:
							ImGui::Text("JE ");
							ImGui::Text("%x", i + (DWORD)me32.modBaseAddr + 2 + buffer[i + 1]);
							i += 2;
							break;
						case 0x80:
							ImGui::Text("CMP ");
							i++;
							i += decode_operand(buffer, i);
							break;
						case 0x8D:
							ImGui::Text("LEA ");
							i++;
							i += decode_operand(buffer, i);
							break;
						case 0x8B:
						case 0x89:
							ImGui::Text("MOV ");
							i++;
							i += decode_operand(buffer, i);
							break;
						case 0xE8:
							ImGui::Text("CALL ");
							i++;
							loc = buffer[i] | (buffer[i + 1] << 8) | (buffer[i + 2] << 16) | (buffer[i + 3] << 24);
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "%x", loc + (i + (DWORD)me32.modBaseAddr) + 4);
							i += 4;
							break;
						case 0x6A:
							ImGui::Text("PUSH ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "0x%x", buffer[i]);
							i++;
							break;
						case 0x55:
							ImGui::Text("PUSH ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "ebp");
							break;
						case 0xA1:
							ImGui::Text("MOV ");
							i += 4;
							break;
						case 0x64:
							i++;
							ImGui::TextColored(ImColor(255, 0, 0), "%x FS", buffer[i]);
							i += 1;
							break;
						case 0x4D:
							ImGui::Text("DEC ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "ebp");
							break;
						case 0x5A:
							ImGui::Text("POP ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "edx");
							break;
						case 0x90:
							ImGui::Text("NOP ");
							i++;
							break;
						case 0x68:
							ImGui::Text("PUSH ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "offset, 0x%x", buffer[i]);
							i += 4;
							break;
						case 0x53:
							ImGui::Text("PUSH ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "ebx", buffer[i]);
							break;
						case 0x57:
							ImGui::Text("PUSH ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "edi", buffer[i]);
							break;
						case 0xBE:
							ImGui::Text("MOV ");
							i++;
							i += decode_operand(buffer, i);
							i+=3;
							break;

						case 0x83:
							ImGui::Text("CMP ");
							i++;
							i += decode_operand(buffer, i);
							i++;
							break;
						case 0x3B:
							ImGui::Text("CMP ");
							i+=2;
							i += decode_operand(buffer, i);
							break;
						case 0x72:
							ImGui::Text("JB ");
							i+=2;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "short", buffer[i]);
							break;
						case 0x56:
							ImGui::Text("PUSH ");
							i++;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "esi", buffer[i]);
							break;
						case 0x75:
							ImGui::Text("JNZ ");
							i+=2;
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "short", buffer[i]);
							break;
						case 0xF:
							ImGui::Text("CMOVA ");
							i++;
							i += decode_operand(buffer, i);
							break;
						case 0x00:
							ImGui::Text("ADD ");
							i += 2;
							i += decode_operand(buffer, i);
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "[eax], al", buffer[i]);
							break;
						case 0x40:
							ImGui::Text("ADD ");
							i++;
							i += decode_operand(buffer, i);
							ImGui::SameLine();
							ImGui::TextColored(ImColor(255, 0, 0), "[eax][0], al", buffer[i]);
							break;
						case 0xFF:
							ImGui::Text("#UD ");
							i += 2;
							i += decode_operand(buffer, i);
							break;
						default:
							ImGui::Text("%x", buffer[i]);
							i++;
							break;
						}

						ImGui::Text("\n");
					}

					free(buffer);
					break;
				}

			} while (Module32Next(module_snapshot, &me32));

			CloseHandle(process);
			break;
		}
	} while (Process32Next(process_snapshot, &pe32));

}

void run() {
	
	ImGui::InputText("##Username", start_addr, IM_ARRAYSIZE(start_addr));
	if (ImGui::Button("Start")) flag = true;
	if (flag)
	{
		int Addr = std::stoi(start_addr, nullptr, 16);
		runDisAsm(true, Addr);
	}

}

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGui::Begin(
		"disasm",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);

	run();

	ImGui::End();
}
