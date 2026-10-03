#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CubeRebusTypes.h"
#include "CubeLogicTypes.h"
#include "GM_CubeGameMode.generated.h"

class UDA_CubeDifficulty;
class UCubeGenerator;
class AGS_CubeGameState;
class ACubeRoom;

/**
 * GameMode serveur-autoritaire : génère et détient la vérité du niveau
 * (manifest, champ mortel, raccourcis), arbitre morts et victoire.
 * Les données secrètes (formules, champ mortel) restent côté serveur.
 */
UCLASS()
class CUBE_API AGM_CubeGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGM_CubeGameMode();
    virtual void BeginPlay() override;
    virtual void RestartPlayer(AController* NewPlayer) override;

    /** Table de calibrage indexée par difficulté. */
    UPROPERTY(EditDefaultsOnly, Category="Cube|Difficulty")
    TMap<ECubeDifficulty, TObjectPtr<UDA_CubeDifficulty>> DifficultyTable;

    UPROPERTY(EditDefaultsOnly, Category="Cube|Difficulty")
    ECubeDifficulty StartingDifficulty = ECubeDifficulty::Facile;

    /** Espacement métrique des salles (cm). */
    UPROPERTY(EditDefaultsOnly, Category="Cube|Geometry")
    float RoomSize = 500.0f;

    /** Seed forcée (vide = dérivée aléatoirement au démarrage). */
    UPROPERTY(EditDefaultsOnly, Category="Cube|Geometry")
    FString ForcedSeed;

    UPROPERTY(EditDefaultsOnly, Category="Cube|PathFinder")
    bool bEnableShortcuts = true;

    /** Classe de salle utilisée par le streamer (assignée dans un BP dérivé). */
    UPROPERTY(EditDefaultsOnly, Category="Cube|Streaming")
    TSubclassOf<ACubeRoom> RoomClass;

    /** Rayon de streaming (distance de Tchebychev en cellules). */
    UPROPERTY(EditDefaultsOnly, Category="Cube|Streaming")
    int32 StreamRadius = 3;

    /** Notifie l'entrée du joueur dans une cellule (arbitrage mort/victoire). */
    UFUNCTION(BlueprintCallable, Category="Cube|Gameplay")
    void NotifyRoomEntered(AController* Controller, FCubeCoordinate Cell);

    /** Accès lecture seule pour la couche présentation (construction locale). */
    UFUNCTION(BlueprintCallable, Category="Cube|Gameplay")
    const FCubeManifest& GetManifest() const { return Manifest; }

    UFUNCTION(BlueprintCallable, Category="Cube|Gameplay")
    ECubeCellState GetCellState(FCubeCoordinate Cell) const;

    /**
     * Fournit le rébus de la salle : la formule modulaire et l'indice directionnel sûr.
     * Retourne false si la cellule n'est pas une étape (autre que la dernière) du chemin.
     * Réservé aux paliers locaux ; pour WorldChampionship la vérité reste serveur.
     */
    UFUNCTION(BlueprintCallable, Category="Cube|Gameplay")
    bool GetRoomRebus(FCubeCoordinate Cell, FCubeFormula& OutFormula, ECubeDirection& OutSafeHint) const;

    /**
     * Indice de progression de la salle (une salle sur ClueInterval du chemin, sortie exclue).
     * Contient la solution : ne jamais l'envoyer tel quel à un client hors Facile (voir PushRoomHUD).
     */
    UFUNCTION(BlueprintCallable, Category="Cube|Gameplay")
    bool GetRoomClue(FCubeCoordinate Cell, FCubeClue& OutClue) const;

    /** Vérité serveur : champ mortel + zones interdites du mode chemin unique. */
    UFUNCTION(BlueprintCallable, Category="Cube|Gameplay")
    bool IsCellLethal(FCubeCoordinate Cell) const;

protected:
    void BuildLevel();
    int32 PickGridSize(const FString& Seed) const;
    FString ResolveSeed() const;

    /** Difficulté choisie au menu (option d'URL "?Difficulty=Facile|Impossible"), sinon StartingDifficulty. */
    ECubeDifficulty ResolveDifficulty() const;

    /** Envoie au joueur l'indice et les coordonnées de sa salle (solution retirée hors Facile). */
    void PushRoomHUD(AController* Controller, const FCubeCoordinate& Cell) const;

    /** Replace tous les pawns joueurs sur la cellule de départ. */
    void PlaceAllPlayersAtStart();
    FCubeCoordinate GetStartCell() const;

    UPROPERTY() FCubeManifest Manifest;
    UPROPERTY() TObjectPtr<UDA_CubeDifficulty> Config;
    UPROPERTY() TObjectPtr<UCubeGenerator> Generator;

    TArray<int32> PathRoomIDs;
    TMap<int32, int32> PathIndexByRoom;
    TSet<int32> SafeSet;
    TSet<int32> ShortcutSet;
    /** Mode chemin unique : cellules saines qui formeraient un raccourci, traitées comme mortelles. */
    TSet<int32> BridgingSet;
    ECubeDifficulty Difficulty = ECubeDifficulty::Facile;
    float Rho = 0.0f;
    bool bLevelReady = false;
};
