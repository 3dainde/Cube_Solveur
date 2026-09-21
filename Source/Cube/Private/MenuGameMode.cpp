#include "MenuGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AMenuGameMode::AMenuGameMode()
{
    // Menu statique : aucun pion a faire apparaitre.
    DefaultPawnClass = nullptr;
}

void AMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    if (!PlayerController)
    {
        return;
    }

    // Utilise la premiere Camera Actor de la scene comme point de vue du menu.
    if (World)
    {
        for (TActorIterator<ACameraActor> It(World); It; ++It)
        {
            PlayerController->SetViewTargetWithBlend(*It);
            break;
        }
    }

    if (!MainMenuWidgetClass)
    {
        return;
    }

    MainMenuWidget = CreateWidget<UUserWidget>(PlayerController, MainMenuWidgetClass);
    if (!MainMenuWidget)
    {
        return;
    }

    MainMenuWidget->AddToViewport();

    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = true;
}
