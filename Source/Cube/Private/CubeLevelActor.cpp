#include "CubeLevelActor.h"
#include "CubeGenerator.h"
#include "CubeRebusLibrary.h"
#include "Components/ArrowComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACubeLevelActor::ACubeLevelActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;                                    // deux ancrages : toujours pertinents

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PreviewRooms = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewRooms"));
    PreviewRooms->SetupAttachment(SceneRoot);
    PreviewRooms->NumCustomDataFloats = 1;                  // PerInstanceCustomData[0] = ECubeViewMaterial
    PreviewRooms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewRooms->SetMobility(EComponentMobility::Movable);   // la racine est mobile (replacée à l'origine en jeu)

    EntryAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("Socket_Entree"));
    EntryAnchor->SetupAttachment(SceneRoot);
    ExitAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("Socket_Sortie"));
    ExitAnchor->SetupAttachment(SceneRoot);

#if WITH_EDITORONLY_DATA
    EntryArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EntryArrow"));
    if (EntryArrow)
    {
        EntryArrow->SetupAttachment(EntryAnchor);
        EntryArrow->ArrowColor = FColor(30, 220, 90);
        EntryArrow->ArrowSize = 2.0f;
    }
    ExitArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ExitArrow"));
    if (ExitArrow)
    {
        ExitArrow->SetupAttachment(ExitAnchor);
        ExitArrow->ArrowColor = FColor(230, 60, 60);
        ExitArrow->ArrowSize = 2.0f;
    }
#endif

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        RoomMesh = CubeMesh.Object;
    }
}

void ACubeLevelActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACubeLevelActor, EntryPortal);
    DOREPLIFETIME(ACubeLevelActor, ExitPortal);
}

void ACubeLevelActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    const UWorld* World = GetWorld();
    if (bPreviewInEditor && World && !World->IsGameWorld())
    {
        RegeneratePreviewInEditor();
    }
}

void ACubeLevelActor::RegeneratePreviewInEditor()
{
    // Même clé que le cache de la R&D : la génération ne se relance que si seed/paramètres changent.
    const FString Key = FString::Printf(TEXT("%s|%d|%.4f|%.4f|%d"), *PreviewSeed, PreviewGridSize,
                                        PreviewPathLen, PreviewVerticality, PreviewNKeys);
    if (Key != CachedKey)
    {
        Manifest = UCubeGenerator::Generate(PreviewSeed, PreviewGridSize, PreviewPathLen, PreviewVerticality, PreviewNKeys);
        CachedKey = Key;
        bMaterialsValid = false;
        bExternalShortcuts = false;
    }
    if (!Manifest.bIsValid)
    {
        PreviewRooms->ClearInstances();
        return;
    }
    EntryPortal = UCubePortalLibrary::ComputePortal(Manifest, true, RoomSize);
    ExitPortal = UCubePortalLibrary::ComputePortal(Manifest, false, RoomSize);
    ApplyAnchors();
    RefreshPreview();
}

void ACubeLevelActor::InitFromManifest(const FCubeManifest& InManifest, float InRoomSize, float InRho, const TSet<int32>& InShortcuts)
{
    if (!HasAuthority())
    {
        return;
    }
    RoomSize = InRoomSize;
    EntryPortal = UCubePortalLibrary::ComputePortal(InManifest, true, RoomSize);
    ExitPortal = UCubePortalLibrary::ComputePortal(InManifest, false, RoomSize);
    ApplyAnchors();

    if (bShowPreviewInGame)
    {
        Manifest = InManifest;
        Shortcuts = InShortcuts;
        ViewParams.Rho = InRho;
        CachedKey.Reset();
        bExternalShortcuts = true;
        bMaterialsValid = false;
        RefreshPreview();
    }
    else
    {
        PreviewRooms->ClearInstances();
    }
}

void ACubeLevelActor::OnRep_Portals()
{
    ApplyAnchors();
}

void ACubeLevelActor::ApplyAnchors()
{
    EntryAnchor->SetRelativeTransform(UCubePortalLibrary::GetAnchorTransform(EntryPortal));
    ExitAnchor->SetRelativeTransform(UCubePortalLibrary::GetAnchorTransform(ExitPortal));
}

