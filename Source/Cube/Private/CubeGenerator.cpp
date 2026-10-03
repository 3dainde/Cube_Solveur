#include "CubeGenerator.h"
#include "CubeSolver.h"
#include "CubeHash.h"

// Directions (dx,dy,dz) et leur opposée, ordre = ECubeDirection.
static ECubeDirection StepToDirection(int32 dx, int32 dy, int32 dz)
{
    if (dx > 0) return ECubeDirection::East;
    if (dx < 0) return ECubeDirection::West;
    if (dy > 0) return ECubeDirection::North;
    if (dy < 0) return ECubeDirection::South;
    if (dz > 0) return ECubeDirection::Top;
    if (dz < 0) return ECubeDirection::Bottom;
    return ECubeDirection::None;
}

static ECubeDirection OppositeDirection(ECubeDirection D)
{
    switch (D)
    {
        case ECubeDirection::East:   return ECubeDirection::West;
        case ECubeDirection::West:   return ECubeDirection::East;
        case ECubeDirection::North:  return ECubeDirection::South;
        case ECubeDirection::South:  return ECubeDirection::North;
        case ECubeDirection::Top:    return ECubeDirection::Bottom;
        case ECubeDirection::Bottom: return ECubeDirection::Top;
        default:                     return ECubeDirection::None;
    }
}

bool UCubeGenerator::CarvePath(int32 N, FRandomStream& Stream, float Verticality, float Toward,
                               TArray<FCubeCoordinate>& OutFull, TArray<FCubeCoordinate>& OutInterior,
                               FCubeCoordinate& OutStart, FCubeCoordinate& OutExit) const
{
    const int32 Lo = 2;
    const int32 Hi = (N - 2) & ~1;                 // borne paire intérieure (force pair)
    if (Hi < Lo) return false;

    TArray<int32> Ev;
    for (int32 v = Lo; v <= Hi; v += 2) Ev.Add(v);

    const int32 Sc = (N / 2) & ~1;
    const FCubeCoordinate A(2, Sc, Sc);
    const int32 Ey = Ev[Stream.RandRange(0, Ev.Num() - 1)];
    const int32 Ez = Ev[Stream.RandRange(0, Ev.Num() - 1)];
    const FCubeCoordinate T(Hi, Ey, Ez);

    struct FMove { int32 dx, dy, dz; float w; };
    const FMove Base[6] = {
        { 2, 0, 0, 1.0f}, {-2, 0, 0, 1.0f},
        { 0, 2, 0, 1.0f}, { 0,-2, 0, 1.0f},
        { 0, 0, 2, Verticality}, { 0, 0,-2, Verticality}
    };

    auto Dist = [&T](const FCubeCoordinate& P)
    {
        return FMath::Abs(P.X - T.X) + FMath::Abs(P.Y - T.Y) + FMath::Abs(P.Z - T.Z);
    };

    TSet<FCubeCoordinate> Visited;
    TArray<FCubeCoordinate> Stack;
    Visited.Add(A);
    Stack.Add(A);
    bool bFound = (A == T);

    while (Stack.Num() > 0 && !bFound)
    {
        const FCubeCoordinate Cur = Stack.Last();
        const int32 D0 = Dist(Cur);

        TArray<FCubeCoordinate> Cand;
        TArray<float> Wts;
        float WSum = 0.0f;
        for (const FMove& M : Base)
        {
            const FCubeCoordinate P(Cur.X + M.dx, Cur.Y + M.dy, Cur.Z + M.dz);
            if (P.X >= Lo && P.X <= Hi && P.Y >= Lo && P.Y <= Hi && P.Z >= Lo && P.Z <= Hi
                && !Visited.Contains(P))
            {
                const float W = M.w * ((Dist(P) < D0) ? Toward : 1.0f);
                Cand.Add(P);
                Wts.Add(W);
                WSum += W;
            }
        }

        if (Cand.Num() > 0)
        {
            // Sélection pondérée déterministe (FRandomStream).
            float R = Stream.FRand() * WSum;
            int32 Pick = 0;
            for (; Pick < Cand.Num(); ++Pick)
            {
                R -= Wts[Pick];
                if (R <= 0.0f) break;
            }
            if (Pick >= Cand.Num()) Pick = Cand.Num() - 1;

            const FCubeCoordinate Next = Cand[Pick];
            Visited.Add(Next);
            Stack.Add(Next);
            if (Next == T) bFound = true;
        }
        else
        {
            Stack.Pop();
        }
    }

    if (!bFound) return false;

    OutInterior = Stack;                            // pile DFS = chemin simple A..T

    OutFull.Reset();
    OutFull.Add(FCubeCoordinate(0, Sc, Sc));        // entrée sur la face
    OutFull.Add(FCubeCoordinate(1, Sc, Sc));        // connecteur
    for (int32 i = 0; i < OutInterior.Num(); ++i)
    {
        OutFull.Add(OutInterior[i]);
        if (i + 1 < OutInterior.Num())
        {
            const FCubeCoordinate& Nd = OutInterior[i];
            const FCubeCoordinate& Q  = OutInterior[i + 1];
            OutFull.Add(FCubeCoordinate((Nd.X + Q.X) / 2, (Nd.Y + Q.Y) / 2, (Nd.Z + Q.Z) / 2));
        }
    }

    // Jonction de sortie : connecteurs linéaires même si N est impair.
    FCubeCoordinate C = OutInterior.Last();
    while (C.X < N - 2)
    {
        C.X += 1;
        OutFull.Add(C);
    }
    OutFull.Add(FCubeCoordinate(N - 1, Ey, Ez));    // sortie sur la face opposée

    OutStart = FCubeCoordinate(0, Sc, Sc);
    OutExit  = FCubeCoordinate(N - 1, Ey, Ez);
    return true;
}

