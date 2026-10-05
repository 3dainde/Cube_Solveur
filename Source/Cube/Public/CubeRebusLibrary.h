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

    /** Hash de base du champ mortel d'une seed : Combine(CRC32(seed), CRC32("TRAP")). À calculer une fois. */
    static uint32 GetTrapSeedHash(const FString& Seed);

    /** Seuil comparé à H mod 10000 : floor(Rho * 10000), calculé en double comme la R&D. */
    static int32 GetRhoLimit(double Rho);

    /** Variante rapide de IsLethal (sans SafeSet) pour les boucles sur toute la grille. */
    static bool IsLethalFast(uint32 TrapSeedHash, int32 X, int32 Y, int32 Z, int32 RhoLimit);

    UFUNCTION(BlueprintCallable, Category="Cube|Rebus")
    static bool IsLethalCell(const FString& Seed, FCubeCoordinate Cell,
                             float Rho, int32 GridSize, const TSet<int32>& SafeSet);

    /**
     * PathFinder_Annexes : cellules SafeShortcut = petits détours NON mortels, hors chemin, à au plus
     * Reach salles du chemin, situés sur le plus court détour qui relie deux étapes i < j du chemin en
     * GAGNANT des pas (détour de L salles retenu si L + 1 < j - i). Coût linéaire (BFS borné), parité R&D.
     */
    UFUNCTION(BlueprintCallable, Category="Cube|Rebus")
    static TSet<int32> ComputeSafeShortcuts(const FString& Seed, const TArray<int32>& PathRoomIDs,
                                            int32 GridSize, float Rho, int32 Reach = 2);

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
