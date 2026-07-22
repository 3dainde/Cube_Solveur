#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PS_PlayerStateCustom.h"
#include "GM_WingsuitGameMode.generated.h"

UCLASS()
class CUBE_API AGM_WingsuitGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGM_WingsuitGameMode();

    UPROPERTY(EditDefaultsOnly, Category="Game")
    TSubclassOf<APawn> WingsuitPawnClass;

    UFUNCTION(BlueprintCallable)
    void GM_RespawnWinners();
};
