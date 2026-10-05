#include "CubeRoomStreamer.h"
#include "CubeRoom.h"
#include "GM_CubeGameMode.h"
#include "CubeRebusTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

void UCubeRoomStreamer::Configure(TSubclassOf<ACubeRoom> InRoomClass, int32 InRadius)
{
    RoomClass = InRoomClass;
    Radius = FMath::Clamp(InRadius, 1, 12);
    bConfigured = (RoomClass != nullptr);
    bDirty = true;
}

void UCubeRoomStreamer::SetFocusCell(FCubeCoordinate NewFocus)
{
    if (NewFocus != Focus)
    {
        Focus = NewFocus;
        bDirty = true;
    }
}

bool UCubeRoomStreamer::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCubeRoomStreamer::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UCubeRoomStreamer, STATGROUP_Tickables);
}

void UCubeRoomStreamer::Tick(float DeltaTime)
{
    if (!bConfigured || !bDirty) return;
    if (Focus == LastFocus) { bDirty = false; return; }

    RefreshStreaming();
    LastFocus = Focus;
    bDirty = false;
}

ACubeRoom* UCubeRoomStreamer::AcquireRoom()
{
    if (Pool.Num() > 0)
    {
        return Pool.Pop();
    }
    UWorld* World = GetWorld();
    if (!World || !RoomClass) return nullptr;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<ACubeRoom>(RoomClass, FTransform::Identity, Params);
}

void UCubeRoomStreamer::RefreshStreaming()
{
    UWorld* World = GetWorld();
    if (!World) return;

    AGM_CubeGameMode* GM = World->GetAuthGameMode<AGM_CubeGameMode>();
    if (!GM) return;   // Vérité disponible côté serveur/local uniquement.

    const FCubeManifest& Manifest = GM->GetManifest();
    const int32 N = Manifest.GridSize;
    if (N <= 0) return;
    const float RoomSize = GM->RoomSize; // Aligné sur la géométrie du GameMode.

    // 1. Ensemble désiré : cube de rayon Radius autour du focus, borné à la grille.
    TSet<int32> Desired;
    for (int32 dz = -Radius; dz <= Radius; ++dz)
    for (int32 dy = -Radius; dy <= Radius; ++dy)
    for (int32 dx = -Radius; dx <= Radius; ++dx)
    {
        const FCubeCoordinate C(Focus.X + dx, Focus.Y + dy, Focus.Z + dz);
        if (C.X < 0 || C.X >= N || C.Y < 0 || C.Y >= N || C.Z < 0 || C.Z >= N) continue;
        Desired.Add(FCubeMath::EncodeCoord(C, N, N));
    }

    // 2. Recyclage des salles hors zone.
    TArray<int32> ToRelease;
    for (const TPair<int32, TObjectPtr<ACubeRoom>>& Pair : ActiveRooms)
    {
        if (!Desired.Contains(Pair.Key))
        {
            ToRelease.Add(Pair.Key);
        }
    }
    for (int32 ID : ToRelease)
    {
        if (ACubeRoom* Room = ActiveRooms.FindRef(ID))
        {
            Room->ReturnToPool();
            Pool.Add(Room);
        }
        ActiveRooms.Remove(ID);
    }

    // 3. Activation des nouvelles salles.
    for (int32 ID : Desired)
    {
        if (ActiveRooms.Contains(ID)) continue;

        const FCubeCoordinate Cell = FCubeMath::DecodeCoord(ID, N, N);
        ACubeRoom* Room = AcquireRoom();
        if (!Room) continue;

        FCubeRoomData RData;
        RData.RoomID = ID;
        RData.Cell = Cell;
        RData.State = GM->GetCellState(Cell);
        if (const FCubeLogicalRoom* LR = Manifest.Rooms.Find(ID))
        {
            RData.PathIndex = LR->bIsCriticalPath ? 0 : -1; // Rang précis fourni par le GameMode si besoin.
        }
        Room->Setup(RData, N, RoomSize);
        ActiveRooms.Add(ID, Room);
    }
}
