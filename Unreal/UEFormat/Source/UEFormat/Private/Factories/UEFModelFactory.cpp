// Fill out your copyright notice in the Description page of Project Settings.

#include "Factories/UEFModelFactory.h"
#include "AssetToolsModule.h"
#include "StaticMeshAttributes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "SkeletalMeshModelingToolsMeshConverter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Widgets/SkelMesh/UEFSkelMeshWidget.h"

UEFModelFactory::UEFModelFactory( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("uemodel; UEMODEL Mesh File"));
	SupportedClass = UStaticMesh::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bSilentImport = false;
	SettingsImporter = CreateDefaultSubobject<UEFSkelMeshImportOptions>(TEXT("Skeletal Mesh Options"));
}

UObject* UEFModelFactory::FactoryCreateFile(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UEFModelReader Data = UEFModelReader(Filename);
	//empty mesh
	if (!Data.Read() || Data.LODs.Num() == 0)
		return nullptr;
	
	if (Data.Skeleton.Bones.Num() > 0) //SkelMesh
		Name = "TEMP";
	UStaticMesh* StaticMesh = CreateStaticMesh(Data.LODs[0], Name, Parent, Flags);
	
	if(Data.LODs.Num() > 1) //has extra LODs
	{
		for(int i = 1; i < Data.LODs.Num(); i++)
		{
			UStaticMesh* LODStaticMesh = CreateStaticMesh(Data.LODs[i], "LOD_" + i, Parent, Flags);
			LODStaticMesh->PostEditChange();
			StaticMesh->SetCustomLOD(LODStaticMesh, i, "LOD" + i);
		}
	}
	
	if (Data.Skeleton.Bones.Num() > 0)
	{
		USkeletalMesh* SkeletalMesh = CreateSkeletalMeshFromStatic(Data.Header.ObjectName.c_str(), Data.Skeleton, Data.LODs, StaticMesh, Flags);
		StaticMesh->RemoveFromRoot();
		StaticMesh->MarkAsGarbage();
		return SkeletalMesh;
	}
	
	FAssetRegistryModule::AssetCreated(StaticMesh);
	StaticMesh->PostEditChange();
	return StaticMesh;
}

UStaticMesh* UEFModelFactory::CreateStaticMesh(FLODData& Data, FName Name, UObject* Parent, EObjectFlags Flags) {
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Parent, Name, Flags);

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();
	
    // Reserve space
    MeshDesc.ReserveNewVertices(Data.Vertices.Num());
    MeshDesc.ReserveNewVertexInstances(Data.Vertices.Num());
    MeshDesc.ReserveNewPolygons(Data.Indices.Num() / 3);
    MeshDesc.ReserveNewPolygonGroups(Data.Materials.Num());
    MeshDesc.SetNumUVChannels(Data.TextureCoordinates.Num());

	TArray<FVertexInstanceID> VertexIDs;
    TArray<FVertexInstanceID> VertexInstanceIDs;
    const auto VertexPositions = Attributes.GetVertexPositions();
    const auto VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
    const auto VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
    const auto VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    const auto VertexInstanceColors = Attributes.GetVertexInstanceColors();
    const auto VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
    VertexInstanceUVs.SetNumChannels(Data.TextureCoordinates.Num());


	TMap<int32, FVertexID> VertexIndexMap; //Store unique vertices
    for (auto i = 0; i < Data.Indices.Num(); i++) {
    	int index = Data.Indices[i];

    	FVertexID VertexID;
    	if (!VertexIndexMap.Contains(index)) {
    		VertexID = MeshDesc.CreateVertex();
    		VertexPositions.Set(VertexID, FVector3f(Data.Vertices[index].X, -Data.Vertices[index].Y, Data.Vertices[index].Z));
    		VertexIndexMap.Add(index, VertexID);
    	} else
    		VertexID = VertexIndexMap[index];
    	
        FVertexInstanceID VertexInstanceID = MeshDesc.CreateVertexInstance(VertexID);
        VertexInstanceIDs.Add(VertexInstanceID);
    	
        if (Data.Normals.Num() > 0) {
            VertexInstanceBinormalSigns.Set(VertexInstanceID, Data.Normals[index].X);
            VertexInstanceNormals.Set(VertexInstanceID, FVector3f(Data.Normals[index].Y, -Data.Normals[index].Z, Data.Normals[index].W));
        }
        if (Data.Tangents.Num() > 0)
            VertexInstanceTangents.Set(VertexInstanceID, FVector3f(Data.Tangents[index].X, -Data.Tangents[index].Y, Data.Tangents[index].Z));
        if (Data.VertexColors.Num() > 0)
            VertexInstanceColors.Set(VertexInstanceID, FVector4f(Data.VertexColors[0].Data[index]));
        for (auto u = 0; u < Data.TextureCoordinates.Num(); u++)
            VertexInstanceUVs.Set(VertexInstanceID, u, Data.TextureCoordinates[u][index]);
    }
	
	for (auto& [MatIndex, MatName, SlotName, FirstIndex, NumFaces] : Data.Materials) {
        FPolygonGroupID PolygonGroup = MeshDesc.CreatePolygonGroup();
        for (auto i = FirstIndex; i < FirstIndex + (NumFaces * 3); i += 3) {
            FVertexInstanceID& VI0 = VertexInstanceIDs[i];
            FVertexInstanceID& VI1 = VertexInstanceIDs[i + 1];
            FVertexInstanceID& VI2 = VertexInstanceIDs[i + 2];
            MeshDesc.CreatePolygon(PolygonGroup, { VI0, VI1, VI2 });
        }
        Attributes.GetPolygonGroupMaterialSlotNames()[PolygonGroup] = SlotName.c_str();

        FStaticMaterial StaticMat = FStaticMaterial();
        StaticMat.MaterialSlotName = SlotName.c_str();
        StaticMat.ImportedMaterialSlotName = SlotName.c_str();
		FString MaterialName = FString(MatName.c_str());
		MaterialName.ReplaceInline(TEXT("/Game/"), TEXT("/Game/Nikki/"));
		StaticMat.MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialName);
        StaticMesh->GetStaticMaterials().Add(StaticMat);
    }
	
    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
    StaticMesh->PostEditChange();
    StaticMesh->BuildFromMeshDescriptions({ &MeshDesc }, BuildParams);

    return StaticMesh;
}

