#define WIN32_LEAN_AND_MEAN
#include "InternalFunctions.h"

#include "MinHook/MinHook.h"
#if _WIN64 
#pragma comment(lib, "MinHook/libMinHook.x64.lib")
#else
#pragma comment(lib, "MinHook/libMinHook.x86.lib")
#endif

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

// Globals
HINSTANCE dll_handle;

uintptr_t GameAssemblyDll = MF::GetModuleAddress("GameAssembly.dll");
uintptr_t UnityPlayerDll = MF::GetModuleAddress("UnityPlayer.dll");
uintptr_t Die = 0x6B90B0;
uintptr_t Revive = 0x6BD670;
uintptr_t SetKillTimer = 0x6BF6E0;
uintptr_t IsKillTimerEnabled = 0x6C20F0;

typedef void(__fastcall* hookfunc)(__int64 a); //made to know what are hookable
void __fastcall EmptyFunction(__int64 a) { }
bool __fastcall ReturnTrueFunction(__int64 a) { return true; }
bool __fastcall ReturnFalseFunction(__int64 a) { return false; }

uintptr_t SetKillTimerHook = uintptr_t(GameAssemblyDll + SetKillTimer);
uintptr_t IsKillTimerEnabledHook = uintptr_t(GameAssemblyDll + IsKillTimerEnabled);
uintptr_t RemoveProtection = uintptr_t(GameAssemblyDll + 0x6BCAF0);
uintptr_t TurnOnProtection = uintptr_t(GameAssemblyDll + 0x6C15D0);

unsigned char* HookedBytes = nullptr;
unsigned char* HookedBytes2 = nullptr;
unsigned char* HookedTimerBytes = nullptr;

bool Show_Window = true;
bool LocalPlayer = false;
bool Impostor = false;
bool Host = false;
bool Protection = false;

bool GodMode = false;
bool SetKillTimerBool = false;

uintptr_t Settings = NULL;
uintptr_t SpeedSetting = (Settings + 0x14);
uintptr_t CrewmateVision = (Settings + 0x18);
uintptr_t ImpostorVision = (Settings + 0x1C);
uintptr_t KillCooldown = (Settings + 0x20);
uintptr_t MaxPlayers = (Settings + 0x08);
uintptr_t Players = (Settings + 0x0C);
uintptr_t CommonTasks =	(Settings + 0x24);
uintptr_t LongTasks = (Settings + 0x28);
uintptr_t ShortTasks = (Settings + 0x2C);
uintptr_t EmMettings = (Settings + 0x30);
uintptr_t EmCooldown = (Settings + 0x34);
uintptr_t Impostors = (Settings + 0x38);
uintptr_t KillDistance = (Settings + 0x40);
uintptr_t DiscussTime =	(Settings + 0x44);
uintptr_t VotingTime = (Settings + 0x48);
uintptr_t ConfirmEjects = (Settings + 0x4C);
uintptr_t TaskbarUpdate = (Settings + 0x50);
int MaxPlayersValue = NULL;
int PlayersValue = NULL;
float CrewmateVisionValue = NULL;
float ImpostorVisionValue = NULL;
float SpeedValue = NULL;
float KillCooldownValue = NULL;
int CommonTasksValue = NULL;
int LongTasksValue = NULL;
int ShortTasksValue = NULL;
int EmMeetingsValue = NULL;
float EmCooldownValue = NULL;
int ImpostorsValue = NULL;
int KillDistanceValue = NULL;
int DiscussionTimeValue = NULL;
int VotingTimeValue = NULL;
int ConfirmEjectsValue = NULL;
int TaskbarUpdatesValue = NULL;

int HOST	= NULL;
float HostX = NULL;
float HostY = NULL;
float HostZ = NULL;

typedef long(__stdcall* present)(IDXGISwapChain*, UINT, UINT);
present p_present;
present p_present_target;
bool get_present_pointer()
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	IDXGISwapChain* swap_chain;
	ID3D11Device* device;

	const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(
		NULL, 
		D3D_DRIVER_TYPE_HARDWARE, 
		NULL, 
		0, 
		feature_levels, 
		2, 
		D3D11_SDK_VERSION, 
		&sd, 
		&swap_chain, 
		&device, 
		nullptr, 
		nullptr) == S_OK)
	{
		void** p_vtable = *reinterpret_cast<void***>(swap_chain);
		swap_chain->Release();
		device->Release();
		//context->Release();
		p_present_target = (present)p_vtable[8];
		return true;
	}
	return false;
}

