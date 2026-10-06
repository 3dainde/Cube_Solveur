#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CubeCore.h"
#include "CubeLogicTypes.h"
#include "CubeSolver.generated.h"

/**
 * Solveur indépendant du rendu (Manuals/Solveur_Documentation.md §4).
 * BFS sur l'espace d'états S = <Room, Orientation, Inventory, Flags> : une porte n'est franchissable
 * qu'avec le bit de clé correspondant ; une clé l'ajoute à l'inventaire. Le premier état atteint dans
 * la salle de sortie donne le chemin optimal. Parité bit à bit avec la R&D (même chemin, même nombre d'états).
 */
UCLASS(BlueprintType, Blueprintable)
class CUBE_API UCubeSolver : public UObject
{
    GENERATED_BODY()

public:
    /** Résout le manifest et y écrit SolutionPath, CriticalPath, SolutionLength, NumStates, bIsValid. */
    UFUNCTION(BlueprintCallable, Category="Cube|Solver")
    bool SolveManifest(UPARAM(ref) FCubeManifest& Manifest);

    /** Variante sans UObject (générateur, tests, threads de fond). */
    static bool Solve(FCubeManifest& Manifest);

    /** Transitions valides depuis un état, dans l'ordre de ECubeDirection (référence lisible du BFS). */
    UFUNCTION(BlueprintPure, Category="Cube|Solver")
    static TArray<ECubeDirection> GetValidTransitions(const FCubeState& CurrentState, const FCubeManifest& Manifest);

    /** Salle voisine dans une direction, -1 hors grille. */
    UFUNCTION(BlueprintPure, Category="Cube|Solver")
    static int32 GetAdjacentRoomID(int32 CurrentRoomID, ECubeDirection Direction, int32 GridSize);

    /** Reconstruit Cells depuis Rooms (manifest construit à la main, sans grille). */
    static void BuildCellsFromRooms(FCubeManifest& Manifest);
};
