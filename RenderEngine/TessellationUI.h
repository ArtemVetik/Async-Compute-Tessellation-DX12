#pragma once

#include "framework.h"

namespace AsyncComputeTessellation
{
	class AdaptiveTessellation;

	class TessellationUI
	{
	public:
		TessellationUI(AdaptiveTessellation* parent);

		void DrawUI(UINT screenWidth, UINT screenHeight);

	private:
		AdaptiveTessellation* m_Parent;
	};
}