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

    /** Sous-seed retenue (0 = la seed initiale était valide). */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 Attempt = 0;

    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 StartRoomID = -1;

    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 ExitRoomID = -1;

    // Toutes les salles logiques ouvertes de ce niveau (le couloir)
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    TMap<int32, FCubeLogicalRoom> Rooms;

    /**
     * ECubeCellType par RoomID (GridSize^3 octets, 256 Ko en 64^3) : la grille complète, murs compris.
     * Source de vérité du solveur. Non exposé en Blueprint (copie coûteuse).
     */
    UPROPERTY()
    TArray<uint8> Cells;

    // Chemin critique ordonné (Start -> ... -> Exit), en RoomID = chemin du solveur. Sert au rébus (PathIndex, RLE).
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    TArray<int32> CriticalPath;

    /** Chemin optimal du solveur, état par état (inventaire et flags compris). */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    TArray<FCubeState> SolutionPath;

    // Indicateurs du Solveur
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    bool bIsValid = false;

    /** Nombre de pas du chemin optimal (-1 sans solution). */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 SolutionLength = -1;

    /** États uniques découverts par le BFS. */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Manifest")
    int32 NumStates = 0;

    int32 NumRooms() const { return GridSize * GridSize * GridSize; }
    bool HasCells() const { return GridSize > 0 && Cells.Num() == NumRooms(); }
    bool IsOpen(int32 RoomID) const { return Cells[RoomID] != static_cast<uint8>(ECubeCellType::Wall); }
    ECubeCellType GetCellType(int32 RoomID) const { return static_cast<ECubeCellType>(Cells[RoomID]); }
};
