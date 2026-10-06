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
    if (EasyButton)
    {
        EasyButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleEasyClicked);
    }
    if (ImpossibleButton)
    {
        ImpossibleButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleImpossibleClicked);
    }
}

void UMainMenuWidget::HandlePlayClicked()
{
    OpenPlayLevel(FString());
}

void UMainMenuWidget::HandleEasyClicked()
{
    OpenPlayLevel(TEXT("Difficulty=Facile"));
}

void UMainMenuWidget::HandleImpossibleClicked()
{
    OpenPlayLevel(TEXT("Difficulty=Impossible"));
}

void UMainMenuWidget::OpenPlayLevel(const FString& Options)
{
    if (!PlayLevel.IsNull())
    {
        UGameplayStatics::OpenLevelBySoftObjectPtr(this, PlayLevel, true, Options);
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
