#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CubeRebusTypes.h"
#include "DA_CubeDifficulty.generated.h"

/**
 * Données de calibrage par palier de difficulté.
 * Un asset par difficulté (Facile / Impossible / WorldChampionship).
 */
UCLASS(BlueprintType)
class CUBE_API UDA_CubeDifficulty : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty")
    ECubeDifficulty Difficulty = ECubeDifficulty::Facile;

    /** Densité du champ mortel [0..1]. 0 = aucune mort (Facile). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Rho = 0.0f;

    /** Bornes de taille de grille (N in [10,64]). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="10", ClampMax="64"))
    int32 GridSizeMin = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="10", ClampMax="64"))
    int32 GridSizeMax = 32;

    /** Nombre de couples clé/porte le long du chemin (0 à 3 : l'inventaire tient sur 3 bits). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="0", ClampMax="3"))
    int32 NKeys = 2;

    /** Calibrage route : 0 = court/direct, 1 = long/sinueux. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="0.0", ClampMax="1.0"))
    float PathLen = 0.45f;

    /** Biais vertical (puits en Z) du carving. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="0.02", ClampMax="1.0"))
    float Verticality = 0.18f;

    /** Distance maximale (en salles) entre un raccourci sain et le chemin (PathFinder_Annexes). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="1", ClampMax="8"))
    int32 ShortcutReach = 2;

    /** Tier des formules murales (1..). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty", meta=(ClampMin="1"))
    int32 FormulaTier = 1;

    /** Active les directions leurres (decoy) sur les formules. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty")
    bool bEnableDecoy = false;

    /** Boussole d'aide active (Facile uniquement). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Difficulty")
    bool bCompassEnabled = false;

    /**
     * Chemin unique strict : toute zone saine qui relierait deux portions du chemin devient mortelle.
     * Restent saines : le chemin du solveur et les culs-de-sac (pièces safe hors chemin).
     * Rho ~0.6 donne une dizaine de pièces safe ; un Rho bas les fait presque toutes disparaître.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Clue")
    bool bStrictUniquePath = false;

    /** Un indice de progression tous les N pas du chemin. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Clue", meta=(ClampMin="1", ClampMax="20"))
    int32 ClueInterval = 5;

    /** Affiche les coordonnées XYZ du joueur dans le HUD (le gizmo d'axes reste toujours visible). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Clue")
    bool bShowCoordinates = false;
};
