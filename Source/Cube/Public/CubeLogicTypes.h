#pragma once

#include "CoreMinimal.h"
#include "CubeCore.h"
#include "CubeLogicTypes.generated.h"

USTRUCT(BlueprintType)
struct CUBE_API FCubeLogicalRoom
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    int32 RoomID = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    FCubeCoordinate Coordinate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    ERoomArchetype Archetype = ERoomArchetype::Normal;

    // Portes physiques présentes dans cette salle
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    TMap<ECubeDirection, bool> Doors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    bool bIsCriticalPath = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    bool bIsDecoy = false;

    // Bit de clé fourni par cette salle (0 = aucune).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    int32 KeyBit = 0;

    // Bit de clé exigé par la porte de cette salle (0 = aucune).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Room")
    int32 GateBit = 0;

    FCubeLogicalRoom() {}
};

USTRUCT(BlueprintType)
struct CUBE_API FCubeManifest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    FString Seed;

    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 GridSize = 0;

    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 StartRoomID = -1;

    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 ExitRoomID = -1;

    // Toutes les salles logiques de ce niveau
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    TMap<int32, FCubeLogicalRoom> Rooms;

    // Chemin critique ordonné (Start -> ... -> Exit), en RoomID. Sert au rébus (PathIndex, RLE).
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    TArray<int32> CriticalPath;

    // Indicateurs du Solveur
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    bool bIsValid = false;

    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 SolutionLength = -1;
};
