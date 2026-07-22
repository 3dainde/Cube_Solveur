#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "CubeGameInstance.generated.h"

UCLASS()
class CUBE_API UCubeGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    UCubeGameInstance();

    virtual void Init() override;

    // Créer une session
    UFUNCTION(BlueprintCallable)
    void CreateSession(int32 NumPublicConnections, bool bIsLAN);

    // Rejoindre une session
    UFUNCTION(BlueprintCallable)
    void JoinSession();

    using UGameInstance::JoinSession;

    // Détruire la session
    UFUNCTION(BlueprintCallable)
    void DestroySession();

private:
    IOnlineSessionPtr SessionInterface;

    FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
    FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;
    FOnFindSessionsCompleteDelegate OnFindSessionsCompleteDelegate;
    FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate;

    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    TSharedPtr<FOnlineSessionSearch> SessionSearch;
};
