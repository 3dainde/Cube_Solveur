#include "PC_CubePlayerController.h"
#include "CubeCharacter.h"
#include "GM_CubeGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/World.h"

APC_CubePlayerController::APC_CubePlayerController()
{
}

void APC_CubePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (MappingContext)
            {
                Subsystem->AddMappingContext(MappingContext, MappingPriority);
            }
        }
    }
}

void APC_CubePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC) return;

    if (IA_MoveEast)   EIC->BindAction(IA_MoveEast,   ETriggerEvent::Triggered, this, &APC_CubePlayerController::StepEast);
    if (IA_MoveWest)   EIC->BindAction(IA_MoveWest,   ETriggerEvent::Triggered, this, &APC_CubePlayerController::StepWest);
    if (IA_MoveNorth)  EIC->BindAction(IA_MoveNorth,  ETriggerEvent::Triggered, this, &APC_CubePlayerController::StepNorth);
    if (IA_MoveSouth)  EIC->BindAction(IA_MoveSouth,  ETriggerEvent::Triggered, this, &APC_CubePlayerController::StepSouth);
    if (IA_MoveTop)    EIC->BindAction(IA_MoveTop,    ETriggerEvent::Triggered, this, &APC_CubePlayerController::StepTop);
    if (IA_MoveBottom) EIC->BindAction(IA_MoveBottom, ETriggerEvent::Triggered, this, &APC_CubePlayerController::StepBottom);
}

void APC_CubePlayerController::StepEast(const FInputActionValue&)   { DoStep(ECubeDirection::East); }
void APC_CubePlayerController::StepWest(const FInputActionValue&)   { DoStep(ECubeDirection::West); }
void APC_CubePlayerController::StepNorth(const FInputActionValue&)  { DoStep(ECubeDirection::North); }
void APC_CubePlayerController::StepSouth(const FInputActionValue&)  { DoStep(ECubeDirection::South); }
void APC_CubePlayerController::StepTop(const FInputActionValue&)    { DoStep(ECubeDirection::Top); }
void APC_CubePlayerController::StepBottom(const FInputActionValue&) { DoStep(ECubeDirection::Bottom); }

void APC_CubePlayerController::DoStep(ECubeDirection Dir)
{
    if (ACubeCharacter* Cube = Cast<ACubeCharacter>(GetPawn()))
    {
        Cube->MoveInDirection(Dir);
    }
}

void APC_CubePlayerController::ClientReceiveRoomClue_Implementation(const FCubeClue& Clue, bool bHasClue,
                                                                   FCubeCoordinate Cell, bool bShowCoordinates)
{
    OnCoordinatesUpdated(Cell, bShowCoordinates);
    OnClueUpdated(Clue, bHasClue);
}

void APC_CubePlayerController::RefreshRoomHUD()
{
    ACubeCharacter* Cube = Cast<ACubeCharacter>(GetPawn());
    if (!Cube) return;

    // La vérité du rébus est locale (GameMode accessible en standalone/listen/PIE).
    // Pour un client pur en WorldChampionship, le GameMode est absent : le HUD reste vierge.
    AGM_CubeGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AGM_CubeGameMode>() : nullptr;
    if (!GM)
    {
        OnFormulaUpdated(FCubeFormula(), false);
        OnCompassUpdated(ECubeDirection::None, false);
        return;
    }

    FCubeFormula Formula;
    ECubeDirection SafeHint = ECubeDirection::None;
    const bool bHasRebus = GM->GetRoomRebus(Cube->GetCurrentCell(), Formula, SafeHint);

    OnFormulaUpdated(Formula, bHasRebus);
    OnCompassUpdated(SafeHint, bHasRebus);
}
