#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CubeRebusTypes.h"
#include "GS_CubeGameState.generated.h"

/** Phase globale de la partie, répliquée à tous les clients. */
UENUM(BlueprintType)
enum class ECubeMatchPhase : uint8
{
    Booting,
    Generating,
    Playing,
    Solved,
    Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCubePhaseChanged, ECubeMatchPhase, NewPhase);

/**
 * État partagé répliqué du niveau : seed, taille, pitch, difficulté, phase.
 * Ne contient AUCUNE donnée secrète (formules/champ mortel restent serveur en compétition).
 */
UCLASS()
class CUBE_API AGS_CubeGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AGS_CubeGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing=OnRep_Seed, BlueprintReadOnly, Category="Cube|State")
    FString Seed;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cube|State")
    int32 GridSize = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cube|State")
    float RoomSize = 500.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cube|State")
    ECubeDifficulty Difficulty = ECubeDifficulty::Facile;

    UPROPERTY(ReplicatedUsing=OnRep_Phase, BlueprintReadOnly, Category="Cube|State")
    ECubeMatchPhase Phase = ECubeMatchPhase::Booting;

    UPROPERTY(BlueprintAssignable, Category="Cube|State")
    FCubePhaseChanged OnPhaseChanged;

    UFUNCTION()
    void OnRep_Seed();

    UFUNCTION()
    void OnRep_Phase();
};
