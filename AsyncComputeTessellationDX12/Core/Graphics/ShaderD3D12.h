#pragma once
#include "framework.h"

#include <dxcapi.h>

#pragma comment(lib,"dxcompiler.lib")

namespace EduEngine
{
	enum GRAPHICS_API EDU_SHADER_TYPE
	{
		EDU_SHADER_TYPE_VERTEX,
		EDU_SHADER_TYPE_GEOMETRY,
		EDU_SHADER_TYPE_PIXEL,
		EDU_SHADER_TYPE_COMPUTE,
	};

	class GRAPHICS_API ShaderD3D12
	{
	public:
		ShaderD3D12(std::wstring	fileName,
					EDU_SHADER_TYPE	type,
					const LPCWSTR*  defines,
					std::wstring	entryPoint,
					std::wstring	target);

		D3D12_SHADER_BYTECODE GetShaderBytecode() const;

		EDU_SHADER_TYPE GetShaderType() const;

	private:
		Microsoft::WRL::ComPtr<IDxcBlob> m_ShaderBlob;

		EDU_SHADER_TYPE m_Type;
	};
}

