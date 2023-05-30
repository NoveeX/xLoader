#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include "../xorStr.h"
#include "fonts.h"
#include <TlHelp32.h>
#include <Windows.h>
#include <urlmon.h>
#include <exception>
#include <string>
#include "globals.h"
#include <wininet.h>

#pragma comment(lib,"Wininet.lib")
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

void gui::CreateHWindow(const char* windowName) noexcept
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
	windowClass.lpszClassName = XorStr("xloader");
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		XorStr("xloader"),
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

ImFont* fontbold;
ImFont* fontreg;
ImFont* icons;
void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);

	fontbold = io.Fonts->AddFontFromMemoryTTF(fonts::fontBold, sizeof(fonts::fontBold), 40.f);
	io.Fonts->AddFontDefault();
	icons = io.Fonts->AddFontFromMemoryTTF(fonts::icons, sizeof(fonts::icons), 40.f); // font: https://github.com/NoveeX/Cheat-menu-font
	fontreg = io.Fonts->AddFontFromMemoryTTF(fonts::fontReg, sizeof(fonts::fontReg), 47.f);
}

void gui::DestroyImGui() noexcept
{
	IM_FREE(fontbold);
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

DWORD GetProcId(const char* procName) {
	DWORD procid = 0;
	HANDLE hsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hsnapshot != INVALID_HANDLE_VALUE){
		PROCESSENTRY32 procEntry;
		procEntry.dwSize = sizeof(procEntry);

		if (Process32First(hsnapshot, &procEntry)) {
			while (Process32Next(hsnapshot, &procEntry)) {
				if (!_stricmp(procEntry.szExeFile, procName)) {
					procid = procEntry.th32ProcessID;
					break;
				}
			}
		}
		CloseHandle(hsnapshot);
		return procid;
		}
}

// function by https://stackoverflow.com/users/219136/pherricoxide
inline bool fileExists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

