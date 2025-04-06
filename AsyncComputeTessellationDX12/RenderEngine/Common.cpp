#include "Common.h"

namespace AsyncComputeTessellation
{
    std::wstring Common::OpenFolderDialog(bool folder, const wchar_t* description)
    {
		CoInitialize(nullptr);

		std::wstring selectedFolder;

		IFileDialog* pFileDialog = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));

		if (SUCCEEDED(hr)) {
			DWORD options = 0;
			pFileDialog->GetOptions(&options);
			pFileDialog->SetOptions(options | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));

			pFileDialog->SetTitle(description);
			hr = pFileDialog->Show(nullptr);
			if (SUCCEEDED(hr)) {
				IShellItem* pItem = nullptr;
				hr = pFileDialog->GetResult(&pItem);
				if (SUCCEEDED(hr)) {
					PWSTR folderPath = nullptr;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &folderPath);
					if (SUCCEEDED(hr)) {
						selectedFolder = folderPath;
						CoTaskMemFree(folderPath);
					}
					pItem->Release();
				}
			}
			pFileDialog->Release();
		}

		CoUninitialize();
		return selectedFolder;
    }
}