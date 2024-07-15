#include "Bintree.h"

#include <stdexcept>

Bintree::Bintree(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    mDevice = device;
    mCommandList = commandList;
}

MeshGeometry* Bintree::BuildLeafMesh(uint32 cpuTessLevel)
{
    auto vertices = GetLeafVertices(cpuTessLevel);
    auto indices = GetLeafIndices(cpuTessLevel);

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(DirectX::XMFLOAT3);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);
    
    if (mLeafGeometry == nullptr)
        mLeafGeometry = std::make_unique<MeshGeometry>();

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mLeafGeometry->VertexBufferCPU));
    CopyMemory(mLeafGeometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mLeafGeometry->IndexBufferCPU));
    CopyMemory(mLeafGeometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mLeafGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice,
        mCommandList, vertices.data(), vbByteSize, mLeafGeometry->VertexBufferUploader);

    mLeafGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice,
        mCommandList, indices.data(), ibByteSize, mLeafGeometry->IndexBufferUploader);

    mLeafGeometry->VertexByteStride = sizeof(DirectX::XMFLOAT3);
    mLeafGeometry->VertexBufferByteSize = vbByteSize;
    mLeafGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    mLeafGeometry->IndexBufferByteSize = ibByteSize;
    
    return mLeafGeometry.get();
}

std::vector<DirectX::XMFLOAT3> Bintree::GetLeafVertices(uint32 level)
{
    std::vector<DirectX::XMFLOAT3> vertices;

    float num_row = 1 << level;
    float col = 0.0, row = 0.0;
    float d = 1.0 / float(num_row);
    
    while (row <= num_row)
    {
        while (col <= row)
        {
            vertices.push_back(DirectX::XMFLOAT3(col * d, 1.0 - row * d, 0));
            col++;
        }
        row++;
        col = 0;
    }

    return vertices;
}

std::vector<uint16_t> Bintree::GetLeafIndices(uint32 level)
{
    std::vector<uint16> indices;
    uint32 col = 0, row = 0;
    uint32 elem = 0, num_col = 1;
    uint32 orientation;
    uint32 num_row = 1 << level;
    auto new_triangle = [&]() {
        if (orientation == 0)
            return DirectX::XMINT3(elem, elem + num_col, elem + num_col + 1);
        else if (orientation == 1)
            return DirectX::XMINT3(elem, elem - 1, elem + num_col);
        else if (orientation == 2)
            return DirectX::XMINT3(elem, elem + num_col, elem + 1);
        else if (orientation == 3)
            return DirectX::XMINT3(elem, elem + num_col - 1, elem + num_col);
        else
            throw std::runtime_error("Bad orientation error");
        };
    while (row < num_row)
    {
        orientation = (row % 2 == 0) ? 0 : 2;
        while (col < num_col)
        {
            auto t = new_triangle();
            indices.push_back(t.x);
            indices.push_back(t.y);
            indices.push_back(t.z);
            orientation = (orientation + 1) % 4;
            if (col > 0) {
                auto t = new_triangle();
                indices.push_back(t.x);
                indices.push_back(t.y);
                indices.push_back(t.z);
                orientation = (orientation + 1) % 4;
            }
            col++;
            elem++;
        }
        col = 0;
        num_col++;
        row++;
    }
    return indices;
}
