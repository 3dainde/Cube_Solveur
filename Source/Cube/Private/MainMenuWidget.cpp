#include "MainMenuWidget.h"

#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (PlayButton)
    {
        PlayButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandlePlayClicked);
    }
    if (OptionsButton)
    {
        OptionsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleOptionsClicked);
    }
    if (QuitButton)
    {
        QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);
    }
}

void UMainMenuWidget::HandlePlayClicked()
{
    if (!PlayLevel.IsNull())
    {
        UGameplayStatics::OpenLevelBySoftObjectPtr(this, PlayLevel);
    }
}

void UMainMenuWidget::HandleOptionsClicked()
{
    OnOptionsRequested();
}

void UMainMenuWidget::HandleQuitClicked()
{
    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
    }
}
