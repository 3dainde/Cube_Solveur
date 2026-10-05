#include "CubeRebusLibrary.h"
#include "CubeHash.h"

static FORCEINLINE int32 PosMod(int32 A, int32 M) { return ((A % M) + M) % M; }

// Ordre canonique des 6 voisins : East, West, North, South, Top, Bottom (index 0..5).
// Ordre partagé par ComputeSafeShortcuts, ComputeBridgingCells et IndexToDirection.
static const int32 GNeighborDelta[6][3] =
{
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1}
};

int32 UCubeRebusLibrary::DirectionToIndex(ECubeDirection Dir)
{
    // ECubeDirection : None=0, East=1 ... Bottom=6  ->  index sénaire 0..5.
    return static_cast<int32>(Dir) - 1;
}

ECubeDirection UCubeRebusLibrary::IndexToDirection(int32 Index)
{
    if (Index < 0 || Index > 5) return ECubeDirection::None;
    return static_cast<ECubeDirection>(Index + 1);
}

TArray<FCubeRouteToken> UCubeRebusLibrary::RouteToRLE(const TArray<int32>& PathRoomIDs, int32 GridSize)
{
    TArray<FCubeRouteToken> Tokens;
    if (PathRoomIDs.Num() < 2 || GridSize <= 0) return Tokens;

    auto StepDir = [GridSize](int32 FromID, int32 ToID) -> ECubeDirection
    {
        const FCubeCoordinate A = FCubeMath::DecodeCoord(FromID, GridSize, GridSize);
        const FCubeCoordinate B = FCubeMath::DecodeCoord(ToID, GridSize, GridSize);
        const int32 dx = B.X - A.X, dy = B.Y - A.Y, dz = B.Z - A.Z;
        if (dx ==  1 && dy == 0 && dz == 0) return ECubeDirection::East;
        if (dx == -1 && dy == 0 && dz == 0) return ECubeDirection::West;
        if (dx == 0 && dy ==  1 && dz == 0) return ECubeDirection::North;
        if (dx == 0 && dy == -1 && dz == 0) return ECubeDirection::South;
        if (dx == 0 && dy == 0 && dz ==  1) return ECubeDirection::Top;
        if (dx == 0 && dy == 0 && dz == -1) return ECubeDirection::Bottom;
        return ECubeDirection::None;
    };

    ECubeDirection CurDir = StepDir(PathRoomIDs[0], PathRoomIDs[1]);
    int32 Run = 0;
    for (int32 i = 1; i < PathRoomIDs.Num(); ++i)
    {
        const ECubeDirection D = StepDir(PathRoomIDs[i - 1], PathRoomIDs[i]);
        if (D == CurDir)
        {
            ++Run;
        }
        else
        {
            if (Run > 0) { FCubeRouteToken T; T.Dir = CurDir; T.Run = Run; Tokens.Add(T); }
            CurDir = D;
            Run = 1;
        }
    }
    if (Run > 0) { FCubeRouteToken T; T.Dir = CurDir; T.Run = Run; Tokens.Add(T); }
    return Tokens;
}

uint32 UCubeRebusLibrary::GetTrapSeedHash(const FString& Seed)
{
    return CubeHash::Combine(CubeHash::Crc32Str(Seed), CubeHash::Crc32Ascii("TRAP"));
}

int32 UCubeRebusLibrary::GetRhoLimit(double Rho)
{
    // Troncature en double, comme int(rho * 10000.0) en Python. En float, 0.35f * 10000 donnerait 3500
    // au lieu de 3499 et décalerait le champ mortel d'une cellule sur 10 000.
    return static_cast<int32>(FMath::Max(Rho, 0.0) * 10000.0);
}

bool UCubeRebusLibrary::IsLethalFast(uint32 TrapSeedHash, int32 X, int32 Y, int32 Z, int32 RhoLimit)
{
    // Produits en uint32 (repli modulo 2^32). En int32, 63 * 83492791 déborde : comportement indéfini en C++.
    const uint32 B = (static_cast<uint32>(X) * 73856093u) ^ (static_cast<uint32>(Y) * 19349663u)
                   ^ (static_cast<uint32>(Z) * 83492791u);
    return (CubeHash::Combine(TrapSeedHash, B) % 10000u) < static_cast<uint32>(RhoLimit);
}

