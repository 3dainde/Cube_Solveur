#include "GM_ShooterGameMode.h"
#include "BPC_PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "MyCharacterWingsuit.h"
#include "PS_PlayerStateCustom.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

AGM_ShooterGameMode::AGM_ShooterGameMode()
{
    // Définit le Pawn par défaut
    DefaultPawnClass = AMyCharacterWingsuit::StaticClass();
    // Définit ton PlayerController custom
    PlayerControllerClass = ABPC_PlayerController::StaticClass(); // ✅ important
    // Définit le PlayerState custom
    PlayerStateClass = APS_PlayerStateCustom::StaticClass();
    // Checkpoint non verrouillé au départ
    bCheckpointLocked = false;
}

void AGM_ShooterGameMode::BeginPlay()
{
    Super::BeginPlay();

    InitializeTeamSpawns();
    TempTeamAStarts = TeamAStarts;
    TempTeamBStarts = TeamBStarts;

    SpawnAllPlayersAtStart();
}

void AGM_ShooterGameMode::InitializeTeamSpawns()
{
    TeamAStarts.Empty();
    TeamBStarts.Empty();

    TArray<AActor*> FoundStarts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundStarts);

    for (AActor* StartActor : FoundStarts)
    {
        if (StartActor->ActorHasTag(FName("TeamA")))
            TeamAStarts.Add(Cast<APlayerStart>(StartActor));
        else if (StartActor->ActorHasTag(FName("TeamB")))
            TeamBStarts.Add(Cast<APlayerStart>(StartActor));
    }
}

void AGM_ShooterGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (bCheckpointLocked) return;

    AssignTeamAndSpawn(NewPlayer);
}

void AGM_ShooterGameMode::AssignTeamAndSpawn(APlayerController* NewPlayer)
{
    if (!NewPlayer || !NewPlayer->PlayerState) return;

    APS_PlayerStateCustom* PSC = Cast<APS_PlayerStateCustom>(NewPlayer->PlayerState);
    if (!PSC) return;

    int32 TeamACount = 0;
    int32 TeamBCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        APS_PlayerStateCustom* OtherPSC = Cast<APS_PlayerStateCustom>(PC->PlayerState);
        if (OtherPSC)
        {
            if (OtherPSC->GetTeamTag() == FName("TeamA")) TeamACount++;
            else if (OtherPSC->GetTeamTag() == FName("TeamB")) TeamBCount++;
        }
    }

    FName AssignedTeam = (TeamACount <= TeamBCount) ? FName("TeamA") : FName("TeamB");
    PSC->SetTeamTag(AssignedTeam);
    PSC->SetTeamID((AssignedTeam == FName("TeamA")) ? 0 : 1);

    SpawnPlayerForTeam(NewPlayer, AssignedTeam);
}

void AGM_ShooterGameMode::SpawnPlayerForTeam(AController* NewPlayer, FName TeamTag)
{
    if (!NewPlayer) return;

    TArray<APlayerStart*>* SpawnArray = nullptr;
    if (TeamTag == FName("TeamA")) SpawnArray = &TempTeamAStarts;
    else if (TeamTag == FName("TeamB")) SpawnArray = &TempTeamBStarts;

    if (!SpawnArray || SpawnArray->Num() == 0) return;

    int32 Index = FMath::RandRange(0, SpawnArray->Num() - 1);
    APlayerStart* ChosenStart = (*SpawnArray)[Index];
    SpawnArray->RemoveAt(Index);

    if (ChosenStart)
    {
        FVector Location = ChosenStart->GetActorLocation();
        FRotator Rotation = ChosenStart->GetActorRotation();

        FActorSpawnParameters Params;
        Params.Owner = NewPlayer;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AMyCharacterWingsuit* NewPawn = GetWorld()->SpawnActor<AMyCharacterWingsuit>(
            AMyCharacterWingsuit::StaticClass(),
            Location,
            Rotation,
            Params
        );

        if (NewPawn) NewPlayer->Possess(NewPawn);
    }
}

void AGM_ShooterGameMode::SpawnAllPlayersAtStart()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->PlayerState) AssignTeamAndSpawn(PC);
    }
}

void AGM_ShooterGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    if (!Exiting || !Exiting->PlayerState) return;

    APS_PlayerStateCustom* PSC = Cast<APS_PlayerStateCustom>(Exiting->PlayerState);
    if (!PSC) return;

    FName TeamTag = PSC->GetTeamTag();
    if (TeamTag == FName("TeamA"))
    {
        TempTeamAStarts.Append(TeamAStarts.FilterByPredicate([&](APlayerStart* Start){ return !TempTeamAStarts.Contains(Start); }));
    }
    else if (TeamTag == FName("TeamB"))
    {
        TempTeamBStarts.Append(TeamBStarts.FilterByPredicate([&](APlayerStart* Start){ return !TempTeamBStarts.Contains(Start); }));
    }
}

void AGM_ShooterGameMode::LockCheckpoint()
{
    bCheckpointLocked = true;
}

void AGM_ShooterGameMode::GM_EndShooterPhase()
{
    DetermineWinners();
}

void AGM_ShooterGameMode::GM_DisplayPodium() {}
void AGM_ShooterGameMode::DetermineWinners() {}