void ACubeLevelActor::RefreshPreview()
{
    if (!Manifest.bIsValid || !PreviewRooms)
    {
        return;
    }

    // Matériaux par salle : recalculés seulement si seed, rho ou portée des raccourcis changent.
    const bool bShowShortcuts = ViewParams.bShowShortcuts && ViewParams.Rho > 0.0f;
    if (!bMaterialsValid || CachedRho != ViewParams.Rho || CachedReach != ViewParams.ShortcutReach || bCachedShowShortcuts != bShowShortcuts)
    {
        if (!bExternalShortcuts)
        {
            Shortcuts = bShowShortcuts
                ? UCubeRebusLibrary::ComputeSafeShortcuts(Manifest.Seed, Manifest.CriticalPath, Manifest.GridSize, ViewParams.Rho, ViewParams.ShortcutReach)
                : TSet<int32>();
        }
        UCubeViewLibrary::BuildCellMaterials(Manifest, ViewParams, Shortcuts, CellMaterials);
        bMaterialsValid = true;
        CachedRho = ViewParams.Rho;
        CachedReach = ViewParams.ShortcutReach;
        bCachedShowShortcuts = bShowShortcuts;
    }
    UCubeViewLibrary::BuildViewCached(Manifest, ViewParams, Shortcuts, CellMaterials, ViewInstances);

    if (PreviewRooms->GetStaticMesh() != RoomMesh)
    {
        PreviewRooms->SetStaticMesh(RoomMesh);
    }

    const double Scale = static_cast<double>(RoomSize) * RoomFill / RoomMeshSize;
    const int32 N = Manifest.GridSize;
    const int32 Num = ViewInstances.RoomIDs.Num();
    InstanceTransforms.Reset(Num);
    for (int32 I = 0; I < Num; ++I)
    {
        const FCubeCoordinate Cell = FCubeMath::DecodeCoord(ViewInstances.RoomIDs[I], N, N);
        InstanceTransforms.Add(FTransform(FQuat::Identity, FCubeMath::CellToWorld(Cell, N, RoomSize), FVector(Scale)));
    }

    // Un seul ajout groupé, puis les données d'instance sans invalider le rendu à chaque appel.
    PreviewRooms->ClearInstances();
    PreviewRooms->AddInstances(InstanceTransforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);
    for (int32 I = 0; I < Num; ++I)
    {
        const float MaterialIndex = static_cast<float>(ViewInstances.Materials[I]);
        PreviewRooms->SetCustomData(I, TArrayView<const float>(&MaterialIndex, 1), /*bMarkRenderStateDirty*/ false);
    }
    PreviewRooms->MarkRenderStateDirty();
}

USceneComponent* ACubeLevelActor::GetAnchor(bool bEntry) const
{
    return bEntry ? EntryAnchor.Get() : ExitAnchor.Get();
}

FTransform ACubeLevelActor::GetAnchorWorldTransform(bool bEntry) const
{
    const USceneComponent* Anchor = GetAnchor(bEntry);
    return Anchor ? Anchor->GetComponentTransform() : FTransform::Identity;
}

bool ACubeLevelActor::AttachBridge(AActor* Bridge, bool bEntry, FName BridgeSocket)
{
    USceneComponent* Anchor = GetAnchor(bEntry);
    const FCubePortal& Portal = bEntry ? EntryPortal : ExitPortal;
    if (BridgeSocket.IsNone())
    {
        BridgeSocket = TEXT("Socket_Cube");
    }
    if (!IsValid(Bridge) || !Anchor || !Portal.bValid)
    {
        return false;
    }

    TInlineComponentArray<USceneComponent*> Components(Bridge);
    for (USceneComponent* Component : Components)
    {
        if (Component && Component->DoesSocketExist(BridgeSocket))
        {
            const FTransform SocketRelative = Component->GetSocketTransform(BridgeSocket, RTS_Actor);
            Bridge->SetActorTransform(UCubePortalLibrary::ComputeBridgeTransform(Anchor->GetComponentTransform(), SocketRelative));
            Bridge->AttachToComponent(Anchor, FAttachmentTransformRules::KeepWorldTransform);
            return true;
        }
    }
    UE_LOG(LogCubeSolver, Warning, TEXT("AttachBridge: socket '%s' not found on %s."), *BridgeSocket.ToString(), *GetNameSafe(Bridge));
    return false;
}
