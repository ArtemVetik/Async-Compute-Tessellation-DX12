#pragma once
#include "framework.h"

#include <string>
#include <shobjidl.h>

namespace AsyncComputeTessellation
{
	class Common
	{
	public:
		static std::wstring OpenFolderDialog(bool folder, const wchar_t* description);
	};
}