USkeletalMesh* UEFModelFactory::CreateSkeletalMeshFromStatic(FString Name, FSkeletonData& SkeletonData, TArray<FLODData>& Datas, UStaticMesh* Mesh, EObjectFlags Flags)
{
	FReferenceSkeleton RefSkeleton;
	FSkeletalMeshImportData SkelMeshImportData;

	USkeleton* Skeleton = CreateSkeleton(Name, Mesh->GetPackage(), Flags, SkeletonData, RefSkeleton, SkelMeshImportData);

	USkeletalMeshFromStaticMeshFactory* SkeletalMeshFactory = NewObject<USkeletalMeshFromStaticMeshFactory>();
	SkeletalMeshFactory->StaticMesh = Mesh;
	SkeletalMeshFactory->Skeleton = Skeleton;
	SkeletalMeshFactory->ReferenceSkeleton = RefSkeleton;

	FString SkeletalMeshPath = FPackageName::GetLongPackagePath(Mesh->GetPackage()->GetName()) + "/" + Name;
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
	if (SkeletalMesh)
	{
		return SkeletalMesh;
	}
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	SkeletalMesh = Cast<USkeletalMesh>(AssetTools.CreateAsset(Name, FPackageName::GetLongPackagePath(Mesh->GetPackage()->GetName()), USkeletalMesh::StaticClass(), SkeletalMeshFactory));

	for (int32 LodIndex = 0; LodIndex < Datas.Num(); ++LodIndex)
	{
		auto& Data = Datas[LodIndex];
		{
			FMeshDescription* MeshDescription = SkeletalMesh->GetMeshDescription(LodIndex);
			if (!MeshDescription->IsEmpty())
			{
				SkelMeshImportData = FSkeletalMeshImportData::CreateFromMeshDescription(*MeshDescription);
			}
		}
		// SkeletalMesh->LoadLODImportedData(0, SkelMeshImportData);
		TArray<SkeletalMeshImportData::FRawBoneInfluence> Influences;
		for (auto i = 0; i < Data.Weights.Num(); i++)
		{
			const auto                                Weight = Data.Weights[i];
			SkeletalMeshImportData::FRawBoneInfluence Influence;
			Influence.BoneIndex   = int32(Weight.WeightBoneIndex);
			Influence.VertexIndex = Weight.WeightVertexIndex;
			Influence.Weight      = Weight.WeightAmount;
			Influences.Add(Influence);
		}
		SkelMeshImportData.Influences = Influences;
		// SkeletalMesh->SaveLODImportedData(LodIndex, SkelMeshImportData);
		FMeshDescription MeshDescription;
		if (SkelMeshImportData.GetMeshDescription(nullptr, &SkeletalMesh->GetLODInfo(LodIndex)->BuildSettings, MeshDescription))
		{
			SkeletalMesh->CreateMeshDescription(LodIndex, MoveTemp(MeshDescription));
			SkeletalMesh->CommitMeshDescription(LodIndex);
		}
	}

	SkeletalMesh->CalculateInvRefMatrices();
	const FSkeletalMeshBuildSettings BuildOptions;
	SkeletalMesh->GetLODInfo(0)->BuildSettings = BuildOptions;
	SkeletalMesh->SetImportedBounds(FBoxSphereBounds(FBoxSphereBounds3f(FBox3f(SkelMeshImportData.Points))));

	SkeletalMesh->SetSkeleton(Skeleton);
	SkeletalMesh->PostEditChange();
	FAssetRegistryModule::AssetCreated(SkeletalMesh);

	Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
	Skeleton->SetPreviewMesh(SkeletalMesh);
	Skeleton->PostEditChange();
	FAssetRegistryModule::AssetCreated(Skeleton);

	return SkeletalMesh;
}

