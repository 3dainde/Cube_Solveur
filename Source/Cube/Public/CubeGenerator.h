#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CubeLogicTypes.h"
#include "CubeGenerator.generated.h"

/**
 * Générateur déterministe du Cube (Manuals/Solveur_Documentation.md §5).
 * Sépare intentionnellement la génération de la logique de rendu UE5.
 *
 * Parité bit à bit avec la R&D Python : même seed -> mêmes cellules, même chemin, mêmes clés.
 * Le chemin est creusé sur le réseau des noeuds PAIRS (pas de 2) par un DFS biaisé vers la sortie,
 * tiré avec FCubeRandom (PCG64 + SeedSequence de numpy), puis densifié (cases intermédiaires).
 * Les grilles impaires sont gérées par une jonction de sortie (connecteurs jusqu'à N-1).
 * Une boucle de sous-seeds garantit qu'aucune seed n'est orpheline (solvabilité vérifiée par UCubeSolver).
 */
UCLASS(BlueprintType, Blueprintable)
class CUBE_API UCubeGenerator : public UObject
{
    GENERATED_BODY()

public:
    /** Borne de sécurité de la boucle de sous-seeds quand MaxAttempts <= 0 (« jusqu'au succès »). */
    static constexpr int32 UnboundedAttempts = 100000;

    /** API simple : seed + taille, paramètres par défaut de la R&D. Boucle jusqu'à un niveau solvable. */
    UFUNCTION(BlueprintCallable, Category="Cube|Generator")
    FCubeManifest GenerateCube(FString Seed, int32 GridSize);

    /**
     * API complète : calibrage route/verticalité/clés.
     * MaxAttempts <= 0 : retente jusqu'au premier niveau jouable (borne UnboundedAttempts), comme la R&D.
     */
    UFUNCTION(BlueprintCallable, Category="Cube|Generator")
    FCubeManifest GenerateCubeAdvanced(FString Seed, int32 GridSize,
                                       float PathLen = 0.45f, float Verticality = 0.18f,
                                       int32 NKeys = 2, int32 MaxAttempts = 0);

    /** Vrai si les paramètres peuvent produire un niveau (taille et nombre de clés compatibles). */
    UFUNCTION(BlueprintPure, Category="Cube|Generator")
    static bool AreParamsFeasible(int32 GridSize, int32 NKeys);

    /** Génération + validation sans UObject (tests, outils, threads de fond). Renvoie le manifest résolu. */
    static FCubeManifest Generate(const FString& Seed, int32 GridSize, double PathLen, double Verticality,
                                  int32 NKeys, int32 MaxAttempts = 0);

    /** Une tentative déterministe indexée par sous-seed (seed, attempt). bIsValid = false si à rejeter. */
    static FCubeManifest GenerateManifestAttempt(const FString& Seed, int32 Attempt, int32 N,
                                                 double PathLen, double Verticality, int32 NKeys);

    /** Graine du flux aléatoire d'une tentative : Combine(Combine(CRC32(seed), CRC32("PATH")), attempt). */
    static uint32 GetAttemptRngSeed(const FString& Seed, int32 Attempt);
};
