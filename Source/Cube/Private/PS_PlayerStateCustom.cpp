#include "PS_PlayerStateCustom.h"
#include "Net/UnrealNetwork.h"

APS_PlayerStateCustom::APS_PlayerStateCustom()
{
    TeamID = 0;
    TeamTag = FName("TeamA"); // Valeur par défaut
    bIsWinner = false;

    SetReplicates(true);
}

void APS_PlayerStateCustom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APS_PlayerStateCustom, TeamID);
    DOREPLIFETIME(APS_PlayerStateCustom, TeamTag);
    DOREPLIFETIME(APS_PlayerStateCustom, bIsWinner);
    
    DOREPLIFETIME(APS_PlayerStateCustom, Health);
    DOREPLIFETIME(APS_PlayerStateCustom, MaxHealth);
    DOREPLIFETIME(APS_PlayerStateCustom, Ammo);
    DOREPLIFETIME(APS_PlayerStateCustom, Armor);
    DOREPLIFETIME(APS_PlayerStateCustom, XP);
    DOREPLIFETIME(APS_PlayerStateCustom, Currency);
}

void APS_PlayerStateCustom::AddHealth(float Amount)
{
    Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
}

void APS_PlayerStateCustom::AddAmmo(int32 Amount)
{
    Ammo += Amount;
}

void APS_PlayerStateCustom::AddArmor(float Amount)
{
    Armor += Amount;
}

void APS_PlayerStateCustom::AddXP(int32 Amount)
{
    XP += Amount;
}

void APS_PlayerStateCustom::AddCurrency(int32 Amount)
{
    Currency += Amount;
}
