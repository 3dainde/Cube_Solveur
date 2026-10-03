#pragma once

#include "CoreMinimal.h"
#include "CubeCore.h"                 // ECubeDirection, FCubeCoordinate, FCubeMath
#include "CubeRebusTypes.generated.h"

/** Palier de difficulté. */
UENUM(BlueprintType)
enum class ECubeDifficulty : uint8
{
    Facile,             // T0 : route RLE complète, aucune mort, boussole active
    Impossible,         // T1 : grilles 10..50, champ mortel local (Rho), formules modulaires
    WorldChampionship   // T2 : secret serveur-autoritaire, formules masquées en RAM
};

/** État logique d'une pièce (+ conventions de couleurs DEBUG). */
UENUM(BlueprintType)
enum class ECubeCellState : uint8
{
    SafePath,     // Sur le chemin critique du solveur       — Debug : Turquoise
    SafeShortcut, // Raccourci sain reliant deux pas du chemin — Debug : Vert
    Lethal,       // Piège mortel au contact                 — Debug : Rouge
    Inactive,     // Roche/Mur ou cul-de-sac non reliant      — Debug : Gris sombre
    Start,        // Entrée
    Exit,         // Sortie
    Key,          // Emplacement clé
    Gate          // Porte verrouillée
};

/** Un terme RLE : direction + longueur (ex. +Y x2 -> "Y+(2)"). */
USTRUCT(BlueprintType)
struct CUBE_API FCubeRouteToken
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") ECubeDirection Dir = ECubeDirection::None;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 Run = 0;
};

/**
 * Formule affichée sur le mur. Le résultat sénaire d in {0..5} désigne la
 * direction sûre (index de ECubeDirection - 1).
 */
USTRUCT(BlueprintType)
struct CUBE_API FCubeFormula
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 Tier = 1;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 A1 = 0;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 A2 = 0;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 A3 = 0;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 A4 = 0;   // Terme libre calibré
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 Mod = 6;

    // Non exposé au client en compétition ; validateur serveur uniquement.
    int32 Answer = 0;
    bool bEnableDecoy = false;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") ECubeDirection Decoy = ECubeDirection::None;
};

/** Présentation d'un indice de progression (un indice tous les ClueInterval pas du chemin). */
UENUM(BlueprintType)
enum class ECubeClueStyle : uint8
{
    Plain,    // Facile : segments en clair, ex. "X+3  Y+1  Z-1"
    Braille,  // Une cellule braille par segment : 6 points = 6 bits
    Formula,  // Expression dont le résultat, écrit en binaire 6 bits, donne le segment
    Euler     // Rotation (Pitch, Yaw, Roll) en radians + distance en formule
};

/**
 * Indice de progression : les prochains pas du chemin critique, compressés en RLE.
 * Chaque segment est codé sur 6 bits :
 *   [b5 b4] axe (01 = X, 10 = Y, 11 = Z) | [b3] signe (1 = négatif) | [b2 b1 b0] longueur 1..7
 *   Ex. X+1 = 01 0 001 = 17 ; Z-2 = 11 1 010 = 58.
 */
USTRUCT(BlueprintType)
struct CUBE_API FCubeClue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cube|Clue") int32 PathIndex = -1;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Clue") ECubeClueStyle Style = ECubeClueStyle::Plain;

    /** Solution (segments RLE). Vidée avant l'envoi au client hors Facile. */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Clue") TArray<FCubeRouteToken> Tokens;

    /** Code 6 bits de chaque segment. Vidé avant l'envoi au client hors Facile. */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Clue") TArray<int32> Codes;

    /** Une ligne de rébus par segment, dans l'ordre. */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Clue") TArray<FString> PuzzleLines;

    /** Lignes jointes, prêtes à afficher. */
    UPROPERTY(BlueprintReadOnly, Category="Cube|Clue") FString PuzzleText;

    void StripSolution() { Tokens.Reset(); Codes.Reset(); }
};

/** Configuration complète d'une salle pour BP_CUBE. */
USTRUCT(BlueprintType)
struct CUBE_API FCubeRoomData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 RoomID = -1;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") FCubeCoordinate Cell;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") ECubeCellState State = ECubeCellState::Inactive;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") int32 PathIndex = -1;   // Rang sur le chemin (-1 si hors chemin)
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") bool bHasFormula = false;
    UPROPERTY(BlueprintReadOnly, Category="Cube|Rebus") FCubeFormula Formula;
};
