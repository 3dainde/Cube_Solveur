#include "CubeRoom.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ACubeRoom::ACubeRoom()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
    ShellMesh->SetupAttachment(Root);
    ShellMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

FLinearColor ACubeRoom::DebugColorForState(ECubeCellState State)
{
    switch (State)
    {
        case ECubeCellState::SafePath:     return FLinearColor(0.00f, 0.85f, 0.85f); // Turquoise
        case ECubeCellState::SafeShortcut: return FLinearColor(0.10f, 0.85f, 0.10f); // Vert
        case ECubeCellState::Lethal:       return FLinearColor(0.85f, 0.10f, 0.10f); // Rouge
        case ECubeCellState::Start:        return FLinearColor(0.20f, 0.40f, 1.00f); // Bleu
        case ECubeCellState::Exit:         return FLinearColor(1.00f, 0.85f, 0.10f); // Or
        case ECubeCellState::Key:          return FLinearColor(0.90f, 0.60f, 1.00f); // Violet
        case ECubeCellState::Gate:         return FLinearColor(1.00f, 0.45f, 0.00f); // Orange
        case ECubeCellState::Inactive:
        default:                           return FLinearColor(0.05f, 0.05f, 0.06f); // Gris sombre
    }
}

void ACubeRoom::Setup(const FCubeRoomData& InData, int32 GridSize, float RoomSize)
{
    Data = InData;

    SetActorLocation(FCubeMath::CellToWorld(Data.Cell, GridSize, RoomSize));
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);

    // Couleur de debug (le mesh doit exposer un paramètre vectoriel "DebugColor").
    if (ShellMesh && ShellMesh->GetMaterial(0))
    {
        if (!DebugMID)
        {
            DebugMID = ShellMesh->CreateDynamicMaterialInstance(0);
        }
        if (DebugMID)
        {
            DebugMID->SetVectorParameterValue(TEXT("DebugColor"), DebugColorForState(Data.State));
        }
    }

    OnRoomConfigured(Data);
}

void ACubeRoom::ReturnToPool()
{
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    OnRoomPooled();
}