bool UCubeRebusLibrary::IsLethal(const FString& Seed, int32 X, int32 Y, int32 Z,
                                 float Rho, const TSet<int32>& SafeSet, int32 GridSize)
{
    const int32 RoomID = FCubeMath::EncodeCoord(FCubeCoordinate(X, Y, Z), GridSize, GridSize);
    if (SafeSet.Contains(RoomID)) return false; // Le chemin du solveur reste toujours sain.
    return IsLethalFast(GetTrapSeedHash(Seed), X, Y, Z, GetRhoLimit(static_cast<double>(Rho)));
}

bool UCubeRebusLibrary::IsLethalCell(const FString& Seed, FCubeCoordinate Cell,
                                     float Rho, int32 GridSize, const TSet<int32>& SafeSet)
{
    return IsLethal(Seed, Cell.X, Cell.Y, Cell.Z, Rho, SafeSet, GridSize);
}

TSet<int32> UCubeRebusLibrary::ComputeSafeShortcuts(const FString& Seed, const TArray<int32>& PathRoomIDs,
                                                    int32 GridSize, float Rho, int32 Reach)
{
    TSet<int32> Out;
    const int32 N = GridSize;
    if (PathRoomIDs.Num() == 0 || Rho <= 0.0f || Reach <= 0 || N <= 0) return Out;

    const int32 N2 = N * N;
    const int32 N3 = N2 * N;
    const uint32 TrapHash = GetTrapSeedHash(Seed);
    const int32 RhoLimit = GetRhoLimit(static_cast<double>(Rho));

    // Première étape où chaque salle du chemin est atteinte (INDEX_NONE hors chemin).
    TArray<int32> FirstIdx;
    FirstIdx.Init(INDEX_NONE, N3);
    for (int32 i = 0; i < PathRoomIDs.Num(); ++i)
    {
        const int32 ID = PathRoomIDs[i];
        if (ID >= 0 && ID < N3 && FirstIdx[ID] == INDEX_NONE) FirstIdx[ID] = i;
    }

    auto IsSafe = [&](int32 C)
    {
        const int32 Rest = C % N2;
        return !IsLethalFast(TrapHash, Rest % N, Rest / N, C / N2, RhoLimit);
    };

    // Voisins dans l'ordre -X, +X, -Y, +Y, -Z, +Z (contractuel : fixe les parents du BFS, parité R&D).
    auto ForEachNeighbor = [N, N2](int32 C, auto&& Fn)
    {
        const int32 Rest = C % N2;
        const int32 X = Rest % N;
        const int32 Y = Rest / N;
        const int32 Z = C / N2;
        if (X > 0)     { Fn(C - 1); }
        if (X < N - 1) { Fn(C + 1); }
        if (Y > 0)     { Fn(C - N); }
        if (Y < N - 1) { Fn(C + N); }
        if (Z > 0)     { Fn(C - N2); }
        if (Z < N - 1) { Fn(C + N2); }
    };

    TArray<int32> Dist;      // 0 = non atteint
    TArray<int32> Src;       // étape du chemin dont part le front
    TArray<int32> Parent;
    TArray<uint8> Marked;
    Dist.Init(0, N3);
    Src.Init(0, N3);
    Parent.Init(INDEX_NONE, N3);
    Marked.Init(0, N3);

    auto Mark = [&](int32 C)
    {
        while (C != INDEX_NONE && !Marked[C])
        {
            Marked[C] = 1;
            C = Parent[C];
        }
    };

    // Graines : salles sûres voisines du chemin (distance 1), salles du chemin parcourues par RoomID croissant.
    TArray<int32> Queue;
    Queue.Reserve(4096);
    for (int32 P = 0; P < N3; ++P)
    {
        const int32 FP = FirstIdx[P];
        if (FP == INDEX_NONE) continue;
        ForEachNeighbor(P, [&](int32 C)
        {
            if (FirstIdx[C] != INDEX_NONE || !IsSafe(C)) return;
            if (Dist[C] == 0)
            {
                Dist[C] = 1;
                Src[C] = FP;
                Queue.Add(C);
            }
            else
            {
                if (FMath::Abs(FP - Src[C]) > 2) Marked[C] = 1;   // une seule salle relie deux étapes éloignées
                if (FP < Src[C]) Src[C] = FP;
            }
        });
    }

    // Fronts : deux fronts d'étapes éloignées qui se touchent (ou un retour sur le chemin) = détour, retenu s'il gagne des pas.
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const int32 C = Queue[Head];
        const int32 Dc = Dist[C];
        const int32 Sc = Src[C];
        ForEachNeighbor(C, [&](int32 D)
        {
            const int32 F = FirstIdx[D];
            if (F != INDEX_NONE)
            {
                if (Dc + 1 < FMath::Abs(F - Sc)) Mark(C);
                return;
            }
            if (Dist[D] != 0)
            {
                if (Dc + Dist[D] + 1 < FMath::Abs(Src[D] - Sc))
                {
                    Mark(C);
                    Mark(D);
                }
            }
            else if (Dc < Reach && IsSafe(D))
            {
                Dist[D] = Dc + 1;
                Src[D] = Sc;
                Parent[D] = C;
                Queue.Add(D);
            }
        });
    }

    for (int32 C = 0; C < N3; ++C)
    {
        if (Marked[C]) Out.Add(C);
    }
    return Out;
}

