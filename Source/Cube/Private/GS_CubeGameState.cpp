#include "GS_CubeGameState.h"
#include "Net/UnrealNetwork.h"

AGS_CubeGameState::AGS_CubeGameState()
{
    bReplicates = true;
}

void AGS_CubeGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGS_CubeGameState, Seed);
    DOREPLIFETIME(AGS_CubeGameState, GridSize);
    DOREPLIFETIME(AGS_CubeGameState, RoomSize);
    DOREPLIFETIME(AGS_CubeGameState, Difficulty);
    DOREPLIFETIME(AGS_CubeGameState, Phase);
}

void AGS_CubeGameState::OnRep_Seed()
{
    // Les clients peuvent régénérer la présentation locale à partir de la seed.
}

void AGS_CubeGameState::OnRep_Phase()
{
    OnPhaseChanged.Broadcast(Phase);
}
