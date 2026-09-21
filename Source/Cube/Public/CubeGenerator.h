#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CubeLogicTypes.h"
#include "CubeGenerator.generated.h"

/**
 * Générateur Déterministe pour le Cube.
 * Sépare intentionnellement la génération de la logique UE5.
 */
UCLASS(BlueprintType, Blueprintable)
class CUBE_API UCubeGenerator : public UObject
{
    GENERATED_BODY()

public:
    // Paramètre : Seed String et Taille. Retourne un Manifeste complet.
    UFUNCTION(BlueprintCallable, Category="Cube|Generator")
    FCubeManifest GenerateCube(FString Seed, int32 GridSize);

private:
    void GenerateCriticalPath(FCubeManifest& Manifest, FRandomStream& Stream);
};
