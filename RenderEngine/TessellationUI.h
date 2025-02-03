#pragma once

#include "framework.h"

namespace AsyncComputeTessellation
{
	class AdaptiveTessellation;

	class TessellationUI
	{
	public:
		TessellationUI(AdaptiveTessellation* parent);

		void DrawUI();

	private:
		AdaptiveTessellation* m_Parent;
	};
}