#include "CubeSolver.h"
#include "Algo/Reverse.h"

DEFINE_LOG_CATEGORY(LogCubeSolver);

namespace
{
    // Index d'état local : Inventory | Flags << 3 | Orientation << 6 (9 bits).
    constexpr int32 FlagShift  = 3;
    constexpr int32 OriShift   = 6;
    constexpr int32 StateSlots = 512;

    // Voisins dans l'ordre de ECubeDirection (East, West, North, South, Top, Bottom).
    constexpr int32 DirX[6] = { 1, -1, 0,  0, 0,  0 };
    constexpr int32 DirY[6] = { 0,  0, 1, -1, 0,  0 };
    constexpr int32 DirZ[6] = { 0,  0, 0,  0, 1, -1 };

    struct FNode
    {
        int32 Room;
        uint16 Local;      // Inventory | Flags << 3 | Orientation << 6
        int32 Parent;      // index du noeud parent dans la file, -1 pour le départ
    };
}

void UCubeSolver::BuildCellsFromRooms(FCubeManifest& M)
{
    const int32 N = M.GridSize;
    M.Cells.Init(static_cast<uint8>(ECubeCellType::Wall), N * N * N);
    for (const TPair<int32, FCubeLogicalRoom>& Pair : M.Rooms)
    {
        const FCubeLogicalRoom& R = Pair.Value;
        ECubeCellType Type = ECubeCellType::Floor;
        if (R.GateBit != 0)     { Type = ECubeCellType::Gate; }
        else if (R.KeyBit != 0) { Type = ECubeCellType::Key; }
        M.Cells[Pair.Key] = static_cast<uint8>(Type);
    }
}

bool UCubeSolver::SolveManifest(FCubeManifest& Manifest)
{
    return Solve(Manifest);
}