bool inject(const char* url, bool shouldInjectInSteam, const char* steamurl) {

	try
	{
		WinExec(XorStr("taskkill /f /im csgo.exe"), SW_HIDE);
	}
	catch (const std::exception&)
	{
		MessageBoxA(0, XorStr("Could not kill the CSGO process!"), XorStr("Error while injecting."), 0);
		return false;
	}

	if (shouldInjectInSteam) {
		std::string steamDllPath = getenv(XorStr("APPDATA"));
		steamDllPath += XorStr("\\steam.dll");
		if (fileExists(steamDllPath))
			remove(steamDllPath.c_str());
		HRESULT steamfileres = URLDownloadToFile(NULL, steamurl, steamDllPath.c_str(), 0, NULL);

		DWORD steamProcId = 0;


		steamProcId = GetProcId(XorStr("steam.exe"));
		Sleep(50);
		if (!steamProcId)
		{MessageBox(0, XorStr("Steam must be running!"), XorStr("Error while injecting."), 0);
		return false;
		}
			
		HANDLE hSteamProc = OpenProcess(PROCESS_ALL_ACCESS, 0, steamProcId);
		if (hSteamProc && hSteamProc != INVALID_HANDLE_VALUE) {
			void* loc = VirtualAllocEx(hSteamProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			
			WriteProcessMemory(hSteamProc, loc, steamDllPath.c_str(), strlen(steamDllPath.c_str()) + 1, 0);
			HANDLE hThread = CreateRemoteThread(hSteamProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, loc, 0, 0);

			if (hThread) {
				CloseHandle(hThread);
			}

			if (hSteamProc) {
				CloseHandle(hSteamProc);
			}
		}
	}
	Sleep(500);
	
	MessageBox(0, XorStr("You can now open csgo!"), XorStr("xLoader"), 0);
	DWORD csgoProcId = 0;

	while (!csgoProcId) {
		csgoProcId = GetProcId(XorStr("csgo.exe"));
		Sleep(50);
	}

	std::string csgoDllPath = getenv(XorStr("APPDATA"));
	csgoDllPath += "\\csgo.dll";
	if (fileExists(csgoDllPath))
		remove(csgoDllPath.c_str());
	HRESULT csgofileres = URLDownloadToFile(NULL, url, csgoDllPath.c_str(), 0, NULL);
	if (!SUCCEEDED(csgofileres)) {
		MessageBoxA(0, XorStr("Could not fetch cheat dll!"), XorStr("Error while injecting."), 0);
		return false;
	}

	Sleep(5000);
	
		
	HANDLE hCsgoProc = OpenProcess(PROCESS_ALL_ACCESS, 0, csgoProcId);
	if (hCsgoProc && hCsgoProc != INVALID_HANDLE_VALUE) {
		LPVOID ntOpenFile = GetProcAddress(LoadLibraryW(L"ntdll"), "NtOpenFile");
		if (ntOpenFile) {
			char originalBytes[5];
			memcpy(originalBytes, ntOpenFile, 5);
			WriteProcessMemory(hCsgoProc, ntOpenFile, originalBytes, 5, NULL);
		}

		void* loca = VirtualAllocEx(hCsgoProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (loca == NULL) {
			MessageBoxA(0, XorStr("Could not virtual allocate!"), XorStr("Error while injecting."), 0);
			return false;
		}
		if (!WriteProcessMemory(hCsgoProc, loca, csgoDllPath.c_str(), strlen(csgoDllPath.c_str()) + 1, 0)) {
			MessageBoxA(0, XorStr("There was a error writing to the process!"), XorStr("Error while injecting."), 0);
			return false;
		}
		HANDLE hThread = CreateRemoteThread(hCsgoProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, loca, 0, 0);
		if (hThread == NULL) {
			MessageBoxA(0, XorStr("Error at CreateRemoteThread()"), XorStr("Error while injecting."), 0);
			return false;
		}
		if (hThread) {
			CloseHandle(hThread);
		}

		if (hCsgoProc) {
			CloseHandle(hCsgoProc);
		}
	}

	return true;
}

struct cheat_template {
	const char* name;
	const char* desc;
	bool detected;
	const char* url;
	bool shouldInjectInSteam = false;
};

int currentItem = 0;
const char* detailspage_title = "";
const char* detailspage_desc = "";
bool detailspage_detected = false;

bool loaded = false;
const char* themes[] = { "Dark blue", "Dark red", "Dark green", "Old School"};

// How to add cheats into the loader:
// 
//   1. in the forum, add a folder called 'dl'
//   2. in /dl/ add another folder with the name of the cheat, for example 'osiris'
//   3. in /osiris/ add the cheat dll called 'csgo.dll' and optionally 'steam.dll' (csgo.dll is injected in csgo, steam.dll is injected in steam) 
//   4. in this array, add a cheat_template with this parameters:
// 
//		cheat_template(const char* name				-> the name that will appear in the list box and the details
//					   const char* description		-> description that will appear in the details
//					   bool detected				-> self explanatory
//					   const char* url				-> the name of the folder that we created in /dl/
//					   bool injectInSteam			-> you can decide if it will also get the steam.dll and inject in steam
// 
//   5. search for "// add here"
//   6. continue as i did
//   
// Done
cheat_template cheats[] = { cheat_template("Fatality Fix", "Fatality CS:GO cheat fixed!\nWorking as of 27 May", false, "fatality", true),
							cheat_template("Airflow crack", "Airflow hvh cheat" , false, "airflow", true),
							cheat_template("Pandora", "pandora" , false, "pandora", true),
							cheat_template("Osiris multihack", "Free open-source game\ncheat for CSGO written in\nmodern C++. GUI powered\nby Dear ImGui." , true, "osiris")
};

namespace setupclient {
	bool internetchecked;
	bool service;
}

int tab = 0;
int currentTheme = 0;
void gui::Render() noexcept
{
	
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	
	ImGui::PushFont(fontbold);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
	
	ImGui::Begin(
		XorStr("xLoader"),
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse
	);
	ImGui::SetCursorPos(ImVec2(0.f, 0));
	ImGui::PushClipRect(ImVec2(0, 0), ImVec2(23, 23), false);
	ImGui::SetCursorPos(ImVec2(0, 0));
	ImGui::PopFont();
	ImGui::PushFont(icons);
	if(tab == 1)
		if (ImGui::Button(XorStr("E"), ImVec2(23, 23), false))
			tab = 2;
	ImGui::PopFont();
	ImGui::PushFont(fontbold);
	if(tab == 2)
		if (ImGui::Button(XorStr("<"), ImVec2(23, 23), false))
			tab = 1;
	
	ImGui::PopClipRect();

	ImGui::PopStyleColor(3);
	ImGui::SetWindowFontScale(0.475f);
	
	if (tab == 0) {
	
		ImGui::SetCursorPos({ 8,30 });
		ImGui::SetWindowFontScale(0.42f);
		ImGui::Text(XorStr("[+] Checking internet connection"));
		if (!setupclient::internetchecked) {
			char url[128];
			strcat(url, XorStr("https://www.google.com/"));
			bool bConnect = InternetCheckConnection(url, FLAG_ICC_FORCE_CONNECTION, 0);
			if (!bConnect)
			{
				MessageBox(0, XorStr("You must have internet connection to use the loader!"), XorStr("Error"), MB_OK | MB_ICONEXCLAMATION);
				exit(EXIT_FAILURE);
			}
			setupclient::internetchecked = true;
		}
		else {
			ImGui::Text(XorStr("[+] Checking server status"));
			if (!setupclient::service) {
				char url[128];
				strcat(url, globals::forum.c_str());
				bool bConnect = InternetCheckConnection(url, FLAG_ICC_FORCE_CONNECTION, 0);
				if (!bConnect)
				{
					MessageBox(0, XorStr("Service offline!"), XorStr("Error"), MB_OK | MB_ICONEXCLAMATION);
					exit(EXIT_FAILURE);
				}
				setupclient::service = true;
			}
			else {
				//all good to go
				tab = 1;
			}
			
		}
	}
	else if (tab == 1) {
	
		if (!loaded) {
			detailspage_title = cheats[0].name;
			detailspage_desc = cheats[0].desc;
			detailspage_detected = cheats[0].detected;
			loaded = true;
		}
	
		// add here
		const char* items[] = { cheats[0].name , cheats[1].name , cheats[2].name , cheats[3].name};

		ImGui::SetCursorPos({ 5,30 });
		ImGui::BeginChild(XorStr("Cheats"), ImVec2((WIDTH - 3 * 4) / 2, 264), true);
		ImGui::SetNextItemWidth((WIDTH - 3 * 4) / 2 - 2);
		ImGui::SetWindowFontScale(0.475f);
		ImGui::SetCursorPos({ 0,0 });
		if (ImGui::ListBox("", &currentItem, items, 5, 11)) {
			detailspage_title = cheats[currentItem].name;
			detailspage_desc = cheats[currentItem].desc;
			detailspage_detected = cheats[currentItem].detected;
			//MessageBox(0, (globals::forum + (std::string)"/dl/client/" + (std::string)cheats[currentItem].url + (std::string)"/csgo.dll").c_str(), "", 0);
		}
		ImGui::EndChild();

		ImGui::SetCursorPos({ 3 + (WIDTH - 3 * 4) / 2 + 6,30 });
		ImGui::BeginChild(XorStr("Info"), ImVec2((WIDTH - 3 * 4) / 2 - 1, (HEIGHT - 17 - 4) / 1.45 + 8), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::SetCursorPos({ 5, 5 });
		ImGui::BeginChild(XorStr("Info2"), ImVec2((WIDTH - 5 * 4) / 2 - 1 - 6, (HEIGHT - 17 - 4) / 1.45 - 6 + 8), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		
		ImGui::SetWindowFontScale(0.6f);
		ImGui::Text(detailspage_title);
		ImGui::SetWindowFontScale(0.4f);
		ImGui::Separator();
		ImGui::PushFont(fontreg);
		ImGui::Text(detailspage_desc);
		ImGui::SetCursorPosY((HEIGHT - 17 - 4) / 1.45 - 8 - ImGui::CalcTextSize("a").y + 10);
		ImGui::Text(XorStr("Status:"));
		ImGui::SameLine();
		if (!detailspage_detected)
			ImGui::TextColored(ImVec4(0, 1, 0, 1), XorStr("safe"));
		else
			ImGui::TextColored(ImVec4(1, 0, 0, 1), XorStr("detected"));
		ImGui::PopFont();
		ImGui::EndChild();
		ImGui::EndChild();
		ImGui::SetCursorPos({ 3 + (WIDTH - 3 * 4) / 2 + 6,30 + (HEIGHT - 17 - 4) / 1.45 + 12});
		ImGui::SetWindowFontScale(0.55f);
		if (ImGui::Button(XorStr("Inject"), ImVec2((WIDTH - 3 * 4) / 2 - 1, 59),true))
			inject((globals::forum + (std::string)"/dl/client/" + (std::string)cheats[currentItem].url + (std::string)"/csgo.dll").c_str(), cheats[currentItem].shouldInjectInSteam, (globals::forum + (std::string)"/dl/client/" + (std::string)cheats[currentItem].url + (std::string)"/steam.dll").c_str());
		ImGui::SetWindowFontScale(0.475f);
	}
	else if (tab == 2) {
		
		ImGui::Text(((std::string)XorStr("Client version: ") + globals::version).c_str());
		ImGui::Separator();
		ImGui::Text(XorStr("Theme"));
		if (ImGui::Combo(XorStr(""), &currentTheme, themes, IM_ARRAYSIZE(themes))) {
			if (currentTheme == 0)
				ImGui::StyleColorsDark();
			if (currentTheme == 1)
				ImGui::Custom_dark_red();
			if (currentTheme == 2)
				ImGui::Custom_dark_green();
			if (currentTheme == 3)
				ImGui::Custom_classic();
			//if (currentTheme == 4)
			//	ImGui::Custom_light_blue();
			//if (currentTheme == 5)
			//	ImGui::Custom_light_red();
			//if (currentTheme == 6)
			//	ImGui::Custom_light_green();
		}

		ImGui::SetCursorPos({5, HEIGHT - 35});
		if(ImGui::Button(XorStr("Forum"), ImVec2(WIDTH - 5 - 5, 30), true))
			ShellExecute(0, 0, (globals::forum.c_str()), 0, 0, SW_SHOW);

	}
	ImGui::SetWindowFontScale(0.475f);
	ImGui::PopFont();
	ImGui::End();
}
