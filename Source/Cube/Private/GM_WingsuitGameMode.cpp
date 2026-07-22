#include "GM_WingsuitGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AGM_WingsuitGameMode::AGM_WingsuitGameMode()
{
    PlayerStateClass = APS_PlayerStateCustom::StaticClass();
}

void AGM_WingsuitGameMode::GM_RespawnWinners()
{
    TArray<AActor*> PlayerStarts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);

    int32 StartIndex = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            APS_PlayerStateCustom* PSC = Cast<APS_PlayerStateCustom>(PC->PlayerState);
            if (PSC && PSC->bIsWinner)
            {
                if (StartIndex >= PlayerStarts.Num()) break;

                FVector SpawnLocation = PlayerStarts[StartIndex]->GetActorLocation();
                FRotator SpawnRotation = PlayerStarts[StartIndex]->GetActorRotation();

                APawn* NewPawn = GetWorld()->SpawnActor<APawn>(WingsuitPawnClass, SpawnLocation, SpawnRotation);
                PC->Possess(NewPawn);

                StartIndex++;
            }
            else
            {
                PC->StartSpectatingOnly();
            }
        }
    }
}
