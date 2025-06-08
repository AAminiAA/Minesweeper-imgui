#include <iostream>
#include "MineSweeper.h"
#include "imgui.h"
#include "imgui_wrapper.h"
#include <string>


int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    imgui_wrapper::Application_properties props(GetSystemMetrics(SM_CYSCREEN), GetSystemMetrics(SM_CXSCREEN), L"Aminia's MineSweeper", "./resources/icons/icon.ico");
    imgui_wrapper::init(&props);
    if (!imgui_wrapper::start())
        return 1;

    imgui_wrapper::main_loop();
    imgui_wrapper::exit();

    return 0;
}
