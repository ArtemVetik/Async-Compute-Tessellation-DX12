#include "TessellationUI.h"
#include "AdaptiveTessellationCompute.h"

#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	TessellationUI::TessellationUI(AdaptiveTessellationCompute* parent) :
		m_Parent(parent)
	{
	}

	void TessellationUI::DrawUI()
	{
		bool resetBuffers = false;
		bool buildPso = false;
		bool updateLeafMesh = false;
		bool initTessData = false;

		static bool open = false;

		if (!open)
		{
			if (ImGui::Button("Tessellation"))
				open = true;
		}
		else
		{
			if (ImGui::Button("Tessellation [X]"))
				open = false;
		}

		if (!open)
			return;

		if (ImGui::Begin("Tessellation parameters", &open))
		{
			ImGui::SeparatorText("View Mode");

			if (ImGui::Combo("Mode", (int*)&m_Parent->m_Params.MeshMode, "Terrain\0Mesh\0\0"))
			{
				buildPso = true;
				resetBuffers = true;
				initTessData = true;
			}

			ImGui::DragInt("Dispatch 1 Count", (int*)&m_Parent->m_Params.Dispatch1Count, 10.0f);
			ImGui::DragInt("Dispatch 2 Count", (int*)&m_Parent->m_Params.Dispatch2Count, 100.0f);

			if (ImGui::Checkbox("Wireframe Mode", &m_Parent->m_Params.WireframeMode))
				buildPso = true;

			if (m_Parent->m_Params.WireframeMode == false)
			{
				if (ImGui::Checkbox("Flat Normals", &m_Parent->m_Params.FlatNormals))
					buildPso = true;
			}

			ImGui::SeparatorText("LoD");

			if (ImGui::SliderInt("CPU Lod Level", &m_Parent->m_Params.CPULodLevel, 0, 4))
			{
				updateLeafMesh = true;
				initTessData = true;
			}

			if (ImGui::Checkbox("Uniform", &m_Parent->m_Params.Uniform))
				buildPso = true;

			if (m_Parent->m_Params.Uniform)
			{
				ImGui::SameLine();
				int subdLevel = m_Parent->m_Params.CB.SubdivisionLevel;
				if (ImGui::SliderInt(" ", &subdLevel, 0, 16))
				{
					m_Parent->m_Params.CB.SubdivisionLevel = (UINT)subdLevel;
					initTessData = true;
				}
			}

			float expo = log2(m_Parent->m_Params.TargetLength);
			if (ImGui::SliderFloat("Edge Length (2^x)", &expo, 1.2f, 10))
			{
				m_Parent->m_Params.TargetLength = std::pow(2, expo);
				initTessData = true;
			}

			if (m_Parent->m_Params.MeshMode == MeshMode::TERRAIN)
			{
				ImGui::SeparatorText("Displace");

				if (ImGui::Checkbox("Displace Mapping", &m_Parent->m_Params.UseDisplaceMapping))
					buildPso = true;

				if (m_Parent->m_Params.UseDisplaceMapping)
				{
					if (ImGui::SliderFloat("Displace Factor", &m_Parent->m_Params.CB.DisplaceFactor, 1, 20)) initTessData = true;

					bool wavesAnimation = m_Parent->m_Params.CB.WavesAnimationFlag;
					if (ImGui::Checkbox("Animated", &wavesAnimation)) { m_Parent->m_Params.CB.WavesAnimationFlag = wavesAnimation; initTessData = true; }
					if (ImGui::SliderFloat("Displace Lacunarity", &m_Parent->m_Params.CB.DisplaceLacunarity, 0.7, 3)) initTessData = true;
					if (ImGui::SliderFloat("Displace PosScale", &m_Parent->m_Params.CB.DisplacePosScale, 0.01, 0.05)) initTessData = true;
					if (ImGui::SliderFloat("Displace H", &m_Parent->m_Params.CB.DisplaceH, 0.1, 2)) initTessData = true;
				}
			}

			ImGui::SeparatorText("Compute Settings");
			ImGui::Checkbox("Freeze", &m_Parent->m_Params.Freeze);

			if (ImGui::Checkbox("Use FP16 in shader", &m_Parent->m_Params.UseFP16InShader))
				buildPso = true;

			ImGui::Checkbox("Read SubdCount", &m_Parent->m_CopySubdCount);

			if (m_Parent->m_CopySubdCount)
			{

				UINT subdCount = 0;
				m_Parent->m_SubdCounterCpu->ReadData(0, subdCount);
				ImGui::Text("%u triangles", subdCount * m_Parent->m_Params.CB.IndicesCount);
				ImGui::SameLine();
				ImGui::Text("(%u keys x %u indices)", subdCount, m_Parent->m_Params.CB.IndicesCount);
			}
		}
		ImGui::End();

		auto screenRes = std::max(m_Parent->m_SwapChain->GetWidth(), m_Parent->m_SwapChain->GetHeight());
		if (screenRes != m_Parent->m_Params.CB.ScreenRes)
		{
			m_Parent->m_Params.CB.ScreenRes = screenRes;
			initTessData = true;
		}

		if (buildPso) m_Parent->BuildPSO();
		if (resetBuffers) m_Parent->ResetBuffers();
		if (updateLeafMesh) m_Parent->UpdateLeafMesh();
		if (initTessData) m_Parent->InitTessData();
	}
}