FCubeManifest UCubeGenerator::GenerateManifestAttempt(const FString& Seed, int32 Attempt, int32 N,
                                                      float PathLen, float Verticality, int32 NKeys)
{
    FCubeManifest Manifest;
    Manifest.Seed = Seed;
    Manifest.GridSize = N;

    // Sous-seed déterministe : CRC32(seed) x CRC32("PATH") x attempt.
    const uint32 SeedHash = CubeHash::Crc32Str(Seed);
    const uint32 PathHash = CubeHash::Combine(SeedHash, CubeHash::Crc32Str(TEXT("PATH")));
    FRandomStream Stream((int32)CubeHash::Combine(PathHash, (uint32)Attempt));

    const float Toward = FMath::Min(14.0f, FMath::Max(2.0f, FMath::Pow(12.0f, 1.0f - PathLen)));

    TArray<FCubeCoordinate> Full, Interior;
    FCubeCoordinate StartCell, ExitCell;
    if (!CarvePath(N, Stream, Verticality, Toward, Full, Interior, StartCell, ExitCell))
    {
        Manifest.bIsValid = false;
        return Manifest;
    }

    // Construction des salles + portes entre cases consécutives.
    auto AddOrGet = [&Manifest, N](const FCubeCoordinate& Coord) -> FCubeLogicalRoom&
    {
        const int32 ID = FCubeMath::EncodeCoord(Coord, N, N);
        if (!Manifest.Rooms.Contains(ID))
        {
            FCubeLogicalRoom R;
            R.RoomID = ID;
            R.Coordinate = Coord;
            R.bIsCriticalPath = true;
            Manifest.Rooms.Add(ID, R);
        }
        return Manifest.Rooms[ID];
    };

    Manifest.CriticalPath.Reset();
    for (int32 i = 0; i < Full.Num(); ++i)
    {
        FCubeLogicalRoom& Room = AddOrGet(Full[i]);
        Manifest.CriticalPath.Add(Room.RoomID);

        if (i + 1 < Full.Num())
        {
            const FCubeCoordinate& A = Full[i];
            const FCubeCoordinate& B = Full[i + 1];
            const ECubeDirection Dir = StepToDirection(B.X - A.X, B.Y - A.Y, B.Z - A.Z);
            if (Dir != ECubeDirection::None)
            {
                AddOrGet(A).Doors.Add(Dir, true);
                AddOrGet(B).Doors.Add(OppositeDirection(Dir), true);
            }
        }
    }

    const int32 StartID = FCubeMath::EncodeCoord(StartCell, N, N);
    const int32 ExitID  = FCubeMath::EncodeCoord(ExitCell, N, N);
    Manifest.StartRoomID = StartID;
    Manifest.ExitRoomID  = ExitID;
    Manifest.Rooms[StartID].Archetype = ERoomArchetype::Start;
    Manifest.Rooms[ExitID].Archetype  = ERoomArchetype::Exit;

    // Clés et portes le long du chemin (clé j strictement AVANT porte j).
    if (NKeys > 0 && Interior.Num() >= 4 * NKeys + 2)
    {
        TSet<int32> Used;
        bool bOk = true;
        for (int32 j = 0; j < NKeys && bOk; ++j)
        {
            int32 Ki = FMath::RoundToInt((float)(j + 1) / (NKeys + 1) * (Interior.Num() - 1));
            Ki = FMath::Clamp(Ki, 1, Interior.Num() - 4);
            if (Used.Contains(Ki) || Used.Contains(Ki + 2)) { bOk = false; break; }
            Used.Add(Ki); Used.Add(Ki + 2);

            const int32 KeyID  = FCubeMath::EncodeCoord(Interior[Ki],     N, N);
            const int32 GateID = FCubeMath::EncodeCoord(Interior[Ki + 2], N, N);
            if (FCubeLogicalRoom* KR = Manifest.Rooms.Find(KeyID))
            {
                KR->Archetype = ERoomArchetype::Clue; KR->KeyBit = 1 << j;
            }
            if (FCubeLogicalRoom* GR = Manifest.Rooms.Find(GateID))
            {
                GR->Archetype = ERoomArchetype::Puzzle; GR->GateBit = 1 << j;
            }
        }
    }

    Manifest.bIsValid = true;
    return Manifest;
}

FCubeManifest UCubeGenerator::GenerateCubeAdvanced(FString Seed, int32 GridSize,
                                                   float PathLen, float Verticality,
                                                   int32 NKeys, int32 MaxAttempts)
{
    GridSize = FMath::Clamp(GridSize, 10, 64);
    UCubeSolver* Solver = NewObject<UCubeSolver>(this);

    FCubeManifest Manifest;
    for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxAttempts); ++Attempt)
    {
        Manifest = GenerateManifestAttempt(Seed, Attempt, GridSize, PathLen, Verticality, NKeys);
        if (Manifest.bIsValid && Solver->SolveManifest(Manifest))
        {
            return Manifest;   // Succès garanti : chemin solvable.
        }
    }

    // Aucune tentative solvable (extrêmement improbable) : renvoie la dernière, invalidée.
    Manifest.bIsValid = false;
    return Manifest;
}

FCubeManifest UCubeGenerator::GenerateCube(FString Seed, int32 GridSize)
{
    return GenerateCubeAdvanced(Seed, GridSize, 0.45f, 0.18f, 2, 64);
}
