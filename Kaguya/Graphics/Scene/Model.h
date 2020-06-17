#pragma once
#include "Node.h"
#include "Material.h"

struct aiScene;
struct aiMaterial;
struct aiNode;

class Model
{
public:
	Model();
	Model(const char* FileName, float Scale);
	~Model();

	std::string GetName() const;

	const DirectX::BoundingBox& GetRootAABB() const;
	const Transform& GetRootTransform() const;

	const std::vector<Vertex>& GetVertices() const;
	const std::vector<UINT>& GetIndices() const;

	std::vector<std::unique_ptr<Mesh>>& GetMeshes();
	const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const;
	std::vector<std::unique_ptr<Material>>& GetMaterials();
	const std::vector<std::unique_ptr<Material>>& GetMaterials() const;

	void SetTransform(DirectX::FXMMATRIX M);
	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);

	void RenderImGuiWindow();

	static std::unique_ptr<Model> CreateSphereFromFile(const Material& material);
	static std::unique_ptr<Model> CreateGrid(float Width, float Depth, UINT M, UINT N, const Material& Material);
	static std::unique_ptr<Model> CreateBox(float Width, float Height, float Depth, UINT NumSubdivisions, const Material& Material);
private:
	friend class Renderer;

	std::string m_Name;
	std::unique_ptr<Node> m_pRootNode;

	std::vector<Vertex> m_Vertices;
	std::vector<UINT> m_Indices;

	std::vector<std::unique_ptr<Mesh>> m_pMeshes;
	std::vector<std::unique_ptr<Material>> m_pMaterials;

	std::unique_ptr<Node> ParseNode(INT& ID, const aiNode* paiNode);
	// Used for ImGui 
	Node* pSelectedNode = nullptr;
	void TraverseNode(Node* pNode);
	bool PushNode(Node* pNode);
	void PopNode();

	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<UINT> Indices;
	};
	static std::unique_ptr<Model> CreateFromMeshData(MeshData& meshData, std::string name, const Material& material);
	static void Subdivide(MeshData& meshData);
	static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
};