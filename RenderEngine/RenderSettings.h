#pragma once
#include "framework.h"
#include "TessellationPSOData.h"
#include "DeferredLightRendering.h"
#include "MotionBlurRendering.h"
#include "BloomRendering.h"

namespace AsyncComputeTessellation
{
	class RenderSettings
	{
	public:
		RenderSettings(AdaptiveTessellationCompute* tessellation,
					   DeferredLightRendering*		defferedRendering,
					   MotionBlurRendering*			motionBlurRendering,
					   BloomRendering*				bloomRendering);

		void Export();
		void Import();

	private:
		AdaptiveTessellationCompute* m_Tessellation;
		DeferredLightRendering* m_DefferedRendering;
		MotionBlurRendering* m_MotionBlurRendering;
		BloomRendering* m_BloomRendering;
	};
}