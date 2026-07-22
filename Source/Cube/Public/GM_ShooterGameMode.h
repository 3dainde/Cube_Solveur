#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PS_PlayerStateCustom.h"
#include "GM_ShooterGameMode.generated.h"

class APlayerStart;
class AMyCharacterWingsuit;

UCLASS()
class CUBE_API AGM_ShooterGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGM_ShooterGameMode();

    /** Nombre de gagnants */
    UPROPERTY(EditDefaultsOnly, Category="Game")
    int32 NumWinners = 5;

    /** Fin de la phase shooter */
    UFUNCTION(BlueprintCallable)
    void GM_EndShooterPhase();

    /** Affiche le podium */
    UFUNCTION(BlueprintCallable)
    void GM_DisplayPodium();

    /** Assigne l’équipe et spawn le joueur */
    void AssignTeamAndSpawn(APlayerController* NewPlayer);

    /** Spawn un joueur sur un PlayerStart selon l’équipe */
    void SpawnPlayerForTeam(AController* NewPlayer, FName TeamTag);

    /** Verrouiller le checkpoint (appelé depuis BP_Checkpoint) */
    UFUNCTION(BlueprintCallable)
    void LockCheckpoint();

    /** Contrôle si le checkpoint est enclenché */
    UPROPERTY(BlueprintReadWrite, Category="Game")
    bool bCheckpointLocked;

protected:
    /** Liste des gagnants */
    TArray<APS_PlayerStateCustom*> Winners;

    /** PlayerStarts par équipe */
    UPROPERTY()
    TArray<APlayerStart*> TeamAStarts;

    UPROPERTY()
    TArray<APlayerStart*> TeamBStarts;

    /** Temp pour spawn unique */
    TArray<APlayerStart*> TempTeamAStarts;
    TArray<APlayerStart*> TempTeamBStarts;

    /** Récupère tous les PlayerStarts avec le tag de l’équipe */
    void InitializeTeamSpawns();

    /** PostLogin pour assigner l’équipe et spawn le joueur */
    virtual void PostLogin(APlayerController* NewPlayer) override;

    /** Logout pour gérer la déconnexion */
    virtual void Logout(AController* Exiting) override;

    /** Spawn automatique des joueurs déjà connectés */
    void SpawnAllPlayersAtStart();

    void DetermineWinners();

    virtual void BeginPlay() override;
};
