#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BPC_PlayerController.generated.h"

UCLASS()
class CUBE_API ABPC_PlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
    virtual void SetupInputComponent() override;

    /** --- HANDLERS --- */
    void HandleMoveForward(float Value);
    void HandleMoveRight(float Value);
    void HandleTurn(float Value);
    void HandleLookUp(float Value);

    void HandleToggleWingsuit();
    void HandleToggleCameraMode();
    void HandleCycleCameraDistance();
};
