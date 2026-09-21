#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuGameMode.generated.h"

class UUserWidget;

UCLASS()
class CUBE_API AMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMenuGameMode();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
    TSubclassOf<UUserWidget> MainMenuWidgetClass;

private:
    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> MainMenuWidget;
};
