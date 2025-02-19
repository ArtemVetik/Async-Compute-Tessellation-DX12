#pragma once

#include <DirectXMath.h>

namespace AsyncComputeTessellation
{
	struct Vertex
	{
		Vertex() {}
		Vertex(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT3& t,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Normal(n),
			TangentU(t),
			TexC(uv) {}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			TangentU(tx, ty, tz),
			TexC(u, v) {}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 TangentU;
		DirectX::XMFLOAT2 TexC;
	};

	struct VertexStride
	{
		VertexStride() {}
		VertexStride(
			const DirectX::XMFLOAT4& p,
			const DirectX::XMFLOAT4& n,
			const DirectX::XMFLOAT4& t,
			const DirectX::XMFLOAT4& uv) :
			Position(p),
			Normal(n),
			TangentU(t),
			TexC(uv) {
		}
		VertexStride(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v) :
			Position(px, py, pz, 1),
			Normal(nx, ny, nz, 1),
			TangentU(tx, ty, tz, 1),
			TexC(u, v, 0, 0) {
		}

		DirectX::XMFLOAT4 Position;
		DirectX::XMFLOAT4 Normal;
		DirectX::XMFLOAT4 TangentU;
		DirectX::XMFLOAT4 TexC;
	};

	struct VertexPT
	{
		VertexPT() {}
		VertexPT(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			TexC(uv) {}
		VertexPT(
			float px, float py, float pz,
			float u, float v) :
			Position(px, py, pz),
			TexC(u, v) {}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 TexC;
	};
}