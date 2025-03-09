#include "RenderSettings.h"
#include "../Libraries/include/nlohmann/json.hpp"

namespace AsyncComputeTessellation
{
	RenderSettings::RenderSettings(AdaptiveTessellationCompute* tessellation,
		DeferredLightRendering* defferedRendering,
		MotionBlurRendering* motionBlurRendering,
		BloomRendering* bloomRendering) :
		m_Tessellation(tessellation),
		m_DefferedRendering(defferedRendering),
		m_MotionBlurRendering(motionBlurRendering),
		m_BloomRendering(bloomRendering)
	{

	}

	void RenderSettings::Export(std::wstring configFile)
	{
		std::ofstream outFile(configFile);

		if (!outFile)
		{
			OutputDebugStringW(L"Error opening a file for writing!");
			return;
		}

		auto tessParams = m_Tessellation->GetParams();

		nlohmann::json tessellationJson =
		{
			{"MeshMode", tessParams.MeshMode},
			{"Dispatch1Count", tessParams.Dispatch1Count},
			{"Dispatch2Count", tessParams.Dispatch2Count},
			{"WireframeMode", tessParams.WireframeMode},
			{"FlatNormals", tessParams.FlatNormals},
			{"CPULodLevel", tessParams.CPULodLevel},
			{"Uniform", tessParams.Uniform},
			{"TargetLength", tessParams.TargetLength},
			{"UseDisplaceMapping", tessParams.UseDisplaceMapping},
			{"Freeze", tessParams.Freeze},
			{"UseFP16InShader", tessParams.UseFP16InShader},
			{"CB.SubdivisionLevel", tessParams.CB.SubdivisionLevel},
			{"CB.DisplaceFactor", tessParams.CB.DisplaceFactor},
			{"CB.WavesAnimationFlag", tessParams.CB.WavesAnimationFlag},
			{"CB.DisplaceLacunarity", tessParams.CB.DisplaceLacunarity},
			{"CB.DisplacePosScale", tessParams.CB.DisplacePosScale},
			{"CB.DisplaceH", tessParams.CB.DisplaceH},
		};

		auto lights = m_DefferedRendering->GetLights();
		nlohmann::json lightJsonArray = nlohmann::json::array();

		for (const auto& light : lights)
		{
			nlohmann::json lightJson = nlohmann::json
			{
				{"LightType", static_cast<int>(light->LightType)},
				{"Strength", {light->Strength.x, light->Strength.y, light->Strength.z}},
				{"FalloffStart", light->FalloffStart},
				{"Direction", {light->Direction.x, light->Direction.y, light->Direction.z}},
				{"FalloffEnd", light->FalloffEnd},
				{"Position", {light->Position.x, light->Position.y, light->Position.z}},
				{"SpotPower", light->SpotPower}
			};

			lightJsonArray.push_back(lightJson);
		}

		nlohmann::json chromaJson =
		{
			{"Chroma", m_DefferedRendering->GetChroma() },
		};

		nlohmann::json motionBlurJson =
		{
			{"SampleCount", m_MotionBlurRendering->GetSampleCount() },
			{"BlurAmount", m_MotionBlurRendering->GetBlurAmount() },
		};

		nlohmann::json bloomJson =
		{
			{"Threshold", m_BloomRendering->GetThreshold() },
			{"Intensity", m_BloomRendering->GetIntensity() },
			{"Scatter", m_BloomRendering->GetScatter() },
			{"Tint", { m_BloomRendering->GetTint()[0], m_BloomRendering->GetTint()[1], m_BloomRendering->GetTint()[2] } },
		};

		nlohmann::json settings =
		{
			{"TessellationParams", tessellationJson},
			{"Lights", lightJsonArray},
			{"ChromaticAberration", chromaJson},
			{"MotionBlur", motionBlurJson},
			{"Bloom", bloomJson},
		};

		outFile << settings.dump(4);
		outFile.close();
	}

