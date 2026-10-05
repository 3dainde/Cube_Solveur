#include "CubeGenerator.h"
#include "CubeSolver.h"
#include "CubeHash.h"
#include "CubeRandom.h"

namespace
{
    // Pas du DFS sur le réseau des noeuds pairs, dans l'ordre de ECubeDirection (East..Bottom).
    constexpr int32 StepX[6] = { 2, -2, 0,  0, 0,  0 };
    constexpr int32 StepY[6] = { 0,  0, 2, -2, 0,  0 };
    constexpr int32 StepZ[6] = { 0,  0, 0,  0, 2, -2 };

    struct FCarvedPath
    {
        TArray<FCubeCoordinate> Full;       // salles ouvertes, dans l'ordre du chemin
        TArray<FCubeCoordinate> Interior;   // noeuds pairs A..T (pile du DFS)
        FCubeCoordinate Start;
        FCubeCoordinate Exit;
    };

    /**
     * Chemin auto-évitant unique : DFS avec retour arrière, pas de 2, biaisé vers la sortie (poids Toward)
     * et faiblement vers les puits (poids Verticality). Seuls les murs franchis sont ouverts : deux salles
     * ouvertes ne sont voisines que si elles se suivent dans le chemin (aucun raccourci, aucun cul-de-sac).
     * Ordre des tirages contractuel (parité) : Ey, Ez, puis un tirage pondéré par pas.
     */
    bool CarvePath(int32 N, FCubeRandom& Rng, double Verticality, double Toward, FCarvedPath& Out)
    {
        const int32 Lo = 2;
        const int32 Hi = (N - 2) & ~1;                          // borne paire intérieure (force pair)
        if (Hi < Lo)
        {
            return false;
        }
        const int32 NumEven = (Hi - Lo) / 2 + 1;
        const int32 Sc = (N / 2) & ~1;

        const FCubeCoordinate A(2, Sc, Sc);                     // 1er noeud, derrière l'entrée
        const int32 Ey = Lo + 2 * static_cast<int32>(Rng.BoundedUInt32(static_cast<uint32>(NumEven - 1)));
        const int32 Ez = Lo + 2 * static_cast<int32>(Rng.BoundedUInt32(static_cast<uint32>(NumEven - 1)));
        const FCubeCoordinate T(Hi, Ey, Ez);                    // dernier noeud, devant la sortie

        const double BaseWeight[6] = { 1.0, 1.0, 1.0, 1.0, Verticality, Verticality };

        auto DistToTarget = [&T](const FCubeCoordinate& P)
        {
            return FMath::Abs(P.X - T.X) + FMath::Abs(P.Y - T.Y) + FMath::Abs(P.Z - T.Z);
        };
        auto NodeIndex = [&](const FCubeCoordinate& P)
        {
            return (P.X - Lo) / 2 + ((P.Y - Lo) / 2) * NumEven + ((P.Z - Lo) / 2) * NumEven * NumEven;
        };

        TArray<uint8> Visited;
        Visited.Init(0, NumEven * NumEven * NumEven);
        TArray<FCubeCoordinate> Stack;
        Stack.Reserve(NumEven * NumEven * NumEven);
        Stack.Add(A);
        Visited[NodeIndex(A)] = 1;
        bool bFound = (A == T);

        while (Stack.Num() > 0 && !bFound)
        {
            const FCubeCoordinate Cur = Stack.Last();
            const int32 D0 = DistToTarget(Cur);

            FCubeCoordinate Cand[6];
            double Weights[6];
            int32 NumCand = 0;
            for (int32 D = 0; D < 6; ++D)
            {
                const FCubeCoordinate P(Cur.X + StepX[D], Cur.Y + StepY[D], Cur.Z + StepZ[D]);
                if (P.X >= Lo && P.X <= Hi && P.Y >= Lo && P.Y <= Hi && P.Z >= Lo && P.Z <= Hi && !Visited[NodeIndex(P)])
                {
                    Cand[NumCand] = P;
                    Weights[NumCand] = BaseWeight[D] * (DistToTarget(P) < D0 ? Toward : 1.0);
                    ++NumCand;
                }
            }

            if (NumCand > 0)
            {
                const FCubeCoordinate Next = Cand[Rng.WeightedIndex(Weights, NumCand)];
                Visited[NodeIndex(Next)] = 1;
                Stack.Add(Next);
                bFound = (Next == T);
            }
            else
            {
                Stack.Pop();
            }
        }
        if (!bFound)
        {
            return false;
        }

        Out.Interior = MoveTemp(Stack);                         // pile du DFS = chemin simple A..T
        Out.Start = FCubeCoordinate(0, Sc, Sc);
        Out.Exit = FCubeCoordinate(N - 1, Ey, Ez);

        TArray<FCubeCoordinate>& Full = Out.Full;
        Full.Reset();
        Full.Reserve(Out.Interior.Num() * 2 + N);
        Full.Add(Out.Start);                                    // entrée sur la face West
        Full.Add(FCubeCoordinate(1, Sc, Sc));                   // connecteur
        for (int32 I = 0; I < Out.Interior.Num(); ++I)
        {
            const FCubeCoordinate& Node = Out.Interior[I];
            Full.Add(Node);
            if (I + 1 < Out.Interior.Num())
            {
                const FCubeCoordinate& Q = Out.Interior[I + 1];
                Full.Add(FCubeCoordinate((Node.X + Q.X) / 2, (Node.Y + Q.Y) / 2, (Node.Z + Q.Z) / 2));
            }
        }
        // Jonction de sortie : connecteurs linéaires jusqu'à N-2 (grilles impaires).
        const FCubeCoordinate Last = Out.Interior.Last();
        for (int32 Cx = Last.X + 1; Cx <= N - 2; ++Cx)
        {
            Full.Add(FCubeCoordinate(Cx, Last.Y, Last.Z));
        }
        Full.Add(Out.Exit);                                     // sortie sur la face East
        return true;
    }
}