WNDPROC oWndProc;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
HWND window = NULL;
ID3D11Device* p_device = NULL;
ID3D11DeviceContext* p_context = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
static long __stdcall detour_present(IDXGISwapChain* p_swap_chain, UINT sync_interval, UINT flags) {
	if (!init) {
		if (SUCCEEDED(p_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&p_device)))
		{
			p_device->GetImmediateContext(&p_context);
			DXGI_SWAP_CHAIN_DESC sd;
			p_swap_chain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			p_device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX11_Init(p_device, p_context);
			init = true;
		}
		else
			return p_present(p_swap_chain, sync_interval, flags);
	}
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	//LOOPS ENTRY POINT
	HOST = MF::Read<int>(MF::FindDMAAddy(uintptr_t(UnityPlayerDll + 0x01478AC0), { 0x3C, 0x74, 0x24, 0x34, 0xC }));
	HostX = MF::Read<float>(MF::FindDMAAddy(uintptr_t(UnityPlayerDll + 0x01478AC0), { 0x3C, 0x74, 0x24, 0x34, 0x30 }));
	HostY = MF::Read<float>(MF::FindDMAAddy(uintptr_t(UnityPlayerDll + 0x01478AC0), { 0x3C, 0x74, 0x24, 0x34, 0x34 }));
	HostZ = MF::Read<float>(MF::FindDMAAddy(uintptr_t(UnityPlayerDll + 0x01478AC0), { 0x3C, 0x74, 0x24, 0x34, 0x38 }));
	Settings = MF::FindDMAAddy(uintptr_t(GameAssemblyDll + 0x020626FC), { 0x5C, 0x0, 0x24, 0x0 });
	SpeedSetting = (Settings + 0x14);
	CrewmateVision = (Settings + 0x18);
	ImpostorVision = (Settings + 0x1C);
	KillCooldown = (Settings + 0x20);
	MaxPlayers = (Settings + 0x08);
	Players = (Settings + 0x0C);
	CommonTasks = (Settings + 0x24);
	LongTasks = (Settings + 0x28);
	ShortTasks = (Settings + 0x2C);
	EmMettings = (Settings + 0x30);
	EmCooldown = (Settings + 0x34);
	Impostors = (Settings + 0x38);
	KillDistance = (Settings + 0x40);
	DiscussTime = (Settings + 0x44);
	VotingTime = (Settings + 0x48);
	ConfirmEjects = (Settings + 0x4C);
	TaskbarUpdate = (Settings + 0x50);
	//LOOPS END POINT
	
	// MENU ENTRY POINT
	if (LocalPlayer && Show_Window)
	{
		ImGui::Begin("LocalPlayer [Set / Read / HostOnly]");

		ImGui::Text("Settings");
		ImGui::InputFloat("Crewmate Vision", &CrewmateVisionValue, 0.25f);
		ImGui::InputFloat("Impostor Vision", &ImpostorVisionValue, 0.25f);
		ImGui::InputFloat("Speed", &SpeedValue, 0.25f);
		ImGui::InputFloat("Kill Cooldown", &KillCooldownValue, 0.25f);
		ImGui::InputInt("CommonTasks", &CommonTasksValue, 0.25f);
		ImGui::InputInt("LongTasks", &LongTasksValue, 0.25f);
		ImGui::InputInt("ShortTasks", &ShortTasksValue, 0.25f);
		ImGui::InputInt("Emergency Meetings", &EmMeetingsValue, 0.25f);
		ImGui::InputFloat("Emergency Cooldown", &EmCooldownValue, 0.25f);
		ImGui::InputInt("KillDistance", &KillDistanceValue, 0.25f);
		ImGui::InputInt("DiscussTime", &DiscussionTimeValue, 0.25f);
		ImGui::InputInt("VotingTime", &VotingTimeValue, 0.25f);
		ImGui::InputInt("ConfirmEjects", &ConfirmEjectsValue, 0.25f);
		ImGui::InputInt("TaskbarUpdate", &TaskbarUpdatesValue, 0.25f);
		if (ImGui::Button("Set", ImVec2(50, 20)))
		{
			MF::Write<float>(CrewmateVision, CrewmateVisionValue);
			MF::Write<float>(ImpostorVision, ImpostorVisionValue);
			MF::Write<float>(SpeedSetting, SpeedValue);
			MF::Write<float>(KillCooldown, KillCooldownValue);
			MF::Write<int>(CommonTasks, CommonTasksValue);
			MF::Write<int>(LongTasks, LongTasksValue);
			MF::Write<int>(ShortTasks, ShortTasksValue);
			MF::Write<int>(EmMettings, EmMeetingsValue);
			MF::Write<float>(EmCooldown, EmCooldownValue);
			MF::Write<int>(KillDistance, KillDistanceValue);
			MF::Write<int>(DiscussTime, DiscussionTimeValue);
			MF::Write<int>(VotingTime, VotingTimeValue);
			MF::Write<int>(ConfirmEjects, ConfirmEjectsValue);
			MF::Write<int>(TaskbarUpdate, TaskbarUpdatesValue);
		}
		ImGui::SameLine();
		if (ImGui::Button("Get", ImVec2(50, 20)))
		{
			CrewmateVisionValue = MF::Read<float>(CrewmateVision); 
			ImpostorsValue = MF::Read<float>(ImpostorVision); 
			SpeedValue = MF::Read<float>(SpeedSetting);
			KillCooldownValue = MF::Read<float>(KillCooldown);
			CommonTasksValue = MF::Read<int>(CommonTasks);
			LongTasksValue = MF::Read<int>(LongTasks);
			ShortTasksValue = MF::Read<int>(ShortTasks);
			EmMeetingsValue = MF::Read<int>(EmMettings);
			EmCooldownValue = MF::Read<float>(EmCooldown);
			KillDistanceValue = MF::Read<int>(KillDistance);
			DiscussionTimeValue = MF::Read<int>(DiscussTime);
			VotingTimeValue = MF::Read<int>(VotingTime);
			ConfirmEjectsValue = MF::Read<int>(ConfirmEjects);
			TaskbarUpdatesValue = MF::Read<int>(TaskbarUpdate);
		}

		ImGui::End();
	}

	if (Impostor && Show_Window)
	{
		ImGui::Begin("Local Impostor [Set]");

		if (ImGui::Button("Hook Kill Timer"))
		{
			SetKillTimerBool = !SetKillTimerBool;
			if (SetKillTimerBool)
			{
				HookedTimerBytes = MF::hookWithJump(IsKillTimerEnabledHook, SetKillTimerHook);
			}
			else
			{
				MF::unhookWithJump(IsKillTimerEnabledHook, HookedTimerBytes);
				HookedTimerBytes = nullptr;
			}
		}
		ImGui::SameLine();
		ImGui::Text("%s", SetKillTimerBool ? "On" : "Off");

		ImGui::End();
	}

	if (Host && Show_Window)
	{
		ImGui::Begin("Host [Read / Set / HostOnly]");

		ImGui::Text("[Players]");
		ImGui::Text("Max Players: %d", MF::Read<int>(MaxPlayers));
		ImGui::Text("Current Ammount Of Players: %d", MF::Read<int>(Players));

		ImGui::Text("Position: ");
		ImGui::InputFloat("X", &HostX);
		ImGui::InputFloat("Y", &HostY);
		ImGui::InputFloat("Z", &HostZ);

		ImGui::End();
	}

	if (Protection && Show_Window)
	{
		ImGui::Begin("Protection");

		if (ImGui::Button("Anti Die"))
		{
			GodMode = !GodMode;
			if (GodMode) {
				HookedBytes = MF::hookWithJump(
					uintptr_t(GameAssemblyDll + Die),
					uintptr_t(GameAssemblyDll + Revive)
				);
				HookedBytes2 = MF::hookWithJump(RemoveProtection, TurnOnProtection);
			}
			else {
				MF::unhookWithJump(GameAssemblyDll + Die, HookedBytes);
				MF::unhookWithJump(RemoveProtection, HookedBytes2);
			}
		}
		ImGui::SameLine();
		ImGui::Text("%s", GodMode ? "On" : "Off");

		ImGui::End();
	}

	if (Show_Window)
	{
		ImGui::Begin("AmongYou Version 1.3");
		if (ImGui::Button("LocalPlayer [HostOnly]"))
			LocalPlayer = !LocalPlayer;
		
		ImGui::SameLine();

		if (ImGui::Button("Host [HostOnly]"))
			Host = !Host;

		if (ImGui::Button("Impostor"))
			Impostor = !Impostor;
		
		ImGui::SameLine();

		if (ImGui::Button("Protection"))
			Protection = !Protection;
		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();

	p_context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return p_present(p_swap_chain, sync_interval, flags);
}

