#include "pch.h"
#include "ShaderD3D12.h"

namespace EduEngine
{
	ShaderD3D12::ShaderD3D12(std::wstring	 fileName,
							 EDU_SHADER_TYPE type,
							 const LPCWSTR*  defines,
							 std::wstring	 entryPoint,
							 std::wstring	 target) :
		m_Type(type)
	{
		// https://github.com/Microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll

		Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
		Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;

		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

		pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);
		
		std::vector<LPCWSTR> pszArgs = {
			fileName.c_str(),
			L"-E", entryPoint.c_str(),
			L"-T", target.c_str(),
#if defined(DEBUG) | defined(_DEBUG)
			L"-Zi",
			L"-Qembed_debug",
#else
			L"-Fo",
			L"-O3",
			L"-Qstrip_debug",
			L"-Qstrip_reflect",
#endif
		};

		std::vector<std::wstring> macroStr;
		if (defines)
		{
			for (int i = 0; defines[i] != NULL; i += 2)
			{
				macroStr.emplace_back(defines[i]);
				macroStr.back().append(L"=").append(defines[i + 1]);

				pszArgs.push_back(L"-D");
				pszArgs.push_back(macroStr.back().c_str());
			}
		}

		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSource = nullptr;
		Microsoft::WRL::ComPtr<IDxcResult> pResults;

		pUtils->LoadFile(fileName.c_str(), nullptr, &pSource);
		DxcBuffer Source = {};
		Source.Ptr = pSource->GetBufferPointer();
		Source.Size = pSource->GetBufferSize();
		Source.Encoding = DXC_CP_ACP;

		pCompiler->Compile(
			&Source,
			pszArgs.data(),
			pszArgs.size(),
			pIncludeHandler.Get(),
			IID_PPV_ARGS(&pResults)
		);

		Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors = nullptr;
		pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

		if (pErrors && pErrors->GetStringLength() != 0)
			OutputDebugStringA(pErrors->GetStringPointer());

		HRESULT hrStatus;
		pResults->GetStatus(&hrStatus);

		THROW_IF_FAILED(hrStatus, L"Shader Compilation Failed");

		pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&m_ShaderBlob), nullptr);
	}

	D3D12_SHADER_BYTECODE ShaderD3D12::GetShaderBytecode() const
	{
		return D3D12_SHADER_BYTECODE
		{
			reinterpret_cast<BYTE*>(m_ShaderBlob->GetBufferPointer()),
			m_ShaderBlob->GetBufferSize()
		};
	}

	EDU_SHADER_TYPE ShaderD3D12::GetShaderType() const
	{
		return m_Type;
	}
}