uint32 UCubeGenerator::GetAttemptRngSeed(const FString& Seed, int32 Attempt)
{
    const uint32 SeedHash = CubeHash::Crc32Str(Seed);
    const uint32 PathHash = CubeHash::Combine(SeedHash, CubeHash::Crc32Ascii("PATH"));
    return CubeHash::Combine(PathHash, static_cast<uint32>(Attempt));
}

bool UCubeGenerator::AreParamsFeasible(int32 GridSize, int32 NKeys)
{
    if (GridSize < 8 || GridSize > 64 || NKeys < 0 || NKeys > 3)
    {
        return false;
    }
    const int32 NumEven = (((GridSize - 2) & ~1) - 2) / 2 + 1;
    return NumEven * NumEven * NumEven >= 4 * NKeys + 2;
}

FCubeManifest UCubeGenerator::GenerateManifestAttempt(const FString& Seed, int32 Attempt, int32 N,
                                                      double PathLen, double Verticality, int32 NKeys)
{
    FCubeManifest Manifest;
    Manifest.Seed = Seed;
    Manifest.GridSize = N;
    Manifest.Attempt = Attempt;
    Manifest.bIsValid = false;

    FCubeRandom Rng(GetAttemptRngSeed(Seed, Attempt));

    // Court et direct (Toward = 12) <-> long et sinueux (Toward = 2). 12^(1-l) est dans [1, 12] :
    // la borne 14 de la R&D n'est jamais atteinte, la borne 2 sature dès l >= 0,721.
    const double Toward = FMath::Clamp(FMath::Pow(12.0, 1.0 - PathLen), 2.0, 14.0);

    FCarvedPath Path;
    if (!CarvePath(N, Rng, Verticality, Toward, Path))
    {
        return Manifest;
    }

    // Grille complète : murs partout, couloir ouvert.
    Manifest.Cells.Init(static_cast<uint8>(ECubeCellType::Wall), N * N * N);
    for (const FCubeCoordinate& C : Path.Full)
    {
        Manifest.Cells[FCubeMath::EncodeCoord(C, N, N)] = static_cast<uint8>(ECubeCellType::Floor);
    }

    // Salles logiques + portes entre cases consécutives.
    auto AddOrGet = [&Manifest, N](const FCubeCoordinate& Coord) -> FCubeLogicalRoom&
    {
        const int32 ID = FCubeMath::EncodeCoord(Coord, N, N);
        if (FCubeLogicalRoom* Existing = Manifest.Rooms.Find(ID))
        {
            return *Existing;
        }
        FCubeLogicalRoom R;
        R.RoomID = ID;
        R.Coordinate = Coord;
        R.bIsCriticalPath = true;
        return Manifest.Rooms.Add(ID, R);
    };

    for (int32 I = 0; I < Path.Full.Num(); ++I)
    {
        AddOrGet(Path.Full[I]);
        if (I + 1 < Path.Full.Num())
        {
            const FCubeCoordinate& A = Path.Full[I];
            const FCubeCoordinate& B = Path.Full[I + 1];
            const ECubeDirection Dir = FCubeMath::DirectionFromDelta(B - A);
            if (Dir != ECubeDirection::None)
            {
                AddOrGet(A).Doors.Add(Dir, true);
                AddOrGet(B).Doors.Add(FCubeMath::Opposite(Dir), true);
            }
        }
    }

    Manifest.StartRoomID = FCubeMath::EncodeCoord(Path.Start, N, N);
    Manifest.ExitRoomID  = FCubeMath::EncodeCoord(Path.Exit, N, N);
    Manifest.Rooms[Manifest.StartRoomID].Archetype = ERoomArchetype::Start;
    Manifest.Rooms[Manifest.ExitRoomID].Archetype  = ERoomArchetype::Exit;

    // Clé j strictement AVANT porte j (4 salles plus loin) : le solveur doit la détenir en arrivant.
    // Tentative rejetée si le chemin est trop court ou si deux paires se chevauchent (comme la R&D).
    const TArray<FCubeCoordinate>& Interior = Path.Interior;
    const int32 L = Interior.Num();
    if (NKeys > 0)
    {
        if (L < 4 * NKeys + 2)
        {
            return Manifest;
        }
        TArray<int32> Used;
        TArray<int32> KeyIdx;
        for (int32 J = 0; J < NKeys; ++J)
        {
            int32 Ki = FCubeMath::RoundHalfToEven(double(J + 1) / double(NKeys + 1) * double(L - 1));
            Ki = FMath::Min(FMath::Max(Ki, 1), L - 4);
            if (Used.Contains(Ki) || Used.Contains(Ki + 2))
            {
                return Manifest;
            }
            Used.Add(Ki);
            Used.Add(Ki + 2);
            KeyIdx.Add(Ki);
        }
        for (int32 J = 0; J < KeyIdx.Num(); ++J)
        {
            const int32 KeyID  = FCubeMath::EncodeCoord(Interior[KeyIdx[J]], N, N);
            const int32 GateID = FCubeMath::EncodeCoord(Interior[KeyIdx[J] + 2], N, N);
            Manifest.Cells[KeyID]  = static_cast<uint8>(ECubeCellType::Key);
            Manifest.Cells[GateID] = static_cast<uint8>(ECubeCellType::Gate);
            FCubeLogicalRoom& KR = Manifest.Rooms[KeyID];
            KR.Archetype = ERoomArchetype::Clue;
            KR.KeyBit = 1 << J;
            FCubeLogicalRoom& GR = Manifest.Rooms[GateID];
            GR.Archetype = ERoomArchetype::Puzzle;
            GR.GateBit = 1 << J;
        }
    }

    Manifest.bIsValid = true;
    return Manifest;
}

