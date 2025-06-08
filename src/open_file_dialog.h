#pragma once
#include <shlobj.h> // For BROWSEINFO
#include <string>

// Function to open the file dialog and get the selected file path
size_t OpenFileDialog(char* buffer, const size_t buff_size);

// Function to open the folder selection dialog and get the selected folder path
size_t OpenFolderDialog(char* buffer, const size_t buff_size);

// Function to open old-school dir selector
std::string OpenDirectoryDialog();