	void RenderSettings::Import(std::wstring configFile)
	{
		std::ifstream inFile(configFile);

		if (!inFile)
		{
			OutputDebugStringW(L"Error opening a file for reading!");
			return;
		}

		nlohmann::json settings;
		inFile >> settings;

		nlohmann::json tessJson = settings.at("TessellationParams");

		TessellationParams params;

		params.MeshMode = static_cast<MeshMode>(tessJson.at("MeshMode").get<int>());
		params.Dispatch1Count = tessJson.at("Dispatch1Count").get<UINT>();
		params.Dispatch2Count = tessJson.at("Dispatch2Count").get<UINT>();
		params.WireframeMode = tessJson.at("WireframeMode").get<UINT>();
		params.FlatNormals = tessJson.at("FlatNormals").get<int>();
		params.CPULodLevel = tessJson.at("CPULodLevel").get<int>();
		params.Uniform = tessJson.at("Uniform").get<int>();
		params.TargetLength = tessJson.at("TargetLength").get<float>();
		params.UseDisplaceMapping = tessJson.at("UseDisplaceMapping").get<int>();
		params.Freeze = tessJson.at("Freeze").get<int>();
		params.UseFP16InShader = tessJson.at("UseFP16InShader").get<int>();

		params.CB.SubdivisionLevel = tessJson.at("CB.SubdivisionLevel").get<UINT>();
		params.CB.DisplaceFactor = tessJson.at("CB.DisplaceFactor").get<float>();
		params.CB.WavesAnimationFlag = tessJson.at("CB.WavesAnimationFlag").get<int>();
		params.CB.DisplaceLacunarity = tessJson.at("CB.DisplaceLacunarity").get<float>();
		params.CB.DisplacePosScale = tessJson.at("CB.DisplacePosScale").get<float>();
		params.CB.DisplaceH = tessJson.at("CB.DisplaceH").get<float>();

		nlohmann::json lightJsonArray = settings.at("Lights");

		std::vector<Light> lights;
		for (const auto& lightJson : lightJsonArray)
		{
			auto light = Light();

			int type;
			lightJson.at("LightType").get_to(type);
			light.LightType = static_cast<Light::Type>(type);

			std::vector<float> strength = lightJson.at("Strength").get<std::vector<float>>();
			light.Strength = { strength[0], strength[1], strength[2] };

			lightJson.at("FalloffStart").get_to(light.FalloffStart);

			std::vector<float> direction = lightJson.at("Direction").get<std::vector<float>>();
			light.Direction = { direction[0], direction[1], direction[2] };

			lightJson.at("FalloffEnd").get_to(light.FalloffEnd);

			std::vector<float> position = lightJson.at("Position").get<std::vector<float>>();
			light.Position = { position[0], position[1], position[2] };

			lightJson.at("SpotPower").get_to(light.SpotPower);

			lights.push_back(light);
		}

		nlohmann::json chomaticAberrationJson = settings.at("ChromaticAberration");
		m_DefferedRendering->SetChroma(chomaticAberrationJson.at("Chroma").get<float>());

		nlohmann::json motionBlurJson = settings.at("MotionBlur");
		m_MotionBlurRendering->SetSampleCount(motionBlurJson.at("SampleCount").get<int>());
		m_MotionBlurRendering->SetBlurAmount(motionBlurJson.at("BlurAmount").get<float>());

		nlohmann::json bloomJson = settings.at("Bloom");
		m_BloomRendering->SetThreshold(bloomJson.at("Threshold").get<float>());
		m_BloomRendering->SetIntensity(bloomJson.at("Intensity").get<float>());
		m_BloomRendering->SetScatter(bloomJson.at("Scatter").get<float>());
		m_BloomRendering->SetTint(bloomJson.at("Tint").get<std::vector<float>>().data());

		inFile.close();

		m_Tessellation->SetParams(params);
		m_DefferedRendering->SetLights(lights);
	}
}