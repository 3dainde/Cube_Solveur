#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CubeCore.h"
#include "CubeLogicTypes.h"
#include "CubeSolver.generated.h"

/**
 * Solveur Indépendant d'Unreal Engine Graphics.
 * Exécute un parcours en largeur (BFS) sur le graphe de la salle.
 */
UCLASS(BlueprintType, Blueprintable)
class CUBE_API UCubeSolver : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Cube|Solver")
    bool SolveManifest(UPARAM(ref) FCubeManifest& Manifest);

private:
    TArray<ECubeDirection> GetValidTransitions(const FCubeState& CurrentState, const FCubeManifest& Manifest) const;
    int32 GetAdjacentRoomID(int32 CurrentRoomID, ECubeDirection Direction, int32 GridSize) const;
};
