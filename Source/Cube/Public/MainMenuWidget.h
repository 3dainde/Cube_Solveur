#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;

UCLASS()
class CUBE_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> PlayButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> OptionsButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> QuitButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Menu")
    TSoftObjectPtr<UWorld> PlayLevel;

    UFUNCTION()
    void HandlePlayClicked();

    UFUNCTION()
    void HandleOptionsClicked();

    UFUNCTION()
    void HandleQuitClicked();

    // Laisse le WBP afficher son panneau d'options sans code supplementaire.
    UFUNCTION(BlueprintImplementableEvent, Category = "Main Menu")
    void OnOptionsRequested();
};
