#pragma once
#include <DirectXMath.h>
#include <vector>
#include <unordered_set>
#include <cmath>
#include <utility>

class MeshUtils
{
public:
    struct Edge
    {
        DirectX::XMFLOAT3 v1;
        DirectX::XMFLOAT3 v2;

        bool operator==(const Edge& other) const {
            return (DirectX::XMVector3Equal(XMLoadFloat3(&v1), XMLoadFloat3(&other.v1)) &&
                DirectX::XMVector3Equal(XMLoadFloat3(&v2), XMLoadFloat3(&other.v2))) ||
                (DirectX::XMVector3Equal(XMLoadFloat3(&v1), XMLoadFloat3(&other.v2)) &&
                    DirectX::XMVector3Equal(XMLoadFloat3(&v2), XMLoadFloat3(&other.v1)));
        }
    };

    struct EdgeHasher
    {
        std::size_t operator()(const Edge& edge) const {
            auto h1 = std::hash<float>()(edge.v1.x) ^ std::hash<float>()(edge.v1.y) ^ std::hash<float>()(edge.v1.z);
            auto h2 = std::hash<float>()(edge.v2.x) ^ std::hash<float>()(edge.v2.y) ^ std::hash<float>()(edge.v2.z);
            return h1 ^ h2;
        }
    };

    float CalculateEdgeLength(const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2)
    {
        DirectX::XMVECTOR vec1 = DirectX::XMLoadFloat3(&v1);
        DirectX::XMVECTOR vec2 = DirectX::XMLoadFloat3(&v2);
        DirectX::XMVECTOR edgeVec = DirectX::XMVectorSubtract(vec2, vec1);
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(edgeVec));
    }

    float CalculateAverageEdgeLength(const std::vector<DirectX::XMFLOAT3>& vertices, const std::vector<uint32_t>& indices)
    {
        std::unordered_set<Edge, EdgeHasher> edges;

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            DirectX::XMFLOAT3 v1 = vertices[indices[i]];
            DirectX::XMFLOAT3 v2 = vertices[indices[i + 1]];
            DirectX::XMFLOAT3 v3 = vertices[indices[i + 2]];

            edges.insert({ v1, v2 });
            edges.insert({ v2, v3 });
            edges.insert({ v3, v1 });
        }

        float totalLength = 0.0f;
        for (const auto& edge : edges)
        {
            totalLength += CalculateEdgeLength(edge.v1, edge.v2);
        }

        return totalLength / edges.size();
    }
};

