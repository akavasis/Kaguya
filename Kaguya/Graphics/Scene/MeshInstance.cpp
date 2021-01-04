#include "pch.h"
#include "MeshInstance.h"

MeshInstance::MeshInstance(const std::string& Name, size_t MeshIndex, size_t MaterialIndex)
	: Name(Name),
	Dirty(false),
	MeshIndex(MeshIndex),
	MaterialIndex(MaterialIndex)
{

}

void MeshInstance::RenderGui(int NumMeshes, int NumMaterials)
{
	if (ImGui::TreeNode(Name.data()))
	{
		NumMeshes = std::max(0, NumMeshes - 1);
		NumMaterials = std::max(0, NumMaterials - 1);

		Dirty |= ImGui::SliderInt("Mesh Index", (int*)&MeshIndex, 0, NumMeshes);
		Dirty |= ImGui::SliderInt("Material Index", (int*)&MaterialIndex, 0, NumMaterials);

		ImGui::TreePop();
	}
}

void MeshInstance::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	PreviousTransform = Transform;
	Transform.Translate(DeltaX, DeltaY, DeltaZ);
}

void MeshInstance::SetScale(float Scale)
{
	PreviousTransform = Transform;
	Transform.SetScale(Scale, Scale, Scale);
}

void MeshInstance::SetScale(float ScaleX, float ScaleY, float ScaleZ)
{
	PreviousTransform = Transform;
	Transform.SetScale(ScaleX, ScaleY, ScaleZ);
}

void MeshInstance::Rotate(float AngleX, float AngleY, float AngleZ)
{
	PreviousTransform = Transform;
	Transform.Rotate(AngleX, AngleY, AngleZ);
}