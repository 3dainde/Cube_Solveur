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

    // Optionnels : ajouter dans le WBP des boutons nommés exactement EasyButton / ImpossibleButton.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> EasyButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ImpossibleButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Menu")
    TSoftObjectPtr<UWorld> PlayLevel;

    UFUNCTION()
    void HandlePlayClicked();

    UFUNCTION()
    void HandleOptionsClicked();

    UFUNCTION()
    void HandleQuitClicked();

    UFUNCTION()
    void HandleEasyClicked();

    UFUNCTION()
    void HandleImpossibleClicked();

    /** Ouvre PlayLevel en passant les options au GameMode (ex. "Difficulty=Impossible"). */
    UFUNCTION(BlueprintCallable, Category = "Main Menu")
    void OpenPlayLevel(const FString& Options);

    // Laisse le WBP afficher son panneau d'options sans code supplementaire.
    UFUNCTION(BlueprintImplementableEvent, Category = "Main Menu")
    void OnOptionsRequested();
};