FCubeFormula UCubeRebusLibrary::MakeFormula(const FString& Seed, int32 PathIndex,
                                            FCubeCoordinate Pos, int32 SafeDirIndex,
                                            int32 Tier, bool bEnableDecoy)
{
    const uint32 Hs = CubeHash::Combine(CubeHash::Crc32Str(Seed), (uint32)PathIndex);
    FCubeFormula F;
    F.Tier = Tier;
    F.Mod  = 6;
    F.A1 = 1 + (Hs % 3);
    F.A2 = 1 + ((Hs >> 3) % 3);
    F.A3 = 1 + ((Hs >> 6) % 3);

    const int32 Base = F.A1 * Pos.X + F.A2 * Pos.Y + F.A3 * Pos.Z;
    F.A4 = PosMod(SafeDirIndex - Base, 6);
    F.Answer = SafeDirIndex;
    F.bEnableDecoy = bEnableDecoy;
    return F;
}

int32 UCubeRebusLibrary::EvaluateFormula(const FCubeFormula& F, FCubeCoordinate Pos)
{
    return PosMod(F.A1 * Pos.X + F.A2 * Pos.Y + F.A3 * Pos.Z + F.A4, F.Mod);
}

// ============================================================================
// Chemin unique strict
// ============================================================================

TSet<int32> UCubeRebusLibrary::ComputeBridgingCells(const FString& Seed, const TArray<int32>& PathRoomIDs,
                                                    int32 GridSize, float Rho)
{
    TSet<int32> Bridging;
    const int32 N = GridSize;
    if (N <= 0 || PathRoomIDs.Num() < 3) return Bridging;

    const int32 Total = N * N * N;

    // Rang de chaque cellule sur le chemin (INDEX_NONE hors chemin).
    TArray<int32> PathIndex;
    PathIndex.Init(INDEX_NONE, Total);
    for (int32 i = 0; i < PathRoomIDs.Num(); ++i)
    {
        if (PathIndex[PathRoomIDs[i]] == INDEX_NONE)
            PathIndex[PathRoomIDs[i]] = i;
    }

    // Cellules praticables hors chemin.
    TArray<uint8> Open;
    Open.Init(0, Total);
    const uint32 TrapHash = GetTrapSeedHash(Seed);
    const int32 RhoLimit = GetRhoLimit(static_cast<double>(Rho));
    for (int32 Z = 0; Z < N; ++Z)
    for (int32 Y = 0; Y < N; ++Y)
    for (int32 X = 0; X < N; ++X)
    {
        const int32 ID = FCubeMath::EncodeCoord(FCubeCoordinate(X, Y, Z), N, N);
        if (PathIndex[ID] == INDEX_NONE && !IsLethalFast(TrapHash, X, Y, Z, RhoLimit))
            Open[ID] = 1;
    }

    // Composantes connexes : une composante qui saute au moins une case du chemin est interdite.
    TArray<uint8> Seen;
    Seen.Init(0, Total);
    TArray<int32> Component;

    for (int32 StartID = 0; StartID < Total; ++StartID)
    {
        if (!Open[StartID] || Seen[StartID]) continue;

        Component.Reset();
        Component.Add(StartID);
        Seen[StartID] = 1;
        int32 MinTouch = MAX_int32;
        int32 MaxTouch = -1;

        for (int32 Head = 0; Head < Component.Num(); ++Head)
        {
            const int32 Cur = Component[Head];
            const int32 CX = Cur % N;
            const int32 CY = (Cur / N) % N;
            const int32 CZ = Cur / (N * N);

            for (int32 k = 0; k < 6; ++k)
            {
                const int32 NX = CX + GNeighborDelta[k][0];
                const int32 NY = CY + GNeighborDelta[k][1];
                const int32 NZ = CZ + GNeighborDelta[k][2];
                if (NX < 0 || NX >= N || NY < 0 || NY >= N || NZ < 0 || NZ >= N) continue;

                const int32 Nb = FCubeMath::EncodeCoord(FCubeCoordinate(NX, NY, NZ), N, N);
                if (PathIndex[Nb] != INDEX_NONE)
                {
                    MinTouch = FMath::Min(MinTouch, PathIndex[Nb]);
                    MaxTouch = FMath::Max(MaxTouch, PathIndex[Nb]);
                }
                else if (Open[Nb] && !Seen[Nb])
                {
                    Seen[Nb] = 1;
                    Component.Add(Nb);
                }
            }
        }

        if (MaxTouch - MinTouch >= 2)
        {
            Bridging.Append(Component);
        }
    }
    return Bridging;
}

