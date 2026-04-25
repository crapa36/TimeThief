#include "MorphingMeshData.h"

FDensitySet::FDensitySet()
{
	DensityTexture.SetNum(3);
}

bool FDensitySet::IsValid() const
{
	for (auto p : DensityTexture)
	{
		if (!p) return false;
		if (p->GetSizeX() * p->GetSizeY() * p->GetSizeZ() == 0) return false;
	}
	return true;
}

UMorphingMeshData::UMorphingMeshData()
{
	BaseMeshes.SetNum(3);
	DensityTextures.SetNum(3);
	Bounds.SetNum(3);
	UVVolumeTextures.SetNum(3);
	SkeletalMeshes.SetNum(3);
	BoneIndexTextures.SetNum(3);
}

void UMorphingMeshData::UpdateBox()
{
	for (int i = 0; i < 3; ++i)
	{
		if (BaseMeshes[i])
		{
			Bounds[i] = BaseMeshes[i]->GetBoundingBox();
		}
		else
		{
			Bounds[i] = FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector);
		}
	}
}

bool UMorphingMeshData::IsValid() const
{
	for (auto p : BaseMeshes)
	{
		if (!p) return false;
	}
	
	for (auto p : DensityTextures)
	{
		if (!p.IsValid()) return false;
	}
	
	return true;
}

bool UMorphingMeshData::IsSkeletalValid() const
{
	for (auto p : SkeletalMeshes)
	{
		if (!p) return false;
	}
	for (auto p : BoneIndexTextures)
	{
		if (!p) return false;
	}
	
	return true;
}
