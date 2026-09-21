#include "CubeSolver.h"

bool UCubeSolver::SolveManifest(FCubeManifest& Manifest)
{
    if (Manifest.StartRoomID == -1 || Manifest.ExitRoomID == -1) return false;

    TArray<FCubeState> Queue;
    TSet<FCubeState> Visited;
    TMap<FCubeState, FCubeState> ParentMap;

    FCubeState StartState(Manifest.StartRoomID, 0, 0, 0);
    
    Queue.Add(StartState);
    Visited.Add(StartState);

    bool bFoundExit = false;
    FCubeState FinalState;

    // Algorithme BFS (Breadth-First Search) : Garantie du chemin le plus court
    while (Queue.Num() > 0)
    {
        FCubeState CurrentState = Queue[0];
        Queue.RemoveAt(0);

        if (CurrentState.RoomID == Manifest.ExitRoomID)
        {
            bFoundExit = true;
            FinalState = CurrentState;
            break;
        }

        TArray<ECubeDirection> Transitions = GetValidTransitions(CurrentState, Manifest);

        for (ECubeDirection Dir : Transitions)
        {
            int32 NextRoomID = GetAdjacentRoomID(CurrentState.RoomID, Dir, Manifest.GridSize);
            if (NextRoomID == -1 || !Manifest.Rooms.Contains(NextRoomID)) continue;

            // La transition de base ne modifie que la position.
            // (Extensible pour inclure les rotations et inventory plus tard)
            FCubeState NextState(NextRoomID, CurrentState.Orientation, CurrentState.Inventory, CurrentState.Flags);

            if (!Visited.Contains(NextState))
            {
                Visited.Add(NextState);
                ParentMap.Add(NextState, CurrentState);
                Queue.Add(NextState);
            }
        }
    }

    if (bFoundExit)
    {
        int32 PathLength = 0;
        FCubeState Trace = FinalState;
        while (Trace != StartState && ParentMap.Contains(Trace))
        {
            PathLength++;
            Trace = ParentMap[Trace];
        }
        Manifest.SolutionLength = PathLength;
        Manifest.bIsValid = true;
        return true;
    }

    Manifest.bIsValid = false;
    return false;
}

TArray<ECubeDirection> UCubeSolver::GetValidTransitions(const FCubeState& CurrentState, const FCubeManifest& Manifest) const
{
    TArray<ECubeDirection> ValidDirs;
    if (const FCubeLogicalRoom* Room = Manifest.Rooms.Find(CurrentState.RoomID))
    {
        for (const auto& DoorPair : Room->Doors)
        {
            if (DoorPair.Value)
            {
                ValidDirs.Add(DoorPair.Key);
            }
        }
    }
    return ValidDirs;
}

int32 UCubeSolver::GetAdjacentRoomID(int32 CurrentRoomID, ECubeDirection Direction, int32 GridSize) const
{
    FCubeCoordinate Coord = FCubeMath::DecodeCoord(CurrentRoomID, GridSize, GridSize);
    
    switch (Direction)
    {
        case ECubeDirection::East:   Coord.X += 1; break;
        case ECubeDirection::West:   Coord.X -= 1; break;
        case ECubeDirection::North:  Coord.Y += 1; break;
        case ECubeDirection::South:  Coord.Y -= 1; break;
        case ECubeDirection::Top:    Coord.Z += 1; break;
        case ECubeDirection::Bottom: Coord.Z -= 1; break;
        default: return -1;
    }

    // Vérification des limites absolues
    if (Coord.X < 0 || Coord.X >= GridSize ||
        Coord.Y < 0 || Coord.Y >= GridSize ||
        Coord.Z < 0 || Coord.Z >= GridSize)
    {
        return -1;
    }

    return FCubeMath::EncodeCoord(Coord, GridSize, GridSize);
}