// ============================================================================
// Indices de progression
// ============================================================================

namespace CubeClueDetail
{
    static int32 AxisBits(ECubeDirection D)
    {
        switch (D)
        {
            case ECubeDirection::East:  case ECubeDirection::West:   return 1;
            case ECubeDirection::North: case ECubeDirection::South:  return 2;
            case ECubeDirection::Top:   case ECubeDirection::Bottom: return 3;
            default:                                                 return 0;
        }
    }

    static bool IsNegative(ECubeDirection D)
    {
        return D == ECubeDirection::West || D == ECubeDirection::South || D == ECubeDirection::Bottom;
    }

    /** Expression dont le résultat exact est Value (Value >= 0). */
    static FString MakeNumberExpression(int32 Value, FRandomStream& Stream)
    {
        Value = FMath::Max(0, Value);

        // Les formes 0, 2 et 3 n'ont de sens que pour Value >= 1.
        int32 Form = Stream.RandRange(0, 4);
        if (Value < 1 && (Form == 0 || Form == 2 || Form == 3))
        {
            Form = Stream.RandBool() ? 1 : 4;
        }

        switch (Form)
        {
            case 0: // a² + b
            {
                const int32 A = Stream.RandRange(1, FMath::Max(1, FMath::FloorToInt(FMath::Sqrt((float)Value))));
                return FString::Printf(TEXT("%d\u00B2 + %d"), A, Value - A * A);
            }
            case 1: // a × b − c
            {
                const int32 A = Stream.RandRange(2, 9);
                const int32 B = Value / A + 1;
                return FString::Printf(TEXT("%d \u00D7 %d \u2212 %d"), A, B, A * B - Value);
            }
            case 2: // Σ(k=1..n) k + r
            {
                int32 MaxN = 1;
                while ((MaxN + 1) * (MaxN + 2) / 2 <= Value) ++MaxN;
                const int32 Nk = Stream.RandRange(1, MaxN);
                return FString::Printf(TEXT("\u03A3(k=1..%d) k + %d"), Nk, Value - Nk * (Nk + 1) / 2);
            }
            case 3: // 2^p + q
            {
                const int32 P = Stream.RandRange(0, (int32)FMath::FloorLog2((uint32)Value));
                return FString::Printf(TEXT("2^%d + %d"), P, Value - (1 << P));
            }
            default: // x tel que a·x + b = d
            {
                const int32 A = Stream.RandRange(2, 7);
                const int32 B = Stream.RandRange(1, 20);
                return FString::Printf(TEXT("x | %d\u00B7x + %d = %d"), A, B, A * Value + B);
            }
        }
    }

