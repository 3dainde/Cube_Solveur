#include "CubeGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"

UCubeGameInstance::UCubeGameInstance()
{
    OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCubeGameInstance::OnCreateSessionComplete);
    OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCubeGameInstance::OnDestroySessionComplete);
    OnFindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCubeGameInstance::OnFindSessionsComplete);
    OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCubeGameInstance::OnJoinSessionComplete);
}

void UCubeGameInstance::Init()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        SessionInterface = Subsystem->GetSessionInterface();
    }
}

void UCubeGameInstance::CreateSession(int32 NumPublicConnections, bool bIsLAN)
{
    if (!SessionInterface.IsValid()) return;

    FOnlineSessionSettings SessionSettings;
    SessionSettings.NumPublicConnections = NumPublicConnections;
    SessionSettings.bIsLANMatch = bIsLAN;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bAllowJoinInProgress = true;

    SessionInterface->CreateSession(0, FName("CubeSession"), SessionSettings);
}

void UCubeGameInstance::DestroySession()
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->DestroySession(FName("CubeSession"));
    }
}

void UCubeGameInstance::JoinSession()
{
    if (!SessionInterface.IsValid()) return;

    SessionSearch = MakeShareable(new FOnlineSessionSearch());
    SessionSearch->bIsLanQuery = false;
    SessionSearch->MaxSearchResults = 100;
    SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCE")), true, EOnlineComparisonOp::Equals);

    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UCubeGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Log, TEXT("Session %s creation %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
}

void UCubeGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Log, TEXT("Session %s destroyed %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
}

void UCubeGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
    UE_LOG(LogTemp, Log, TEXT("Find sessions %s"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));

    if (bWasSuccessful && SessionSearch.IsValid())
    {
        for (auto& Result : SessionSearch->SearchResults)
        {
            if (Result.IsValid())
            {
                SessionInterface->JoinSession(0, FName("CubeSession"), Result);
                return;
            }
        }
    }
}

void UCubeGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    UE_LOG(LogTemp, Log, TEXT("Join session %s with result %d"), *SessionName.ToString(), (int32)Result);
}