bool UCubeSolver::Solve(FCubeManifest& M)
{
    M.SolutionPath.Reset();
    M.CriticalPath.Reset();
    M.SolutionLength = -1;
    M.NumStates = 0;

    const int32 N = M.GridSize;
    if (N <= 0 || M.StartRoomID < 0 || M.ExitRoomID < 0)
    {
        M.bIsValid = false;
        return false;
    }
    if (!M.HasCells())
    {
        BuildCellsFromRooms(M);
    }
    const int32 NumRooms = M.NumRooms();
    if (M.StartRoomID >= NumRooms || M.ExitRoomID >= NumRooms || !M.IsOpen(M.StartRoomID))
    {
        M.bIsValid = false;
        return false;
    }

    // --- Topologie : seules les salles ouvertes sont indexées (quelques centaines en 64^3) ---------
    // Indexer les N^3 salles coûtait ~95 % du temps de génération pour aucun gain.
    TArray<int32> OpenIndex;
    OpenIndex.Init(INDEX_NONE, NumRooms);
    TArray<int32> OpenRooms;
    for (int32 Rid = 0; Rid < NumRooms; ++Rid)
    {
        if (M.IsOpen(Rid))
        {
            OpenIndex[Rid] = OpenRooms.Add(Rid);
        }
    }
    const int32 NumOpen = OpenRooms.Num();

    TArray<int32> Neighbors;                                   // NumOpen x 6, INDEX_NONE = mur / hors grille
    Neighbors.Init(INDEX_NONE, NumOpen * 6);
    for (int32 O = 0; O < NumOpen; ++O)
    {
        const FCubeCoordinate P = FCubeMath::DecodeCoord(OpenRooms[O], N, N);
        for (int32 D = 0; D < 6; ++D)
        {
            const FCubeCoordinate Q(P.X + DirX[D], P.Y + DirY[D], P.Z + DirZ[D]);
            if (FCubeMath::IsInGrid(Q, N))                     // test de bord : Manhattan = 1 par construction
            {
                const int32 Nid = FCubeMath::EncodeCoord(Q, N, N);
                if (M.IsOpen(Nid))
                {
                    Neighbors[O * 6 + D] = Nid;
                }
            }
        }
    }

    // Bits de clé / porte par salle ouverte (au plus 3 clés).
    TArray<uint8> KeyBit;
    TArray<uint8> GateBit;
    KeyBit.Init(0, NumOpen);
    GateBit.Init(0, NumOpen);
    for (const TPair<int32, FCubeLogicalRoom>& Pair : M.Rooms)
    {
        const int32 O = (Pair.Key >= 0 && Pair.Key < NumRooms) ? OpenIndex[Pair.Key] : INDEX_NONE;
        if (O != INDEX_NONE)
        {
            KeyBit[O]  = static_cast<uint8>(Pair.Value.KeyBit & 7);
            GateBit[O] = static_cast<uint8>(Pair.Value.GateBit & 7);
        }
    }

    // --- BFS : la file sert aussi de table des parents ----------------------------------------------
    TArray<uint8> Visited;
    Visited.Init(0, NumOpen * StateSlots);
    TArray<FNode> Nodes;
    Nodes.Reserve(NumOpen * 2);
    Nodes.Add({ M.StartRoomID, 0, INDEX_NONE });
    Visited[OpenIndex[M.StartRoomID] * StateSlots] = 1;

    int32 Final = INDEX_NONE;
    for (int32 Head = 0; Head < Nodes.Num(); ++Head)
    {
        const FNode Cur = Nodes[Head];
        if (Cur.Room == M.ExitRoomID)
        {
            Final = Head;                                      // premier atteint = optimal
            break;
        }
        const uint32 Inv = Cur.Local & 7u;
        const uint32 Flags = (static_cast<uint32>(Cur.Local) >> FlagShift) & 7u;
        const uint32 Ori = Cur.Local & (7u << OriShift);       // orientation inchangée
        const int32 Base = OpenIndex[Cur.Room] * 6;

        for (int32 D = 0; D < 6; ++D)
        {
            const int32 Nid = Neighbors[Base + D];
            if (Nid == INDEX_NONE)
            {
                continue;
            }
            const int32 On = OpenIndex[Nid];
            uint32 Ni = Inv;
            uint32 Nf = Flags;
            const ECubeCellType Type = M.GetCellType(Nid);
            if (Type == ECubeCellType::Gate)
            {
                if ((Inv & GateBit[On]) == 0u)
                {
                    continue;                                  // porte verrouillée
                }
                Nf |= GateBit[On];
            }
            else if (Type == ECubeCellType::Key)
            {
                Ni |= KeyBit[On];
            }
            const uint16 Local = static_cast<uint16>(Ni | (Nf << FlagShift) | Ori);
            uint8& Seen = Visited[On * StateSlots + Local];
            if (!Seen)
            {
                Seen = 1;
                Nodes.Add({ Nid, Local, Head });
            }
        }
    }

    M.NumStates = Nodes.Num();
    if (Final == INDEX_NONE)
    {
        M.bIsValid = false;
        return false;
    }

    for (int32 I = Final; I != INDEX_NONE; I = Nodes[I].Parent)
    {
        const FNode& Node = Nodes[I];
        M.SolutionPath.Add(FCubeState(Node.Room, (Node.Local >> OriShift) & 7, Node.Local & 7, (Node.Local >> FlagShift) & 7));
    }
    Algo::Reverse(M.SolutionPath);
    M.CriticalPath.Reserve(M.SolutionPath.Num());
    for (const FCubeState& S : M.SolutionPath)
    {
        M.CriticalPath.Add(S.RoomID);
    }
    M.SolutionLength = M.SolutionPath.Num() - 1;
    M.bIsValid = true;
    return true;
}

TArray<ECubeDirection> UCubeSolver::GetValidTransitions(const FCubeState& CurrentState, const FCubeManifest& M)
{
    TArray<ECubeDirection> ValidDirs;
    if (!M.HasCells())
    {
        return ValidDirs;
    }
    for (int32 D = 1; D <= 6; ++D)
    {
        const ECubeDirection Dir = static_cast<ECubeDirection>(D);
        const int32 Nid = GetAdjacentRoomID(CurrentState.RoomID, Dir, M.GridSize);
        if (Nid < 0 || !M.IsOpen(Nid))
        {
            continue;
        }
        if (M.GetCellType(Nid) == ECubeCellType::Gate)
        {
            const FCubeLogicalRoom* Gate = M.Rooms.Find(Nid);
            if (!Gate || (CurrentState.Inventory & Gate->GateBit) == 0)
            {
                continue;
            }
        }
        ValidDirs.Add(Dir);
    }
    return ValidDirs;
}

int32 UCubeSolver::GetAdjacentRoomID(int32 CurrentRoomID, ECubeDirection Direction, int32 GridSize)
{
    if (Direction == ECubeDirection::None || GridSize <= 0)
    {
        return -1;
    }
    const FCubeCoordinate Coord = FCubeMath::DecodeCoord(CurrentRoomID, GridSize, GridSize) + FCubeMath::DirectionDelta(Direction);
    return FCubeMath::IsInGrid(Coord, GridSize) ? FCubeMath::EncodeCoord(Coord, GridSize, GridSize) : -1;
}