    /** Cellule braille Unicode. Point 1 = bit 5 (32) ... point 6 = bit 0 (1). */
    static FString MakeBraille(int32 Code)
    {
        int32 Dots = 0;
        for (int32 Dot = 0; Dot < 6; ++Dot)
        {
            if (Code & (1 << (5 - Dot))) Dots |= (1 << Dot);
        }
        return FString::Chr((TCHAR)(0x2800 + Dots));
    }

    static FString MakeFormula(int32 Code, FRandomStream& Stream)
    {
        return FString::Printf(TEXT("[ %s ]\u2082"), *MakeNumberExpression(Code, Stream));
    }

    /** Écritures d'un angle k·π/2, k dans [-1, 3]. */
    static FString MakeAngle(int32 QuarterTurns, FRandomStream& Stream)
    {
        static const TCHAR* const Forms[5][3] =
        {
            { TEXT("\u2212\u03C0/2"), TEXT("arcsin(\u22121)"), TEXT("\u2212arccos(0)") },
            { TEXT("0"),              TEXT("sin(\u03C0)"),     TEXT("ln(1)") },
            { TEXT("\u03C0/2"),       TEXT("arcsin(1)"),       TEXT("arccos(0)") },
            { TEXT("\u03C0"),         TEXT("arccos(\u22121)"), TEXT("2\u00B7arcsin(1)") },
            { TEXT("3\u03C0/2"),      TEXT("\u2212\u03C0/2"),  TEXT("3\u00B7arccos(0)") },
        };
        const int32 Row = FMath::Clamp(QuarterTurns + 1, 0, 4);
        return Forms[Row][Stream.RandRange(0, 2)];
    }

    /**
     * R(Pitch, Yaw, Roll) · d. Conventions UE : Yaw tourne de +X vers +Y monde, Pitch positif = +Z.
     * Grille -> monde inverse Y (FCubeMath::CellToWorld) : North (+Y grille) = -Y monde = Yaw 3pi/2,
     * South = +Y monde = Yaw pi/2. Le Roll ne change jamais la direction (leurre). À Pitch ±90°, le Yaw
     * n'a plus d'effet (gimbal lock, leurre).
     */
    static FString MakeEuler(const FCubeRouteToken& Token, FRandomStream& Stream)
    {
        int32 Pitch = 0;
        int32 Yaw = 0;
        switch (Token.Dir)
        {
            case ECubeDirection::East:   Yaw = 0; break;
            case ECubeDirection::South:  Yaw = 1; break;
            case ECubeDirection::West:   Yaw = 2; break;
            case ECubeDirection::North:  Yaw = 3; break;
            case ECubeDirection::Top:    Pitch = 1;  Yaw = Stream.RandRange(0, 3); break;
            case ECubeDirection::Bottom: Pitch = -1; Yaw = Stream.RandRange(0, 3); break;
            default: break;
        }
        const int32 Roll = Stream.RandRange(0, 3);

        return FString::Printf(TEXT("R(%s, %s, %s) \u00B7 d = %s"),
                               *MakeAngle(Pitch, Stream), *MakeAngle(Yaw, Stream), *MakeAngle(Roll, Stream),
                               *MakeNumberExpression(Token.Run, Stream));
    }
}

int32 UCubeRebusLibrary::EncodeRouteToken(const FCubeRouteToken& Token)
{
    return (CubeClueDetail::AxisBits(Token.Dir) << 4)
         | (CubeClueDetail::IsNegative(Token.Dir) ? 0x8 : 0x0)
         | FMath::Clamp(Token.Run, 0, 7);
}

