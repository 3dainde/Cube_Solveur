#include "CubeCharacter.h"
#include "GM_CubeGameMode.h"
#include "GS_CubeGameState.h"
#include "CubeRoomStreamer.h"
#include "PC_CubePlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

static FCubeCoordinate DirectionDelta(ECubeDirection Dir)
{
    switch (Dir)
    {
        case ECubeDirection::East:   return FCubeCoordinate( 1, 0, 0);
        case ECubeDirection::West:   return FCubeCoordinate(-1, 0, 0);
        case ECubeDirection::North:  return FCubeCoordinate( 0, 1, 0);
        case ECubeDirection::South:  return FCubeCoordinate( 0,-1, 0);
        case ECubeDirection::Top:    return FCubeCoordinate( 0, 0, 1);
        case ECubeDirection::Bottom: return FCubeCoordinate( 0, 0,-1);
        default:                     return FCubeCoordinate( 0, 0, 0);
    }
}

ACubeCharacter::ACubeCharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false); // Position dérivée de CurrentCell répliquée.

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(Root);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(Root);
    SpringArm->TargetArmLength = 600.0f;
    SpringArm->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);
}

void ACubeCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplyWorldTransform();
    UpdateStreamerFocus();
}

void ACubeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACubeCharacter, CurrentCell);
}

bool ACubeCharacter::GetGridParams(int32& OutN, float& OutRoomSize) const
{
    OutN = 0; OutRoomSize = 500.0f;
    if (const UWorld* World = GetWorld())
    {
        if (const AGS_CubeGameState* GS = World->GetGameState<AGS_CubeGameState>())
        {
            OutN = GS->GridSize;
            OutRoomSize = GS->RoomSize;
            return OutN > 0;
        }
    }
    return false;
}

void ACubeCharacter::MoveInDirection(ECubeDirection Dir)
{
    if (Dir == ECubeDirection::None) return;
    if (HasAuthority())
    {
        ServerMove_Implementation(Dir);
    }
    else
    {
        ServerMove(Dir);
    }
}

void ACubeCharacter::ServerMove_Implementation(ECubeDirection Dir)
{
    int32 N; float RoomSize;
    if (!GetGridParams(N, RoomSize)) return;

    const FCubeCoordinate Delta = DirectionDelta(Dir);
    const FCubeCoordinate Target(CurrentCell.X + Delta.X, CurrentCell.Y + Delta.Y, CurrentCell.Z + Delta.Z);

    if (Target.X < 0 || Target.X >= N || Target.Y < 0 || Target.Y >= N || Target.Z < 0 || Target.Z >= N)
    {
        return; // Hors bornes : pas de mouvement.
    }

    CurrentCell = Target;
    ApplyWorldTransform();
    UpdateStreamerFocus();
    RefreshLocalHUD();

    // Arbitrage serveur : mort / victoire.
    if (AGM_CubeGameMode* GM = GetWorld()->GetAuthGameMode<AGM_CubeGameMode>())
    {
        GM->NotifyRoomEntered(GetController(), CurrentCell);
    }
}

void ACubeCharacter::SetCell(FCubeCoordinate NewCell, bool bNotify)
{
    CurrentCell = NewCell;
    ApplyWorldTransform();
    UpdateStreamerFocus();
    RefreshLocalHUD();

    if (bNotify && HasAuthority())
    {
        if (AGM_CubeGameMode* GM = GetWorld()->GetAuthGameMode<AGM_CubeGameMode>())
        {
            GM->NotifyRoomEntered(GetController(), CurrentCell);
        }
    }
}

void ACubeCharacter::OnRep_CurrentCell()
{
    ApplyWorldTransform();
    if (IsLocallyControlled())
    {
        UpdateStreamerFocus();
        RefreshLocalHUD();
    }
}

void ACubeCharacter::ApplyWorldTransform()
{
    int32 N; float RoomSize;
    if (!GetGridParams(N, RoomSize)) return;
    SetActorLocation(FCubeMath::CellToWorld(CurrentCell, N, RoomSize));
}

void ACubeCharacter::UpdateStreamerFocus()
{
    if (const UWorld* World = GetWorld())
    {
        if (UCubeRoomStreamer* Streamer = World->GetSubsystem<UCubeRoomStreamer>())
        {
            Streamer->SetFocusCell(CurrentCell);
        }
    }
}

void ACubeCharacter::RefreshLocalHUD()
{
    if (!IsLocallyControlled()) return;
    if (APC_CubePlayerController* PC = Cast<APC_CubePlayerController>(GetController()))
    {
        PC->RefreshRoomHUD();
    }
}
