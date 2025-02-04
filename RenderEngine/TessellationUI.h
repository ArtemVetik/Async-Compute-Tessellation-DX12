#pragma once

#include "framework.h"

namespace AsyncComputeTessellation
{
	class AdaptiveTessellationCompute;

	class TessellationUI
	{
	public:
		TessellationUI(AdaptiveTessellationCompute* parent);

		void DrawUI();

	private:
		AdaptiveTessellationCompute* m_Parent;
	};
}