DWORD __stdcall EjectThread(LPVOID lpParameter) {
	Sleep(100);
	FreeLibraryAndExitThread(dll_handle, 0);
	Sleep(100);
	return 0;
}


//"main" loop
int WINAPI main()
{

	if (!get_present_pointer()) 
	{
		return 1;
	}

	MH_STATUS status = MH_Initialize();
	if (status != MH_OK)
	{
		return 1;
	}

	if (MH_CreateHook(reinterpret_cast<void**>(p_present_target), &detour_present, reinterpret_cast<void**>(&p_present)) != MH_OK) {
		return 1;
	}

	if (MH_EnableHook(p_present_target) != MH_OK) {
		return 1;
	}

	while (true) {
		Sleep(10);
		
		if (GetAsyncKeyState(VK_INSERT) & 1) {
			Show_Window = !Show_Window;
		}

		if (GetAsyncKeyState(VK_END)) {
			break;
		}
	}

	//Cleanup
	if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK) {
		return 1;
	}
	if (MH_Uninitialize() != MH_OK) {
		return 1;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = NULL; }
	if (p_context) { p_context->Release(); p_context = NULL; }
	if (p_device) { p_device->Release(); p_device = NULL; }
	SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));

	CreateThread(0, 0, EjectThread, 0, 0, 0);

	return 0;
}



BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		dll_handle = hModule;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, NULL);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{

	}
	return TRUE;
}