# -*- coding:utf-8 -*-
import os, sys, json
import unreal

print("Import UEFormat Start!")

ConvertAbsPathToGamePath = None;
ConvertGamePathToGamePath = None;
RootPath = None;
GameType = "Nikki"
# nikki
# G:/GameExport/Game/Assets/Buildin/Character/Main/Nikki/Nikki_SkMesh.uemodel
def NikkiConvertAbsPathToGamePath(AbsPath):
    _, right = AbsPath.split("/Game/", 1)
    result = "/Game/Nikki/" + right
    return result;

# /Game/Assets/Buildin/Character/Main/Nikki/Texture/T_Head_D_3S_02.0
def NikkiConvertGamePathToGamePath(GamePath):
    print("NikkiConvertGamePathToGamePath")
    print(GamePath)
    _, right = GamePath.split("Game/", 1)
    result = "Game/Nikki/" + right
    return result;

if GameType == "Nikki":
    ConvertAbsPathToGamePath = NikkiConvertAbsPathToGamePath
    ConvertGamePathToGamePath = NikkiConvertGamePathToGamePath
    RootPath = r"G:/GameExport/Game"



# Check
if ConvertAbsPathToGamePath == None or ConvertGamePathToGamePath == None or RootPath == None:
   raise RuntimeError("Incomplete parameter")

# G:/GameExport/Client/Content/Aki/Character/Role/FemaleM/Chun/R2T1ChunMd10011/Model/R2T1ChunMd10011.uemodel
SkeletalMeshes = []
AnimSequences = []
Textures = []
Materials = []
for path, _, filenames in os.walk(RootPath):
    path = path.replace('\\', '/')
    for filename in filenames:
        if filename.endswith('uemodel'):
            SkeletalMeshes.append((path, filename))
            continue

        if filename.endswith('ueanim'):
            AnimSequences.append((path, filename))
            continue
        if filename.endswith('png'):
            Textures.append((path, filename))
            continue
        if filename.endswith('json'):
            Materials.append((path, filename))
            continue
AssetTool = unreal.AssetToolsHelpers.get_asset_tools()

bImportTexture = False
if bImportTexture:
    print("ImportTexture")
    for abspath, filename in Textures:
        GamePath = ConvertAbsPathToGamePath(abspath)
        print(GamePath, filename)
        FileAbsPath = abspath + "/" + filename
        ImportData = unreal.AutomatedAssetImportData()
        ImportData.destination_path = GamePath
        ImportData.filenames = [FileAbsPath]
        ImportData.replace_existing = True
        ImportedAsset = AssetTool.import_assets_automated(ImportData)

bImportMaterial = False

def ConvertMaterialAssetGamePath(InPath):
    if InPath.startswith("/Game/"):
        Path, Index = InPath.split('.', 1)
        GamePath = ConvertGamePathToGamePath(Path)
        return GamePath
    return InPath

if bImportMaterial:
    print("ImportMaterial")
    MaterialInstanceList = []
    MaterialList = []

    # create material
    for abspath, filename in Materials:
        GamePath = ConvertAbsPathToGamePath(abspath)
        print(GamePath, filename)
        FileAbsPath = abspath + "/" + filename
        with open(FileAbsPath, "r", encoding='utf-8') as f:
            data = json.load(f)
            if data["Type"] == "MaterialInstanceConstant":
                ImportedAsset = unreal.load_object(None, GamePath + "/" + data["Name"])
                if not ImportedAsset:
                    ImportedAsset = AssetTool.create_asset(data["Name"], GamePath,
                                                           unreal.MaterialInstanceConstant.static_class(), None)
                ParentPath = ConvertMaterialAssetGamePath(data["Properties"]["Parent"]["ObjectPath"])
                # for Parameter in data["Properties"]["ScalarParameterValues"]:
                #     unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(ImportedAsset, Parameter[
                #         "ParameterInfo"]["Name"], Parameter["ParameterValue"])
                # for Parameter in data["Properties"]["VectorParameterValues"]:
                #     unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(ImportedAsset, Parameter[
                #         "ParameterInfo"]["Name"], unreal.LinearColor(Parameter["ParameterValue"]["R"],
                #                                                      Parameter["ParameterValue"]["G"],
                #                                                      Parameter["ParameterValue"]["B"],
                #                                                      Parameter["ParameterValue"]["A"]))
                if "TextureParameterValues" in data["Properties"]:
                    for Parameter in data["Properties"]["TextureParameterValues"]:
                        if Parameter["ParameterValue"] is None:
                            continue
                        TexturePath = ConvertMaterialAssetGamePath(Parameter["ParameterValue"]["ObjectPath"])
                        TextureObj = unreal.load_object(None, TexturePath)
                        if TextureObj:
                            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(ImportedAsset,
                                                                                                    Parameter[
                                                                                                        "ParameterInfo"][
                                                                                                        "Name"], TextureObj)
                # for Parameter in data["Properties"]["StaticParameters"]["StaticSwitchParameters"]:
                #     unreal.MaterialEditingLibrary.set_material_instance_static_switch_parameter_value(ImportedAsset,
                #                                                                                       Parameter[
                #                                                                                           "ParameterInfo"][
                #                                                                                           "Name"],
                #                                                                                       Parameter[
                #                                                                                           "Value"])
                MaterialInstanceList.append((ImportedAsset, GamePath + '/' + data["Name"], ParentPath))
            if data["Type"] == "Material":
                # 父材质
                ImportedAsset = unreal.load_object(None, GamePath + "/" + data["Name"])
                if not ImportedAsset:
                    ImportedAsset = AssetTool.create_asset(data["Name"], GamePath, unreal.Material.static_class(), None)


                MaterialList.append((ImportedAsset, GamePath + '/' + data["Name"]))

    # setup material
    print("setup material")
    for Asset, Path, ParentPath in MaterialInstanceList:
        print(ParentPath)
        parent = unreal.load_object(None, ParentPath)
        if parent:
            unreal.MaterialEditingLibrary.set_material_instance_parent(Asset, parent)

# print(AnimSequences)
bImportSkeletalMesh = True
if bImportSkeletalMesh:
    print("ImportSkeletalMesh")
    for SkeletalMeshAbsPath in SkeletalMeshes:
        abspath, filename = SkeletalMeshAbsPath
        
        GamePath = ConvertAbsPathToGamePath(abspath)
        print("ImportSkeletalMesh:File:[", abspath, "] Game[", GamePath, "]")

        FilePath = abspath + "/" + filename
        ImportData = unreal.AutomatedAssetImportData()
        ImportData.destination_path = GamePath
        ImportData.filenames = [FilePath]
        ImportData.replace_existing = True
        ImportData.factory = unreal.EFModelFactory()
        ImportData.factory.set_editor_property("silent_import", True)
        ImportedAsset = AssetTool.import_assets_automated(ImportData)
        
        unreal.EditorAssetLibrary.save_directory("/Game")
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

bImportAnim = False
if bImportAnim:
    print("ImportAnim")
    for AnimSequenceAbsPath in AnimSequences:
        abspath, filename = AnimSequenceAbsPath
        GamePath = ConvertAbsPathToGamePath(abspath)
        print(GamePath)

        FilePath = abspath + "/" + filename
        ImportData = unreal.AutomatedAssetImportData()
        ImportData.destination_path = GamePath
        ImportData.filenames = [FilePath]
        ImportData.replace_existing = True
        ImportData.factory = unreal.EFAnimFactory()
        ImportData.factory.set_editor_property("silent_import", True)
        ImportedAsset = AssetTool.import_assets_automated(ImportData)

unreal.EditorAssetLibrary.save_directory("/Game")
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