FCubeManifest UCubeGenerator::Generate(const FString& Seed, int32 GridSize, double PathLen, double Verticality,
                                       int32 NKeys, int32 MaxAttempts)
{
    const int32 N = FMath::Clamp(GridSize, 8, 64);
    NKeys = FMath::Clamp(NKeys, 0, 3);                          // l'inventaire tient sur 3 bits

    FCubeManifest Manifest;
    Manifest.Seed = Seed;
    Manifest.GridSize = N;
    if (!AreParamsFeasible(N, NKeys))
    {
        UE_LOG(LogCubeSolver, Error, TEXT("Generate: infeasible params (N=%d, NKeys=%d)."), N, NKeys);
        return Manifest;
    }

    const int32 Attempts = MaxAttempts > 0 ? MaxAttempts : UnboundedAttempts;
    for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
    {
        Manifest = GenerateManifestAttempt(Seed, Attempt, N, PathLen, Verticality, NKeys);
        if (Manifest.bIsValid && UCubeSolver::Solve(Manifest))
        {
            return Manifest;                                    // chemin solvable garanti
        }
    }

    UE_LOG(LogCubeSolver, Error, TEXT("Generate: seed '%s' has no playable level after %d sub-seeds."), *Seed, Attempts);
    Manifest.bIsValid = false;
    return Manifest;
}

FCubeManifest UCubeGenerator::GenerateCubeAdvanced(FString Seed, int32 GridSize,
                                                   float PathLen, float Verticality,
                                                   int32 NKeys, int32 MaxAttempts)
{
    // float -> double : 0.45f devient 0.449999988..., exactement la valeur que Blender (FloatProperty
    // 32 bits) passe à la R&D ; les niveaux sont donc identiques à ceux de l'add-on.
    return Generate(Seed, GridSize, static_cast<double>(PathLen), static_cast<double>(Verticality), NKeys, MaxAttempts);
}

FCubeManifest UCubeGenerator::GenerateCube(FString Seed, int32 GridSize)
{
    return Generate(Seed, GridSize, 0.45, 0.18, 2, 0);
}