FCubeRouteToken UCubeRebusLibrary::DecodeRouteToken(int32 Code)
{
    FCubeRouteToken Token;
    const int32 Axis = (Code >> 4) & 0x3;
    const bool bNeg = (Code & 0x8) != 0;
    Token.Run = Code & 0x7;

    switch (Axis)
    {
        case 1:  Token.Dir = bNeg ? ECubeDirection::West   : ECubeDirection::East;  break;
        case 2:  Token.Dir = bNeg ? ECubeDirection::South  : ECubeDirection::North; break;
        case 3:  Token.Dir = bNeg ? ECubeDirection::Bottom : ECubeDirection::Top;   break;
        default: Token.Dir = ECubeDirection::None; break;
    }
    return Token;
}

FString UCubeRebusLibrary::RouteTokenToString(const FCubeRouteToken& Token)
{
    static const TCHAR* const AxisNames[] = { TEXT("?"), TEXT("X"), TEXT("Y"), TEXT("Z") };
    return FString::Printf(TEXT("%s%s%d"),
                           AxisNames[CubeClueDetail::AxisBits(Token.Dir)],
                           CubeClueDetail::IsNegative(Token.Dir) ? TEXT("-") : TEXT("+"),
                           Token.Run);
}

FCubeClue UCubeRebusLibrary::MakeClue(const FString& Seed, const TArray<int32>& PathRoomIDs, int32 GridSize,
                                      int32 PathIndex, int32 Interval, ECubeDifficulty Difficulty)
{
    FCubeClue Clue;
    Clue.PathIndex = PathIndex;
    if (GridSize <= 0 || Interval <= 0 || PathIndex < 0 || PathIndex >= PathRoomIDs.Num() - 1) return Clue;

    // Tranche [PathIndex, PathIndex + Interval] du chemin, compressée en RLE.
    const int32 End = FMath::Min(PathIndex + Interval, PathRoomIDs.Num() - 1);
    TArray<int32> Slice;
    for (int32 i = PathIndex; i <= End; ++i) Slice.Add(PathRoomIDs[i]);

    // Une longueur tient sur 3 bits : on découpe les segments de plus de 7 pas.
    for (FCubeRouteToken Token : RouteToRLE(Slice, GridSize))
    {
        while (Token.Run > 7)
        {
            FCubeRouteToken Part = Token;
            Part.Run = 7;
            Clue.Tokens.Add(Part);
            Token.Run -= 7;
        }
        Clue.Tokens.Add(Token);
    }

    const uint32 ClueHash = CubeHash::Combine(CubeHash::Crc32Str(Seed), CubeHash::Crc32Str(TEXT("CLUE")));
    FRandomStream Stream((int32)CubeHash::Combine(ClueHash, (uint32)PathIndex));

    Clue.Style = (Difficulty == ECubeDifficulty::Facile)
        ? ECubeClueStyle::Plain
        : static_cast<ECubeClueStyle>(Stream.RandRange((int32)ECubeClueStyle::Braille, (int32)ECubeClueStyle::Euler));

    for (const FCubeRouteToken& Token : Clue.Tokens)
    {
        const int32 Code = EncodeRouteToken(Token);
        Clue.Codes.Add(Code);

        switch (Clue.Style)
        {
            case ECubeClueStyle::Braille: Clue.PuzzleLines.Add(CubeClueDetail::MakeBraille(Code));         break;
            case ECubeClueStyle::Formula: Clue.PuzzleLines.Add(CubeClueDetail::MakeFormula(Code, Stream)); break;
            case ECubeClueStyle::Euler:   Clue.PuzzleLines.Add(CubeClueDetail::MakeEuler(Token, Stream));  break;
            default:                      Clue.PuzzleLines.Add(RouteTokenToString(Token));                 break;
        }
    }

    Clue.PuzzleText = FString::Join(Clue.PuzzleLines, TEXT("   "));
    return Clue;
}
