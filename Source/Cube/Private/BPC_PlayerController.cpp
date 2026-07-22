#include "BPC_PlayerController.h"
#include "MyCharacterWingsuit.h"

void ABPC_PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    InputComponent->BindAxis("MoveForward", this, &ABPC_PlayerController::HandleMoveForward);
    InputComponent->BindAxis("MoveRight", this, &ABPC_PlayerController::HandleMoveRight);
    InputComponent->BindAxis("Turn", this, &ABPC_PlayerController::HandleTurn);
    InputComponent->BindAxis("LookUp", this, &ABPC_PlayerController::HandleLookUp);

    InputComponent->BindAction("ToggleWingsuit", IE_Pressed, this, &ABPC_PlayerController::HandleToggleWingsuit);
    InputComponent->BindAction("ToggleCameraMode", IE_Pressed, this, &ABPC_PlayerController::HandleToggleCameraMode);
    InputComponent->BindAction("CycleCameraDistance", IE_Pressed, this, &ABPC_PlayerController::HandleCycleCameraDistance);
}

void ABPC_PlayerController::HandleMoveForward(float Value)
{
    if (AMyCharacterWingsuit* Char = Cast<AMyCharacterWingsuit>(GetPawn()))
    {
        Char->MoveForward(Value);
    }
}

void ABPC_PlayerController::HandleMoveRight(float Value)
{
    if (AMyCharacterWingsuit* Char = Cast<AMyCharacterWingsuit>(GetPawn()))
    {
        Char->MoveRight(Value);
    }
}

void ABPC_PlayerController::HandleTurn(float Value)
{
    if (AMyCharacterWingsuit* Char = Cast<AMyCharacterWingsuit>(GetPawn()))
    {
        Char->Turn(Value);
    }
}

void ABPC_PlayerController::HandleLookUp(float Value)
{
    if (AMyCharacterWingsuit* Char = Cast<AMyCharacterWingsuit>(GetPawn()))
    {
        Char->LookUp(Value);
    }
}

void ABPC_PlayerController::HandleToggleWingsuit()
{
    if (AMyCharacterWingsuit* Char = Cast<AMyCharacterWingsuit>(GetPawn()))
    {
        Char->ToggleWingsuit();
    }
}

void ABPC_PlayerController::HandleToggleCameraMode()
{
    if (AMyCharacterWingsuit* Char = Cast<AMyCharacterWingsuit>(GetPawn()))
    {
        Char->ToggleCameraMode();
    }
}

void ABPC_PlayerController::HandleCycleCameraDistance()
{
    if (AMyCharacterWingsuit* Char = Cast<AMyCharacterWingsuit>(GetPawn()))
    {
        Char->CycleCameraDistance();
    }
}
