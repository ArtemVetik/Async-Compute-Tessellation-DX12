#include "SystemData.h"
#include <fstream>

SystemData::SystemData()
{
	currentBaseLocation = 0;
	
	indices = new uint16_t[UINT16_MAX];

	positions = new XMFLOAT3[UINT16_MAX];
	normals = new XMFLOAT3[UINT16_MAX];
	uvs = new XMFLOAT2[UINT16_MAX];
}

SystemData::~SystemData()
{
	delete[] indices;
	indices = 0;

	delete[] positions;
	positions = 0;

	delete[] normals;
	normals = 0;

	delete[] uvs;
	uvs = 0;
}

const uint16_t SystemData::GetCurrentBaseLocation()
{
	return currentBaseLocation;
}

const uint16_t* SystemData::GetIndices() 
{
	return indices;
}

const XMFLOAT3* SystemData::GetPositions() 
{
	return positions;
}

const XMFLOAT3* SystemData::GetNormals()
{
	return normals;
}

const XMFLOAT2* SystemData::GetUvs()
{
	return uvs;
}

SubSystem SystemData::GetSubSystem(char* subSystemName) const
{
	return subSystemData.at(subSystemName);
}

void SystemData::LoadOBJFile(char* fileName, Microsoft::WRL::ComPtr<ID3D12Device> device, char* subSystemName)
{
	SubSystem newSubSystem;
	newSubSystem.baseLocation = currentBaseLocation;
	newSubSystem.count = 0;

	// File input object
	std::ifstream obj(fileName);

	// Check for successful open
	if (!obj.is_open())
		return;

	// Variables used while reading the file
	std::vector<XMFLOAT3> objPositions;     // Positions from the file
	std::vector<XMFLOAT3> objNormals;       // Normals from the file
	std::vector<XMFLOAT2> objUvs;           // UVs from the file
	//std::vector<Vertex> verts;           // Verts we're assembling
	//std::vector<UINT> indices;           // Indices of these verts
	
	unsigned int vertexCounter = 0;        // Count of vertices/indices
	
	char chars[100];                     // String for line reading

										 // Still have data left?
	while (obj.good())
	{
		// Get the line (100 characters should be more than enough)
		obj.getline(chars, 100);

		// Check the type of line
		if (chars[0] == 'v' && chars[1] == 'n')
		{
			// Read the 3 numbers directly into an XMFLOAT3
			XMFLOAT3 norm;
			sscanf_s(
				chars,
				"vn %f %f %f",
				&norm.x, &norm.y, &norm.z);

			// Add to the list of normals
			objNormals.push_back(norm);
		}
		else if (chars[0] == 'v' && chars[1] == 't')
		{
			// Read the 2 numbers directly into an XMFLOAT2
			XMFLOAT2 uv;
			sscanf_s(
				chars,
				"vt %f %f",
				&uv.x, &uv.y);

			// Add to the list of uv's
			objUvs.push_back(uv);
		}
		else if (chars[0] == 'v')
		{
			// Read the 3 numbers directly into an XMFLOAT3
			XMFLOAT3 pos;
			sscanf_s(
				chars,
				"v %f %f %f",
				&pos.x, &pos.y, &pos.z);

			// Add to the positions
			objPositions.push_back(pos);
		}
		else if (chars[0] == 'f')
		{
			// Read the face indices into an array
			unsigned int i[12];
			int facesRead = sscanf_s(
				chars,
				"f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
				&i[0], &i[1], &i[2],
				&i[3], &i[4], &i[5],
				&i[6], &i[7], &i[8],
				&i[9], &i[10], &i[11]);

			// - Create the verts by looking up
			//    corresponding data from vectors
			// - OBJ File indices are 1-based, so
			//    they need to be adusted
			Vertex v1;
			v1.Position = objPositions[i[0] - 1];
			v1.UV = objUvs[i[1] - 1];
			v1.Normal = objNormals[i[2] - 1];

			Vertex v2;
			v2.Position = objPositions[i[3] - 1];
			v2.UV = objUvs[i[4] - 1];
			v2.Normal = objNormals[i[5] - 1];

			Vertex v3;
			v3.Position = objPositions[i[6] - 1];
			v3.UV = objUvs[i[7] - 1];
			v3.Normal = objNormals[i[8] - 1];

			// The model is most likely in a right-handed space,
			// especially if it came from Maya.  We want to convert
			// to a left-handed space for DirectX.  This means we 
			// need to:
			//  - Invert the Z position
			//  - Invert the normal's Z
			//  - Flip the winding order
			// We also need to flip the UV coordinate since DirectX
			// defines (0,0) as the top left of the texture, and many
			// 3D modeling packages use the bottom left as (0,0)

			// Flip the UV's since they're probably "upside down"
			v1.UV.y = 1.0f - v1.UV.y;
			v2.UV.y = 1.0f - v2.UV.y;
			v3.UV.y = 1.0f - v3.UV.y;

			// Flip Z (LH vs. RH)
			v1.Position.z *= -1.0f;
			v2.Position.z *= -1.0f;
			v3.Position.z *= -1.0f;

			// Flip normal Z
			v1.Normal.z *= -1.0f;
			v2.Normal.z *= -1.0f;
			v3.Normal.z *= -1.0f;

			positions[currentBaseLocation] = v1.Position;
			normals[currentBaseLocation] = v1.Normal;
			uvs[currentBaseLocation] = v1.UV;
			currentBaseLocation += 1;
			indices[vertexCounter] = vertexCounter;
			vertexCounter += 1;

			positions[currentBaseLocation] = v3.Position;
			normals[currentBaseLocation] = v3.Normal;
			uvs[currentBaseLocation] = v3.UV;
			currentBaseLocation += 1;
			indices[vertexCounter] = vertexCounter;
			vertexCounter += 1;

			positions[currentBaseLocation] = v2.Position;
			normals[currentBaseLocation] = v2.Normal;
			uvs[currentBaseLocation] = v2.UV;
			currentBaseLocation += 1;
			indices[vertexCounter] = vertexCounter;
			vertexCounter += 1;

			//// Add the verts to the vector (flipping the winding order)
			//verts.push_back(v1);
			//verts.push_back(v3);
			//verts.push_back(v2);

			// Add three more indices
			//indices.push_back(vertCounter); vertCounter += 1;
			//indices.push_back(vertCounter); vertCounter += 1;
			//indices.push_back(vertCounter); vertCounter += 1;

			// Was there a 4th face?
			if (facesRead == 12)
			{
				// Make the last vertex
				Vertex v4;
				v4.Position = positions[i[9] - 1];
				v4.UV = uvs[i[10] - 1];
				v4.Normal = normals[i[11] - 1];

				// Flip the UV, Z pos and normal
				v4.UV.y = 1.0f - v4.UV.y;
				v4.Position.z *= -1.0f;
				v4.Normal.z *= -1.0f;

				positions[currentBaseLocation] = v1.Position;
				normals[currentBaseLocation] = v1.Normal;
				uvs[currentBaseLocation] = v1.UV;
				currentBaseLocation += 1;
				indices[vertexCounter] = vertexCounter;
				vertexCounter += 1;

				positions[currentBaseLocation] = v4.Position;
				normals[currentBaseLocation] = v4.Normal;
				uvs[currentBaseLocation] = v4.UV;
				currentBaseLocation += 1;
				indices[vertexCounter] = vertexCounter;
				vertexCounter += 1;

				positions[currentBaseLocation] = v3.Position;
				normals[currentBaseLocation] = v3.Normal;
				uvs[currentBaseLocation] = v3.UV;
				currentBaseLocation += 1;
				indices[vertexCounter] = vertexCounter;
				vertexCounter += 1;

				//// Add a whole triangle (flipping the winding order)
				//verts.push_back(v1);
				//verts.push_back(v4);
				//verts.push_back(v3);

				// Add three more indices
				//indices.push_back(vertCounter); vertCounter += 1;
				//indices.push_back(vertCounter); vertCounter += 1;
				//indices.push_back(vertCounter); vertCounter += 1;
			}
		}
	}

	// Close the file and create the actual buffers
	obj.close();

	newSubSystem.count = vertexCounter;

	subSystemData[subSystemName] = newSubSystem;
}
