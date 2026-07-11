# Created with python 3.11.4

# This script helps with moving cpp files from Generals or GeneralsMD to Core

import os
import shutil
from enum import Enum


class Game(Enum):
    GENERALS = 0
    ZEROHOUR = 1
    CORE = 2


class CmakeModifyType(Enum):
    ADD_COMMENT = 0
    REMOVE_COMMENT = 1


current_dir = os.path.dirname(os.path.abspath(__file__))
root_dir = os.path.join(current_dir, "..", "..")
root_dir = os.path.normpath(root_dir)
core_dir = os.path.join(root_dir, "Core")
generals_dir = os.path.join(root_dir, "Generals", "Code")
generalsmd_dir = os.path.join(root_dir, "GeneralsMD", "Code")


def get_game_path(game: Game):
    if game == Game.GENERALS:
        return generals_dir
    elif game == Game.ZEROHOUR:
        return generalsmd_dir
    elif game == Game.CORE:
        return core_dir
    assert(0)


def get_opposite_game(game: Game):
    if game == Game.GENERALS:
        return Game.ZEROHOUR
    elif game == Game.ZEROHOUR:
        return Game.GENERALS
    assert(0)


def move_file(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    fromPath = os.path.join(get_game_path(fromGame), os.path.normpath(fromFile))
    toPath = os.path.join(get_game_path(toGame), os.path.normpath(toFile))
    os.makedirs(os.path.dirname(toPath), exist_ok=True)
    shutil.move(fromPath, toPath)


def delete_file(game: Game, path: str):
    os.remove(os.path.join(get_game_path(game), os.path.normpath(path)))


def modify_cmakelists(cmakeFile: str, searchString: str, type: CmakeModifyType):
    lines: list[str]
    with open(cmakeFile, 'r', encoding="ascii") as file:
        lines = file.readlines()

    with open(cmakeFile, 'w', encoding="ascii") as file:
        for index, line  in enumerate(lines):
            if searchString in line:
                if type == CmakeModifyType.ADD_COMMENT:
                    lines[index] = "#" + line
                else:
                    lines[index] = line.replace("#", "", 1)

        file.writelines(lines)


def unify_file(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    assert(toGame == Game.CORE)

    fromOppositeGame = get_opposite_game(fromGame)
    fromOppositeGamePath = get_game_path(fromOppositeGame)
    fromGamePath = get_game_path(fromGame)
    toGamePath = get_game_path(toGame)

    fromFirstFolderIndex = fromFile.find("/")
    toFirstFolderIndex = toFile.find("/")
    assert(fromFirstFolderIndex > 0)
    assert(toFirstFolderIndex > 0)

    fromFirstFolderName = fromFile[:fromFirstFolderIndex]
    toFirstFolderName = toFile[:toFirstFolderIndex]
    fromFileInCmake = fromFile[fromFirstFolderIndex+1:]
    toFileInCmake = toFile[toFirstFolderIndex+1:]

    fromOppositeCmakeFile = os.path.join(fromOppositeGamePath, fromFirstFolderName, "CMakeLists.txt")
    fromCmakeFile = os.path.join(fromGamePath, fromFirstFolderName, "CMakeLists.txt")
    toCmakeFile = os.path.join(toGamePath, toFirstFolderName, "CMakeLists.txt")

    modify_cmakelists(fromOppositeCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(fromCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(toCmakeFile, toFileInCmake, CmakeModifyType.REMOVE_COMMENT)

    delete_file(fromOppositeGame, fromFile)
    move_file(fromGame, fromFile, toGame, toFile)


def unify_move_file(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    assert(toGame == Game.CORE)

    fromGamePath = get_game_path(fromGame)
    toGamePath = get_game_path(toGame)

    fromFirstFolderIndex = fromFile.find("/")
    toFirstFolderIndex = toFile.find("/")
    assert(fromFirstFolderIndex > 0)
    assert(toFirstFolderIndex > 0)

    fromFirstFolderName = fromFile[:fromFirstFolderIndex]
    toFirstFolderName = toFile[:toFirstFolderIndex]
    fromFileInCmake = fromFile[fromFirstFolderIndex+1:]
    toFileInCmake = toFile[toFirstFolderIndex+1:]

    fromCmakeFile = os.path.join(fromGamePath, fromFirstFolderName, "CMakeLists.txt")
    toCmakeFile = os.path.join(toGamePath, toFirstFolderName, "CMakeLists.txt")

    modify_cmakelists(fromCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(toCmakeFile, toFileInCmake, CmakeModifyType.REMOVE_COMMENT)

    move_file(fromGame, fromFile, toGame, toFile)


def unify_file_lib(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    assert(toGame == Game.CORE)

    fromOppositeGame = get_opposite_game(fromGame)
    fromOppositeGamePath = get_game_path(fromOppositeGame)
    fromGamePath = get_game_path(fromGame)
    toGamePath = get_game_path(toGame)

    fromFirstFolderIndex = fromFile.rfind("/")
    toFirstFolderIndex = toFile.rfind("/")
    assert(fromFirstFolderIndex > 0)
    assert(toFirstFolderIndex > 0)

    fromFirstFolderName = fromFile[:fromFirstFolderIndex]
    toFirstFolderName = toFile[:toFirstFolderIndex]
    fromFileInCmake = fromFile[fromFirstFolderIndex+1:]
    toFileInCmake = toFile[toFirstFolderIndex+1:]

    fromOppositeCmakeFile = os.path.join(fromOppositeGamePath, fromFirstFolderName, "CMakeLists.txt")
    fromCmakeFile = os.path.join(fromGamePath, fromFirstFolderName, "CMakeLists.txt")
    toCmakeFile = os.path.join(toGamePath, toFirstFolderName, "CMakeLists.txt")

    modify_cmakelists(fromOppositeCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(fromCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(toCmakeFile, toFileInCmake, CmakeModifyType.REMOVE_COMMENT)

    delete_file(fromOppositeGame, fromFile)
    move_file(fromGame, fromFile, toGame, toFile)


def unify_move_file_lib(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    assert(toGame == Game.CORE)

    fromGamePath = get_game_path(fromGame)
    toGamePath = get_game_path(toGame)

    fromFirstFolderIndex = fromFile.rfind("/")
    toFirstFolderIndex = toFile.rfind("/")
    assert(fromFirstFolderIndex > 0)
    assert(toFirstFolderIndex > 0)

    fromFirstFolderName = fromFile[:fromFirstFolderIndex]
    toFirstFolderName = toFile[:toFirstFolderIndex]
    fromFileInCmake = fromFile[fromFirstFolderIndex+1:]
    toFileInCmake = toFile[toFirstFolderIndex+1:]

    fromCmakeFile = os.path.join(fromGamePath, fromFirstFolderName, "CMakeLists.txt")
    toCmakeFile = os.path.join(toGamePath, toFirstFolderName, "CMakeLists.txt")

    modify_cmakelists(fromCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(toCmakeFile, toFileInCmake, CmakeModifyType.REMOVE_COMMENT)

    move_file(fromGame, fromFile, toGame, toFile)


def main():

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/crc.h", Game.CORE, "GameEngine/Include/Common/crc.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/CRCDebug.h", Game.CORE, "GameEngine/Include/Common/CRCDebug.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/crc.cpp", Game.CORE, "GameEngine/Source/Common/crc.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/CRCDebug.cpp", Game.CORE, "GameEngine/Source/Common/CRCDebug.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/RandomValue.h", Game.CORE, "GameEngine/Include/Common/RandomValue.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ClientRandomValue.h", Game.CORE, "GameEngine/Include/GameClient/ClientRandomValue.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameLogic/LogicRandomValue.h", Game.CORE, "GameEngine/Include/GameLogic/LogicRandomValue.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/RandomValue.cpp", Game.CORE, "GameEngine/Source/Common/RandomValue.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/Debug.h", Game.CORE, "GameEngine/Include/Common/Debug.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/Debug.cpp", Game.CORE, "GameEngine/Source/Common/System/Debug.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/VideoPlayer.h", Game.CORE, "GameEngine/Include/GameClient/VideoPlayer.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/VideoPlayer.cpp", Game.CORE, "GameEngine/Source/GameClient/VideoPlayer.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/VideoStream.cpp", Game.CORE, "GameEngine/Source/GameClient/VideoStream.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/WindowVideoManager.h", Game.CORE, "GameEngine/Include/GameClient/WindowVideoManager.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/WindowVideoManager.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/WindowVideoManager.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/INI/INIVideo.cpp", Game.CORE, "GameEngine/Source/Common/INI/INIVideo.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/VideoDevice/Bink/BinkVideoPlayer.h", Game.CORE, "GameEngineDevice/Include/VideoDevice/Bink/BinkVideoPlayer.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/VideoDevice/Bink/BinkVideoPlayer.cpp", Game.CORE, "GameEngineDevice/Source/VideoDevice/Bink/BinkVideoPlayer.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DVideoBuffer.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DVideoBuffer.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegFile.h", Game.CORE, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegFile.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegVideoPlayer.h", Game.CORE, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegVideoPlayer.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp", Game.CORE, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegVideoPlayer.cpp", Game.CORE, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegVideoPlayer.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/GameMemory.h", Game.CORE, "GameEngine/Include/Common/GameMemory.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/GameMemoryNull.h", Game.CORE, "GameEngine/Include/Common/GameMemoryNull.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemory.cpp", Game.CORE, "GameEngine/Source/Common/System/GameMemory.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemoryNull.cpp", Game.CORE, "GameEngine/Source/Common/System/GameMemoryNull.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/MemoryInit.cpp", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInit.cpp")
    #unify_move_file(Game.GENERALS, "GameEngine/Source/Common/System/GameMemoryInitDMA_Generals.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitDMA_Generals.inl")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemoryInitDMA_GeneralsMD.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitDMA_GeneralsMD.inl")
    #unify_move_file(Game.GENERALS, "GameEngine/Source/Common/System/GameMemoryInitPools_Generals.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitPools_Generals.inl")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemoryInitPools_GeneralsMD.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitPools_GeneralsMD.inl")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/ObjectStatusTypes.h", Game.CORE, "GameEngine/Include/Common/ObjectStatusTypes.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/Radar.h", Game.CORE, "GameEngine/Include/Common/Radar.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/ObjectStatusTypes.cpp", Game.CORE, "GameEngine/Source/Common/System/ObjectStatusTypes.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/Radar.cpp", Game.CORE, "GameEngine/Source/Common/System/Radar.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/Common/W3DRadar.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/Common/W3DRadar.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp")

    #unify_move_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Smudge.h", Game.CORE, "GameEngine/Include/GameClient/Smudge.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/System/Smudge.cpp", Game.CORE, "GameEngine/Source/GameClient/System/Smudge.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DSmudge.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DSmudge.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp")

    #unify_move_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ParabolicEase.h", Game.CORE, "GameEngine/Include/GameClient/ParabolicEase.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/ParabolicEase.cpp", Game.CORE, "GameEngine/Source/GameClient/ParabolicEase.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/camerashakesystem.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/CameraShakeSystem.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/camerashakesystem.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/CameraShakeSystem.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/View.h", Game.CORE, "GameEngine/Include/GameClient/View.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/View.cpp", Game.CORE, "GameEngine/Source/GameClient/View.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DView.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DView.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")

    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/bmp2d.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/bmp2d.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/bmp2d.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/bmp2d.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8texman.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8texman.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8texman.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8texman.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/matpass.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/matpass.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/matpass.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/matpass.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texproject.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texproject.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texproject.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texproject.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texture.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texture.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texture.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texture.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texturefilter.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texturefilter.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texturefilter.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texturefilter.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/textureloader.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/textureloader.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/textureloader.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/textureloader.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texturethumbnail.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texturethumbnail.cpp")
    #unify_file(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/texturethumbnail.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/texturethumbnail.h")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Water.h", Game.CORE, "GameEngine/Include/GameClient/Water.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Water.cpp", Game.CORE, "GameEngine/Source/GameClient/Water.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DLaserDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DLaserDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DWater.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DWater.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DWaterTracks.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DWaterTracks.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DLaserDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DLaserDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWaterTracks.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWaterTracks.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Water/wave.nvp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Water/wave.nvp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Water/wave.nvv", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Water/wave.nvv")

    #unify_move_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Snow.h", Game.CORE, "GameEngine/Include/GameClient/Snow.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Snow.cpp", Game.CORE, "GameEngine/Source/GameClient/Snow.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/FlatHeightMap.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/FlatHeightMap.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DPropBuffer.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DPropBuffer.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DSnow.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DSnow.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTerrainBackground.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTerrainBackground.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DPropDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DPropDraw.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTreeDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTreeDraw.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/FlatHeightMap.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/FlatHeightMap.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DPropBuffer.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DPropBuffer.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DSnow.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DSnow.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainBackground.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainBackground.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DPropDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DPropDraw.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/MapObject.h", Game.CORE, "GameEngine/Include/Common/MapObject.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/MapUtil.h", Game.CORE, "GameEngine/Include/GameClient/MapUtil.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/TerrainRoads.h", Game.CORE, "GameEngine/Include/GameClient/TerrainRoads.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/TerrainVisual.h", Game.CORE, "GameEngine/Include/GameClient/TerrainVisual.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MapUtil.cpp", Game.CORE, "GameEngine/Source/GameClient/MapUtil.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Terrain/TerrainRoads.cpp", Game.CORE, "GameEngine/Source/GameClient/Terrain/TerrainRoads.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Terrain/TerrainVisual.cpp", Game.CORE, "GameEngine/Source/GameClient/Terrain/TerrainVisual.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/HeightMap.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/HeightMap.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/TerrainTex.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/TerrainTex.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/TileData.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/TileData.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTerrainTracks.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTerrainTracks.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTerrainVisual.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTerrainVisual.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTreeBuffer.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DTreeBuffer.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/WorldHeightMap.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/WorldHeightMap.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/TerrainTex.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/TerrainTex.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/TileData.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/TileData.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainTracks.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainTracks.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/UserPreferences.h", Game.CORE, "GameEngine/Include/Common/UserPreferences.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/UserPreferences.cpp", Game.CORE, "GameEngine/Source/Common/UserPreferences.cpp")

    #unify_file(Game.ZEROHOUR, "Libraries/Include/Lib/BaseType.h", Game.CORE, "Libraries/Include/Lib/BaseType.h")
    #unify_file(Game.ZEROHOUR, "Libraries/Include/Lib/trig.h", Game.CORE, "Libraries/Include/Lib/trig.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/Errors.h", Game.CORE, "GameEngine/Include/Common/Errors.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/GameCommon.h", Game.CORE, "GameEngine/Include/Common/GameCommon.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/GameType.h", Game.CORE, "GameEngine/Include/Common/GameType.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/INI.h", Game.CORE, "GameEngine/Include/Common/INI.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/Snapshot.h", Game.CORE, "GameEngine/Include/Common/Snapshot.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/STLTypedefs.h", Game.CORE, "GameEngine/Include/Common/STLTypedefs.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/SubsystemInterface.h", Game.CORE, "GameEngine/Include/Common/SubsystemInterface.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ChallengeGenerals.h", Game.CORE, "GameEngine/Include/GameClient/ChallengeGenerals.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/INI/INI.cpp", Game.CORE, "GameEngine/Source/Common/INI/INI.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameCommon.cpp", Game.CORE, "GameEngine/Source/Common/System/GameCommon.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameType.cpp", Game.CORE, "GameEngine/Source/Common/System/GameType.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/Snapshot.cpp", Game.CORE, "GameEngine/Source/Common/System/Snapshot.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/SubsystemInterface.cpp", Game.CORE, "GameEngine/Source/Common/System/SubsystemInterface.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ChallengeGenerals.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ChallengeGenerals.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ParticleSys.h", Game.CORE, "GameEngine/Include/GameClient/ParticleSys.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/System/ParticleSys.cpp", Game.CORE, "GameEngine/Source/GameClient/System/ParticleSys.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/res/ParticleEditor.rc2", Game.CORE, "Tools/ParticleEditor/res/ParticleEditor.rc2")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/CButtonShowColor.cpp", Game.CORE, "Tools/ParticleEditor/CButtonShowColor.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/CButtonShowColor.h", Game.CORE, "Tools/ParticleEditor/CButtonShowColor.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/CColorAlphaDialog.cpp", Game.CORE, "Tools/ParticleEditor/CColorAlphaDialog.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/CColorAlphaDialog.h", Game.CORE, "Tools/ParticleEditor/CColorAlphaDialog.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/CParticleEditorPage.h", Game.CORE, "Tools/ParticleEditor/CParticleEditorPage.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/CSwitchesDialog.cpp", Game.CORE, "Tools/ParticleEditor/CSwitchesDialog.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/CSwitchesDialog.h", Game.CORE, "Tools/ParticleEditor/CSwitchesDialog.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/EmissionTypePanels.cpp", Game.CORE, "Tools/ParticleEditor/EmissionTypePanels.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/EmissionTypePanels.h", Game.CORE, "Tools/ParticleEditor/EmissionTypePanels.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ISwapablePanel.h", Game.CORE, "Tools/ParticleEditor/ISwapablePanel.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/MoreParmsDialog.cpp", Game.CORE, "Tools/ParticleEditor/MoreParmsDialog.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/MoreParmsDialog.h", Game.CORE, "Tools/ParticleEditor/MoreParmsDialog.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleEditor.cpp", Game.CORE, "Tools/ParticleEditor/ParticleEditor.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleEditor.def", Game.CORE, "Tools/ParticleEditor/ParticleEditor.def")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleEditor.h", Game.CORE, "Tools/ParticleEditor/ParticleEditor.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleEditor.rc", Game.CORE, "Tools/ParticleEditor/ParticleEditor.rc")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleEditorDialog.cpp", Game.CORE, "Tools/ParticleEditor/ParticleEditorDialog.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleEditorDialog.h", Game.CORE, "Tools/ParticleEditor/ParticleEditorDialog.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleEditorExport.h", Game.CORE, "Tools/ParticleEditor/ParticleEditorExport.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleTypePanels.cpp", Game.CORE, "Tools/ParticleEditor/ParticleTypePanels.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ParticleTypePanels.h", Game.CORE, "Tools/ParticleEditor/ParticleTypePanels.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/post-build-Release.bat", Game.CORE, "Tools/ParticleEditor/post-build-Release.bat")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/post-build.bat", Game.CORE, "Tools/ParticleEditor/post-build.bat")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/Resource.h", Game.CORE, "Tools/ParticleEditor/Resource.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ShaderTypePanels.cpp", Game.CORE, "Tools/ParticleEditor/ShaderTypePanels.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/ShaderTypePanels.h", Game.CORE, "Tools/ParticleEditor/ShaderTypePanels.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/StdAfx.cpp", Game.CORE, "Tools/ParticleEditor/StdAfx.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/StdAfx.h", Game.CORE, "Tools/ParticleEditor/StdAfx.h")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/VelocityTypePanels.cpp", Game.CORE, "Tools/ParticleEditor/VelocityTypePanels.cpp")
    #unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/VelocityTypePanels.h", Game.CORE, "Tools/ParticleEditor/VelocityTypePanels.h")

    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DDebrisDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DDebrisDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DDefaultDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DDefaultDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DDependencyModelDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DDependencyModelDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DOverlordTankDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DOverlordTankDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DPoliceCarDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DPoliceCarDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DProjectileStreamDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DProjectileStreamDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DRopeDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DRopeDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DScienceModelDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DScienceModelDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DSupplyDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DSupplyDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTankDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTankDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTankTruckDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTankTruckDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTracerDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTracerDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTruckDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DTruckDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDefaultDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDefaultDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDependencyModelDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDependencyModelDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTankDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTankDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DPoliceCarDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DPoliceCarDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DRopeDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DRopeDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DScienceModelDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DScienceModelDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DSupplyDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DSupplyDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTankDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTankDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTankTruckDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTankTruckDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTracerDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTracerDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTruckDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTruckDraw.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DOverlordAircraftDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DOverlordAircraftDraw.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DOverlordTruckDraw.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DOverlordTruckDraw.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordAircraftDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordAircraftDraw.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTruckDraw.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTruckDraw.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Mouse.h", Game.CORE, "GameEngine/Include/GameClient/Mouse.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Input/Mouse.cpp", Game.CORE, "GameEngine/Source/GameClient/Input/Mouse.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Keyboard.h", Game.CORE, "GameEngine/Include/GameClient/Keyboard.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Input/Keyboard.cpp", Game.CORE, "GameEngine/Source/GameClient/Input/Keyboard.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DMouse.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DMouse.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DMouse.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DMouse.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/Win32Device/GameClient/Win32DIKeyboard.h", Game.CORE, "GameEngineDevice/Include/Win32Device/GameClient/Win32DIKeyboard.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/Win32Device/GameClient/Win32DIMouse.h", Game.CORE, "GameEngineDevice/Include/Win32Device/GameClient/Win32DIMouse.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/Win32Device/GameClient/Win32Mouse.h", Game.CORE, "GameEngineDevice/Include/Win32Device/GameClient/Win32Mouse.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/Win32Device/GameClient/Win32DIKeyboard.cpp", Game.CORE, "GameEngineDevice/Source/Win32Device/GameClient/Win32DIKeyboard.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/Win32Device/GameClient/Win32DIMouse.cpp", Game.CORE, "GameEngineDevice/Source/Win32Device/GameClient/Win32DIMouse.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/Win32Device/GameClient/Win32Mouse.cpp", Game.CORE, "GameEngineDevice/Source/Win32Device/GameClient/Win32Mouse.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GameFont.h", Game.CORE, "GameEngine/Include/GameClient/GameFont.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GameWindow.h", Game.CORE, "GameEngine/Include/GameClient/GameWindow.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GameWindowGlobal.h", Game.CORE, "GameEngine/Include/GameClient/GameWindowGlobal.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GameWindowTransitions.h", Game.CORE, "GameEngine/Include/GameClient/GameWindowTransitions.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/HeaderTemplate.h", Game.CORE, "GameEngine/Include/GameClient/HeaderTemplate.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/IMEManager.h", Game.CORE, "GameEngine/Include/GameClient/IMEManager.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/LoadScreen.h", Game.CORE, "GameEngine/Include/GameClient/LoadScreen.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ProcessAnimateWindow.h", Game.CORE, "GameEngine/Include/GameClient/ProcessAnimateWindow.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/WindowLayout.h", Game.CORE, "GameEngine/Include/GameClient/WindowLayout.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/WinInstanceData.h", Game.CORE, "GameEngine/Include/GameClient/WinInstanceData.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/GameFont.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/GameFont.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/GameWindow.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/GameWindow.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/GameWindowGlobal.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/GameWindowGlobal.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/GameWindowTransitions.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/GameWindowTransitions.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/HeaderTemplate.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/HeaderTemplate.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/IMEManager.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/IMEManager.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/LoadScreen.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/LoadScreen.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ProcessAnimateWindow.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ProcessAnimateWindow.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/WindowLayout.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/WindowLayout.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/WinInstanceData.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/WinInstanceData.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ClientInstance.h", Game.CORE, "GameEngine/Include/GameClient/ClientInstance.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Color.h", Game.CORE, "GameEngine/Include/GameClient/Color.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Credits.h", Game.CORE, "GameEngine/Include/GameClient/Credits.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/DisplayString.h", Game.CORE, "GameEngine/Include/GameClient/DisplayString.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/DisplayStringManager.h", Game.CORE, "GameEngine/Include/GameClient/DisplayStringManager.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/DrawGroupInfo.h", Game.CORE, "GameEngine/Include/GameClient/DrawGroupInfo.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/FXList.h", Game.CORE, "GameEngine/Include/GameClient/FXList.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GameText.h", Game.CORE, "GameEngine/Include/GameClient/GameText.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GlobalLanguage.h", Game.CORE, "GameEngine/Include/GameClient/GlobalLanguage.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GraphDraw.h", Game.CORE, "GameEngine/Include/GameClient/GraphDraw.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/LanguageFilter.h", Game.CORE, "GameEngine/Include/GameClient/LanguageFilter.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Line2D.h", Game.CORE, "GameEngine/Include/GameClient/Line2D.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/RadiusDecal.h", Game.CORE, "GameEngine/Include/GameClient/RadiusDecal.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/SelectionInfo.h", Game.CORE, "GameEngine/Include/GameClient/SelectionInfo.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Statistics.h", Game.CORE, "GameEngine/Include/GameClient/Statistics.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/ClientInstance.cpp", Game.CORE, "GameEngine/Source/GameClient/ClientInstance.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Color.cpp", Game.CORE, "GameEngine/Source/GameClient/Color.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Credits.cpp", Game.CORE, "GameEngine/Source/GameClient/Credits.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/DisplayString.cpp", Game.CORE, "GameEngine/Source/GameClient/DisplayString.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/DisplayStringManager.cpp", Game.CORE, "GameEngine/Source/GameClient/DisplayStringManager.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/DrawGroupInfo.cpp", Game.CORE, "GameEngine/Source/GameClient/DrawGroupInfo.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/FXList.cpp", Game.CORE, "GameEngine/Source/GameClient/FXList.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GameText.cpp", Game.CORE, "GameEngine/Source/GameClient/GameText.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GlobalLanguage.cpp", Game.CORE, "GameEngine/Source/GameClient/GlobalLanguage.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GraphDraw.cpp", Game.CORE, "GameEngine/Source/GameClient/GraphDraw.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/LanguageFilter.cpp", Game.CORE, "GameEngine/Source/GameClient/LanguageFilter.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Line2D.cpp", Game.CORE, "GameEngine/Source/GameClient/Line2D.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/RadiusDecal.cpp", Game.CORE, "GameEngine/Source/GameClient/RadiusDecal.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/SelectionInfo.cpp", Game.CORE, "GameEngine/Source/GameClient/SelectionInfo.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Statistics.cpp", Game.CORE, "GameEngine/Source/GameClient/Statistics.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameLogic/AI/AIPathfind.cpp", Game.CORE, "GameEngine/Source/GameLogic/AI/AIPathfind.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameLogic/AIPathfind.h", Game.CORE, "GameEngine/Include/GameLogic/AIPathfind.h")

    #unify_move_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8rendererdebugger.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8rendererdebugger.h")
    #unify_move_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8rendererdebugger.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8rendererdebugger.cpp")
    #unify_move_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/shdlib.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/shdlib.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8caps.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8caps.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8wrapper.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8wrapper.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8caps.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8caps.cpp")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Display.cpp", Game.CORE, "GameEngine/Source/GameClient/Display.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Display.h", Game.CORE, "GameEngine/Include/GameClient/Display.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/System/Anim2D.cpp", Game.CORE, "GameEngine/Source/GameClient/System/Anim2D.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Anim2D.h", Game.CORE, "GameEngine/Include/GameClient/Anim2D.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/System/Image.cpp", Game.CORE, "GameEngine/Source/GameClient/System/Image.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Image.h", Game.CORE, "GameEngine/Include/GameClient/Image.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/System/DebugDisplay.cpp", Game.CORE, "GameEngine/Source/GameClient/System/DebugDisplay.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/DebugDisplay.h", Game.CORE, "GameEngine/Include/GameClient/DebugDisplay.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/System/RayEffect.cpp", Game.CORE, "GameEngine/Source/GameClient/System/RayEffect.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/RayEffect.h", Game.CORE, "GameEngine/Include/GameClient/RayEffect.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Drawable/Update/AnimatedParticleSysBoneClientUpdate.cpp", Game.CORE, "GameEngine/Source/GameClient/Drawable/Update/AnimatedParticleSysBoneClientUpdate.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Module/AnimatedParticleSysBoneClientUpdate.h", Game.CORE, "GameEngine/Include/GameClient/Module/AnimatedParticleSysBoneClientUpdate.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Drawable/Update/BeaconClientUpdate.cpp", Game.CORE, "GameEngine/Source/GameClient/Drawable/Update/BeaconClientUpdate.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Module/BeaconClientUpdate.h", Game.CORE, "GameEngine/Include/GameClient/Module/BeaconClientUpdate.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/Drawable/Update/SwayClientUpdate.cpp", Game.CORE, "GameEngine/Source/GameClient/Drawable/Update/SwayClientUpdate.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Module/SwayClientUpdate.h", Game.CORE, "GameEngine/Include/GameClient/Module/SwayClientUpdate.h")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Gadget.h", Game.CORE, "GameEngine/Include/GameClient/Gadget.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetCheckBox.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetCheckBox.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetCheckBox.h", Game.CORE, "GameEngine/Include/GameClient/GadgetCheckBox.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetComboBox.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetComboBox.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetComboBox.h", Game.CORE, "GameEngine/Include/GameClient/GadgetComboBox.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetListBox.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetListBox.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetListBox.h", Game.CORE, "GameEngine/Include/GameClient/GadgetListBox.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetProgressBar.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetProgressBar.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetProgressBar.h", Game.CORE, "GameEngine/Include/GameClient/GadgetProgressBar.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetPushButton.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetPushButton.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetPushButton.h", Game.CORE, "GameEngine/Include/GameClient/GadgetPushButton.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetRadioButton.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetRadioButton.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetRadioButton.h", Game.CORE, "GameEngine/Include/GameClient/GadgetRadioButton.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetStaticText.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetStaticText.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetStaticText.h", Game.CORE, "GameEngine/Include/GameClient/GadgetStaticText.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetTabControl.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetTabControl.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetTabControl.h", Game.CORE, "GameEngine/Include/GameClient/GadgetTabControl.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetTextEntry.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetTextEntry.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetTextEntry.h", Game.CORE, "GameEngine/Include/GameClient/GadgetTextEntry.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetHorizontalSlider.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetHorizontalSlider.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/Gadget/GadgetVerticalSlider.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/Gadget/GadgetVerticalSlider.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GadgetSlider.h", Game.CORE, "GameEngine/Include/GameClient/GadgetSlider.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DGadget.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DGadget.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DCheckBox.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DCheckBox.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DComboBox.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DComboBox.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DHorizontalSlider.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DHorizontalSlider.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DListBox.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DListBox.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DProgressBar.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DProgressBar.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DPushButton.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DPushButton.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DRadioButton.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DRadioButton.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DStaticText.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DStaticText.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DTabControl.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DTabControl.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DTextEntry.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DTextEntry.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DVerticalSlider.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DVerticalSlider.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameLogic/CaveSystem.h", Game.CORE, "GameEngine/Include/GameLogic/CaveSystem.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameLogic/CrateSystem.h", Game.CORE, "GameEngine/Include/GameLogic/CrateSystem.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameLogic/Damage.h", Game.CORE, "GameEngine/Include/GameLogic/Damage.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameLogic/RankInfo.h", Game.CORE, "GameEngine/Include/GameLogic/RankInfo.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameLogic/System/CaveSystem.cpp", Game.CORE, "GameEngine/Source/GameLogic/System/CaveSystem.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameLogic/System/CrateSystem.cpp", Game.CORE, "GameEngine/Source/GameLogic/System/CrateSystem.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameLogic/System/Damage.cpp", Game.CORE, "GameEngine/Source/GameLogic/System/Damage.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp", Game.CORE, "GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameLogic/System/RankInfo.cpp", Game.CORE, "GameEngine/Source/GameLogic/System/RankInfo.cpp")

    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8fvf.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8fvf.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8renderer.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8renderer.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8fvf.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8fvf.cpp")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.cpp")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8renderer.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8renderer.cpp")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.cpp")

    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/sortingrenderer.h", Game.CORE, "Libraries/Source/WWVegas/WW3D2/sortingrenderer.h")
    #unify_file_lib(Game.ZEROHOUR, "Libraries/Source/WWVegas/WW3D2/sortingrenderer.cpp", Game.CORE, "Libraries/Source/WWVegas/WW3D2/sortingrenderer.cpp")

    #unify_move_file(Game.ZEROHOUR, "GameEngine/Include/Common/AcademyStats.h", Game.CORE, "GameEngine/Include/Common/AcademyStats.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/Common/RTS/AcademyStats.cpp", Game.CORE, "GameEngine/Source/Common/RTS/AcademyStats.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/CommandXlat.h", Game.CORE, "GameEngine/Include/GameClient/CommandXlat.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/GUICommandTranslator.h", Game.CORE, "GameEngine/Include/GameClient/GUICommandTranslator.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/HintSpy.h", Game.CORE, "GameEngine/Include/GameClient/HintSpy.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/HotKey.h", Game.CORE, "GameEngine/Include/GameClient/HotKey.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/LookAtXlat.h", Game.CORE, "GameEngine/Include/GameClient/LookAtXlat.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/MetaEvent.h", Game.CORE, "GameEngine/Include/GameClient/MetaEvent.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/PlaceEventTranslator.h", Game.CORE, "GameEngine/Include/GameClient/PlaceEventTranslator.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/SelectionXlat.h", Game.CORE, "GameEngine/Include/GameClient/SelectionXlat.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/WindowXlat.h", Game.CORE, "GameEngine/Include/GameClient/WindowXlat.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/GUICommandTranslator.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/GUICommandTranslator.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/HintSpy.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/HintSpy.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/HotKey.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/HotKey.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/MetaEvent.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/MetaEvent.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/PlaceEventTranslator.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/PlaceEventTranslator.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/SelectionXlat.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/SelectionXlat.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/MessageStream/WindowXlat.cpp", Game.CORE, "GameEngine/Source/GameClient/MessageStream/WindowXlat.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GameClientDispatch.cpp", Game.CORE, "GameEngine/Source/GameClient/GameClientDispatch.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ControlBar.h", Game.CORE, "GameEngine/Include/GameClient/ControlBar.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ControlBarResizer.h", Game.CORE, "GameEngine/Include/GameClient/ControlBarResizer.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ControlBarScheme.h", Game.CORE, "GameEngine/Include/GameClient/ControlBarScheme.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarBeacon.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarBeacon.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarCommand.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarCommand.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarCommandProcessing.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarCommandProcessing.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarMultiSelect.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarMultiSelect.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarOCLTimer.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarOCLTimer.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarObserver.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarObserver.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarPrintPositions.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarPrintPositions.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarResizer.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarResizer.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarStructureInventory.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarStructureInventory.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarUnderConstruction.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/ControlBar/ControlBarUnderConstruction.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/GUICallbacks/ControlBarCallback.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/GUICallbacks/ControlBarCallback.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/GUICallbacks/ControlBarPopupDescription.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/GUICallbacks/ControlBarPopupDescription.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DControlBar.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DControlBar.cpp")

    return


if __name__ == "__main__":
    main()
