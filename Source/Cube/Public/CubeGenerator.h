#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CubeLogicTypes.h"
#include "CubeGenerator.generated.h"

/**
 * Générateur Déterministe pour le Cube.
 * Sépare intentionnellement la génération de la logique de rendu UE5.
 *
 * Le chemin est creusé sur le réseau des noeuds PAIRS (pas de 2) par un DFS
 * biaisé vers la sortie, puis densifié (cases intermédiaires). Les grilles impaires sont gérées par une jonction de sortie
 * (connecteurs linéaires jusqu'à N-1). Une boucle d'essais sur sous-seed garantit
 * qu'aucune seed n'est orpheline (solvabilité vérifiée par UCubeSolver).
 */
UCLASS(BlueprintType, Blueprintable)
class CUBE_API UCubeGenerator : public UObject
{
    GENERATED_BODY()

public:
    /** API simple : seed + taille. Boucle jusqu'à obtenir un niveau solvable. */
    UFUNCTION(BlueprintCallable, Category="Cube|Generator")
    FCubeManifest GenerateCube(FString Seed, int32 GridSize);

    /** API complète : calibrage route/verticalité/clés. */
    UFUNCTION(BlueprintCallable, Category="Cube|Generator")
    FCubeManifest GenerateCubeAdvanced(FString Seed, int32 GridSize,
                                       float PathLen = 0.45f, float Verticality = 0.18f,
                                       int32 NKeys = 2, int32 MaxAttempts = 64);

private:
    /** Une tentative déterministe indexée par sous-seed (seed, attempt). */
    FCubeManifest GenerateManifestAttempt(const FString& Seed, int32 Attempt, int32 N,
                                          float PathLen, float Verticality, int32 NKeys);

    /** Creuse le chemin auto-évitant sur le réseau pair. Retourne false si non connexe. */
    bool CarvePath(int32 N, FRandomStream& Stream, float Verticality, float Toward,
                   TArray<FCubeCoordinate>& OutFull, TArray<FCubeCoordinate>& OutInterior,
                   FCubeCoordinate& OutStart, FCubeCoordinate& OutExit) const;
};
