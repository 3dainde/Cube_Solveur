#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PS_PlayerStateCustom.generated.h"

UCLASS()
class CUBE_API APS_PlayerStateCustom : public APlayerState
{
    GENERATED_BODY()

public:
    APS_PlayerStateCustom();

    /** ID de l'équipe, modifiable dans l'éditeur et en Blueprint */
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Team")
    int32 TeamID;

    /** Tag de l'équipe, automatiquement dérivé ou assignable, pour spawn */
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Team")
    FName TeamTag;

    /** Indique si le joueur fait partie des gagnants */
    UPROPERTY(Replicated, BlueprintReadWrite, Category="Player|State")
    bool bIsWinner;

    // ========== STATS ==========
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Stats")
    float Health = 100.0f;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Stats")
    float MaxHealth = 100.0f;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Stats")
    int32 Ammo = 30;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Stats")
    float Armor = 0.0f;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Stats")
    int32 XP = 0;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Player|Stats")
    int32 Currency = 0;

    // Helper functions for Loot
    UFUNCTION(BlueprintCallable, Category="Player|Stats")
    void AddHealth(float Amount);

    UFUNCTION(BlueprintCallable, Category="Player|Stats")
    void AddAmmo(int32 Amount);

    UFUNCTION(BlueprintCallable, Category="Player|Stats")
    void AddArmor(float Amount);

    UFUNCTION(BlueprintCallable, Category="Player|Stats")
    void AddXP(int32 Amount);

    UFUNCTION(BlueprintCallable, Category="Player|Stats")
    void AddCurrency(int32 Amount);


    /** Getter pour l'équipe */
    UFUNCTION(BlueprintCallable, Category="Player|Team")
    int32 GetTeamID() const { return TeamID; }

    UFUNCTION(BlueprintCallable, Category="Player|Team")
    FName GetTeamTag() const { return TeamTag; }

    /** Setter pour l'équipe */
    UFUNCTION(BlueprintCallable, Category="Player|Team")
    void SetTeamID(int32 NewTeamID) { TeamID = NewTeamID; }

    UFUNCTION(BlueprintCallable, Category="Player|Team")
    void SetTeamTag(FName NewTeamTag) { TeamTag = NewTeamTag; }

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
