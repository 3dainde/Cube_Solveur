#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubeRebusTypes.h"
#include "CubeLogicTypes.h"
#include "CubeRebusLibrary.generated.h"

/**
 * Coeur algorithmique pur (Engine-agnostic) du système Rébus.
 *  1. Champ mortel déterministe (IsLethal).
 *  2. Détection stricte des raccourcis viables (ComputeSafeShortcuts / PathFinder_Annexes).
 *  3. Générateur/solveur de formules modulaires (MakeFormula, EvaluateFormula).
 *
 * Autonome : aucune dépendance à un outil externe.
 */
UCLASS()
class CUBE_API UCubeRebusLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Compression de route en tokens RLE (direction + longueur). */
    UFUNCTION(BlueprintCallable, Category="Cube|Rebus")
    static TArray<FCubeRouteToken> RouteToRLE(const TArray<int32>& PathRoomIDs, int32 GridSize);

    /** Évalue si une cellule est un piège mortel selon Seed + Rho. */
    static bool IsLethal(const FString& Seed, int32 X, int32 Y, int32 Z,
                         float Rho, const TSet<int32>& SafeSet, int32 GridSize);

    UFUNCTION(BlueprintCallable, Category="Cube|Rebus")
    static bool IsLethalCell(const FString& Seed, FCubeCoordinate Cell,
                             float Rho, int32 GridSize, const TSet<int32>& SafeSet);

    /**
     * PathFinder_Annexes : identifie STRICTEMENT les cellules formant un vrai raccourci.
     * Une cellule non-mortelle n'est un SafeShortcut que si elle appartient à une composante
     * reliant au moins 2 points distincts et non-adjacents (>= 2 pas) du chemin critique.
     */
    UFUNCTION(BlueprintCallable, Category="Cube|Rebus")
    static TSet<int32> ComputeSafeShortcuts(const FString& Seed, const TArray<int32>& PathRoomIDs,
                                            int32 GridSize, float Rho);

    /** Calibre la formule modulaire pour que le résultat pointe sur la direction sûre. */
    static FCubeFormula MakeFormula(const FString& Seed, int32 PathIndex,
                                    FCubeCoordinate Pos, int32 SafeDirIndex,
                                    int32 Tier, bool bEnableDecoy);

    UFUNCTION(BlueprintCallable, Category="Cube|Rebus")
    static int32 EvaluateFormula(const FCubeFormula& F, FCubeCoordinate Pos);

    UFUNCTION(BlueprintCallable, Category="Cube|Rebus")
    static ECubeDirection IndexToDirection(int32 Index);

    static int32 DirectionToIndex(ECubeDirection Dir);

    // ---- Chemin unique strict ----

    /**
     * Cellules non mortelles hors chemin dont la composante connexe touche deux points du chemin
     * distants d'au moins 2 pas : elles formeraient un raccourci et doivent être traitées comme mortelles.
     * Les composantes restantes ne touchent le chemin qu'en un point (ou deux consécutifs) :
     * ce sont des culs-de-sac, les pièces safe hors chemin.
     */
    static TSet<int32> ComputeBridgingCells(const FString& Seed, const TArray<int32>& PathRoomIDs,
                                            int32 GridSize, float Rho);

    // ---- Indices de progression (codage 6 bits, voir FCubeClue) ----

    UFUNCTION(BlueprintPure, Category="Cube|Clue")
    static int32 EncodeRouteToken(const FCubeRouteToken& Token);

    UFUNCTION(BlueprintPure, Category="Cube|Clue")
    static FCubeRouteToken DecodeRouteToken(int32 Code);

    /** "X+3", "Z-2"... */
    UFUNCTION(BlueprintPure, Category="Cube|Clue")
    static FString RouteTokenToString(const FCubeRouteToken& Token);

    /**
     * Indice affiché en PathRoomIDs[PathIndex] : les Interval prochains pas, en RLE.
     * Déterministe : même seed + même PathIndex = même rébus.
     * Facile = segments en clair ; sinon braille, formule ou Euler selon la seed.
     */
    static FCubeClue MakeClue(const FString& Seed, const TArray<int32>& PathRoomIDs, int32 GridSize,
                              int32 PathIndex, int32 Interval, ECubeDifficulty Difficulty);
};
