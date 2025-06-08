#include "imgui_wrapper.h"
#define IDI_APP_ICON 101

struct Application_Win32_Variables
{
    // DX11 specific variables
    ID3D11Device*           pd3dDevice;
    ID3D11DeviceContext*    pd3dDeviceContext;
    IDXGISwapChain*         pSwapChain;
    bool                    SwapChainOccluded;
    UINT                    ResizeWidth, ResizeHeight;
    ID3D11RenderTargetView* mainRenderTargetView;

    // win32 window handles
    WNDCLASSEXW wc;
    HWND hwnd;
};

// Creating a global variable for all important window properties
static Application_Win32_Variables internal_app_variables{};
static imgui_wrapper::Application_properties* internal_app_props{};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


void imgui_wrapper::init(const Application_properties* const app_props) {
    using namespace imgui_wrapper;

    internal_app_variables.pd3dDevice = nullptr;
    internal_app_variables.pd3dDeviceContext = nullptr;
    internal_app_variables.pSwapChain = nullptr;
    internal_app_variables.SwapChainOccluded = false;
    internal_app_variables.ResizeWidth = 0;
    internal_app_variables.ResizeHeight = 0;
    internal_app_variables.mainRenderTargetView = nullptr;

    // Inititializing internal variables
    internal_app_props = new Application_properties(app_props->height, app_props->width, app_props->window_title, app_props->icon);
    IM_ASSERT(internal_app_props != nullptr);
}

// 
/*
    Make a .rc file and add this line to it (path relative to .rc file)

        #define IDI_APP_ICON 101
        IDI_APP_ICON ICON "path/to/icon.ico"

    Then define IDI_APP_ICON in preprocesses
*/
void SetIcon() {
    // Load from resource
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON));

    //// Load from file
    //HICON hIcon = static_cast<HICON>(LoadImage(NULL, wcstr_icon_path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
    internal_app_variables.wc.hIcon = hIcon;
    SendMessage(internal_app_variables.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(internal_app_variables.hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    //delete[] wcstr_icon_path;
}

bool imgui_wrapper::start() {
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    internal_app_variables.wc = { sizeof(internal_app_variables.wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, internal_app_props->window_title, nullptr };
    
    // Set the icon
    if (internal_app_props->icon) {
        SetIcon();
    }
    
    ::RegisterClassExW(&internal_app_variables.wc);

    internal_app_variables.hwnd = ::CreateWindowW(internal_app_variables.wc.lpszClassName, internal_app_props->window_title, WS_OVERLAPPEDWINDOW, 100, 100,
        internal_app_props->width, internal_app_props->height, nullptr, nullptr, internal_app_variables.wc.hInstance, nullptr);



    // Initialize Direct3D
    if (!CreateDeviceD3D(internal_app_variables.hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(internal_app_variables.wc.lpszClassName, internal_app_variables.wc.hInstance);
        return false;
    }

    // Show the window
    ::ShowWindow(internal_app_variables.hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(internal_app_variables.hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(internal_app_variables.hwnd);
    ImGui_ImplDX11_Init(internal_app_variables.pd3dDevice, internal_app_variables.pd3dDeviceContext);

    return USER_startup();
}

// Load Fonts
// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
// - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
// - Read 'docs/FONTS.md' for more instructions and details.
// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
void imgui_wrapper::load_font(const char* font_path, const float size) {
    ImGuiIO& io = ImGui::GetIO();
    
    ImFontConfig font_conf{};
    font_conf.OversampleH = 3;
    font_conf.OversampleV = 3;
    font_conf.SizePixels = size;

    auto font =  io.Fonts->AddFontFromFileTTF(font_path, size, &font_conf);
    IM_ASSERT(font != nullptr);
}

void imgui_wrapper::load_multiple_size_font(const char* font_path, const float size) {
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImFontConfig font_conf{};
    font_conf.OversampleH = 3;
    font_conf.OversampleV = 3;
    font_conf.SizePixels = size;

    //internal_app_props->main_font = io.Fonts->AddFontFromFileTTF(font_path, size, &font_conf);
    //IM_ASSERT(internal_app_props->main_font != nullptr);
}

ID3D11Device* imgui_wrapper::get_d3d11_Device() {
    return internal_app_variables.pd3dDevice;
}

ID3D11DeviceContext* imgui_wrapper::get_d3d11_DeviceContext() {
    return internal_app_variables.pd3dDeviceContext;
}

HWND imgui_wrapper::get_HWND()
{
    return internal_app_variables.hwnd;
}

void imgui_wrapper::main_loop() {
    // Main loop
    static bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (internal_app_variables.SwapChainOccluded && internal_app_variables.pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        internal_app_variables.SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (internal_app_variables.ResizeWidth != 0 && internal_app_variables.ResizeHeight != 0)
        {
            CleanupRenderTarget();
            internal_app_variables.pSwapChain->ResizeBuffers(0, internal_app_variables.ResizeWidth, internal_app_variables.ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            internal_app_variables.ResizeWidth = internal_app_variables.ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Displaying our application
        static bool show_main_app = true;
        if (show_main_app)
            USER_main_app(show_main_app, internal_app_props);
        else
            break;

        // Rendering
        ImGui::Render();
        auto& clear_color = internal_app_props->clear_color;
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        internal_app_variables.pd3dDeviceContext->OMSetRenderTargets(1, &internal_app_variables.mainRenderTargetView, nullptr);
        internal_app_variables.pd3dDeviceContext->ClearRenderTargetView(internal_app_variables.mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = internal_app_variables.pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        internal_app_variables.SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }
}

void imgui_wrapper::exit() {
    delete[] internal_app_props;

    // User clean-up
    USER_cleanup();

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(internal_app_variables.hwnd);
    ::UnregisterClassW(internal_app_variables.wc.lpszClassName, internal_app_variables.wc.hInstance);
}

void imgui_wrapper::change_style(const uint8_t style) {
        
    switch (style)
    {
    case styles::Spectrum_Light:
        styles::StyleColorsSpectrum();
        break;
    case styles::Spectrum_Dark:
        styles::StyleColorsSpectrum();
        break;
    case styles::Old:
        styles::StyleOld();
        break;
    case styles::MS:
        styles::StyleMS();
        break;
    case styles::Windark:
        styles::StyleWindark();
        break;
    }
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd) {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &internal_app_variables.pSwapChain, &internal_app_variables.pd3dDevice, &featureLevel, &internal_app_variables.pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &internal_app_variables.pSwapChain, &internal_app_variables.pd3dDevice, &featureLevel, &internal_app_variables.pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();

    if (internal_app_variables.pSwapChain) { internal_app_variables.pSwapChain->Release(); internal_app_variables.pSwapChain = nullptr; }
    if (internal_app_variables.pd3dDeviceContext) { internal_app_variables.pd3dDeviceContext->Release(); internal_app_variables.pd3dDeviceContext = nullptr; }
    if (internal_app_variables.pd3dDevice) { internal_app_variables.pd3dDevice->Release(); internal_app_variables.pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    internal_app_variables.pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    internal_app_variables.pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &internal_app_variables.mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (internal_app_variables.mainRenderTargetView) { 
        internal_app_variables.mainRenderTargetView->Release(); 
        internal_app_variables.mainRenderTargetView = nullptr; 
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        internal_app_variables.ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        internal_app_variables.ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
