#include "open_file_dialog.h"

// Function to open the file dialog and get the selected file path
size_t OpenFileDialog(char* buffer, const size_t buff_size) {
    OPENFILENAME ofn;       // common dialog box structure
    wchar_t szFile[MAX_PATH]{}; // buffer for file name

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"All Files\0*.*\0Binary Files\0*.out\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box
    if (GetOpenFileName(&ofn) == TRUE) {
        // Convert wchar_t to regular char
        size_t i{};
        wcstombs_s(&i, buffer, buff_size, ofn.lpstrFile, buff_size - 1);
        return i;
    }
    return 0;
}

// Function to open the folder selection dialog and get the selected folder path
size_t OpenFolderDialog(char* buffer, const size_t buff_size) {
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        return 0;
    }

    // Create the IFileDialog interface
    IFileDialog* pFileDialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog, reinterpret_cast<void**>(&pFileDialog));
    if (FAILED(hr)) {
        CoUninitialize();
        return 0;
    }

    // Set the options for folder selection
    DWORD dwOptions;
    pFileDialog->GetOptions(&dwOptions);
    pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);

    // Show the dialog
    hr = pFileDialog->Show(NULL);
    if (FAILED(hr)) {
        pFileDialog->Release();
        CoUninitialize();
        return 0;
    }

    // Get the selected folder path
    IShellItem* pItem = nullptr;
    hr = pFileDialog->GetResult(&pItem);
    if (FAILED(hr)) {
        pFileDialog->Release();
        CoUninitialize();
        return 0;
    }

    // Retrieve the file path from the selected item
    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr)) {
        pItem->Release();
        pFileDialog->Release();
        CoUninitialize();
        return 0;
    }

    // Convert the file path to a std::string
    size_t i{};
    wcstombs_s(&i, buffer, buff_size, pszFilePath, buff_size - 1);

    // Clean up
    CoTaskMemFree(pszFilePath);
    pItem->Release();
    pFileDialog->Release();
    CoUninitialize();

    return i;
}

std::string OpenDirectoryDialog() {
    BROWSEINFOA bi = { 0 };
    bi.lpszTitle = "Select Directory";
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);

    if (pidl != NULL) {
        // Get the name of the folder
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            return std::string(path);
        }
        // Free memory used by the item ID list
        CoTaskMemFree(pidl);
    }
    return std::string();
}