USkeleton* UEFModelFactory::CreateSkeleton(FString Name, UPackage* ParentPackage, EObjectFlags Flags, FSkeletonData& Data, FReferenceSkeleton& RefSkeleton, FSkeletalMeshImportData& SkeletalMeshImportData)
{
	//Client/Content/Aki/Character/Role/FemaleM/Chun/R2T1ChunMd10011/Model/R2T1ChunMd10011_Skeleton.R2T1ChunMd10011_Skeleton
	// /Game/Assets/Buildin/Character/Main/Nikki/Material/MI_Eye_R_V6.MI_Eye_R_V6
	FString SkeletonPath = FString(Data.SkeletonPath.c_str());
	SkeletonPath.ReplaceInline(TEXT("/Game/"), TEXT("/Game/Nikki/"));
	FString Left;
	FString SkeletonName;
	if (!SkeletonPath.Split(TEXT("."), &Left, &SkeletonName))
	{
		SkeletonName = Name + "_Skeleton";
	}
	FString OldPath = *FPaths::Combine(FPaths::GetPath(ParentPackage->GetPathName()), SkeletonName);
	auto SkeletonPackage = CreatePackage(*OldPath);

	USkeleton* Skeleton = NewObject<USkeleton>(SkeletonPackage, FName(*SkeletonName), Flags);

	FReferenceSkeletonModifier RefSkeletonModifier(RefSkeleton, Skeleton);

	TArray<FString> AddedBoneNames;
	for (auto i = 0; i < Data.Bones.Num(); i++)
	{
		SkeletalMeshImportData::FBone Bone;
		Bone.Name = Data.Bones[i].BoneName.c_str();

		if (AddedBoneNames.Contains(Bone.Name)) continue;
		AddedBoneNames.Add(Bone.Name);
		Bone.ParentIndex = (i > 0) ? Data.Bones[i].BoneParentIndex : INDEX_NONE;

		FTransform3f Transform;
		auto Location = FVector3f(Data.Bones[i].BonePos.X, -Data.Bones[i].BonePos.Y, Data.Bones[i].BonePos.Z);
		Transform.SetLocation(Location);
		auto Rotation = FQuat4f(Data.Bones[i].BoneRot.X, -Data.Bones[i].BoneRot.Y, Data.Bones[i].BoneRot.Z, -Data.Bones[i].BoneRot.W);
		Transform.SetRotation(Rotation);

		SkeletalMeshImportData::FJointPos BonePos;
		BonePos.Transform = Transform;
		BonePos.Length = 1;
		BonePos.XSize = 1;
		BonePos.YSize = 1;
		BonePos.ZSize = 1;
		Bone.BonePos = BonePos;
		SkeletalMeshImportData.RefBonesBinary.Add(Bone);

		const FMeshBoneInfo BoneInfo(FName(*Bone.Name), Bone.Name, Bone.ParentIndex);
		RefSkeletonModifier.Add(BoneInfo, FTransform(Bone.BonePos.Transform));
	}
	return Skeleton;
}