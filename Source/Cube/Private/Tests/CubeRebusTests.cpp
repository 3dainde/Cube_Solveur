// Tests d'automatisation du coeur Rébus — non-régression et solvabilité.
// Lancer via : Session Frontend > Automation > "Cube.*", ou
//   UnrealEditor-Cmd Cube.uproject -ExecCmds="Automation RunTests Cube" -unattended -nop4
//
// Les valeurs de référence figent le comportement actuel du hash, du champ mortel et des formules :
// si l'une change, toutes les seeds existantes produisent un autre niveau.

#include "Misc/AutomationTest.h"
#include "CubeRebusLibrary.h"
#include "CubeHash.h"
#include "CubeGenerator.h"
#include "CubeSolver.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeHashRegressionTest, "Cube.Rebus.HashRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeHashRegressionTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("CRC32(seedA)"), CubeHash::Crc32Str(TEXT("seedA")), 989772893u);
    TestEqual(TEXT("CRC32(TRAP)"),  CubeHash::Crc32Str(TEXT("TRAP")),  2283442823u);
    TestEqual(TEXT("CRC32(PATH)"),  CubeHash::Crc32Str(TEXT("PATH")),  1036084923u);
    TestEqual(TEXT("Combine(1,2)"), CubeHash::Combine(1u, 2u),         2654435834u);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeLethalRegressionTest, "Cube.Rebus.LethalRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeLethalRegressionTest::RunTest(const FString& Parameters)
{
    const TSet<int32> Empty;
    const int32 N = 64;
    TestTrue (TEXT("seedA (3,4,5) rho0.35 mortel"),   UCubeRebusLibrary::IsLethal(TEXT("seedA"), 3, 4, 5, 0.35f, Empty, N));
    TestFalse(TEXT("seedA (0,0,0) rho0.35 sain"),     UCubeRebusLibrary::IsLethal(TEXT("seedA"), 0, 0, 0, 0.35f, Empty, N));
    TestTrue (TEXT("level_1 (12,7,20) rho0.7 mortel"), UCubeRebusLibrary::IsLethal(TEXT("level_1"), 12, 7, 20, 0.7f, Empty, N));
    TestTrue (TEXT("level_1 (49,49,49) rho0.5 mortel"), UCubeRebusLibrary::IsLethal(TEXT("level_1"), 49, 49, 49, 0.5f, Empty, N));

    // Une cellule du chemin (SafeSet) n'est jamais mortelle.
    TSet<int32> Safe;
    Safe.Add(FCubeMath::EncodeCoord(FCubeCoordinate(3, 4, 5), N, N));
    TestFalse(TEXT("SafeSet neutralise la letalite"), UCubeRebusLibrary::IsLethal(TEXT("seedA"), 3, 4, 5, 0.35f, Safe, N));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeFormulaRegressionTest, "Cube.Rebus.FormulaRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeFormulaRegressionTest::RunTest(const FString& Parameters)
{
    struct FReference { const TCHAR* Seed; int32 Pi, X, Y, Z, Sd, A1, A2, A3, A4; };
    const FReference Cases[] = {
        { TEXT("seedA"),   3,  4,  5,  6, 2, 1, 1, 2, 5 },
        { TEXT("level_1"), 17, 12, 7, 20, 5, 2, 1, 1, 2 },
        { TEXT("S1"),      0,  0,  0,  0, 0, 1, 2, 3, 0 },
    };

    for (const FReference& G : Cases)
    {
        const FCubeFormula F = UCubeRebusLibrary::MakeFormula(G.Seed, G.Pi, FCubeCoordinate(G.X, G.Y, G.Z), G.Sd, 1, false);
        TestEqual(FString::Printf(TEXT("%s A1"), G.Seed), F.A1, G.A1);
        TestEqual(FString::Printf(TEXT("%s A2"), G.Seed), F.A2, G.A2);
        TestEqual(FString::Printf(TEXT("%s A3"), G.Seed), F.A3, G.A3);
        TestEqual(FString::Printf(TEXT("%s A4"), G.Seed), F.A4, G.A4);
        // La formule doit pointer sur la direction sûre.
        TestEqual(FString::Printf(TEXT("%s Eval==Sd"), G.Seed),
                  UCubeRebusLibrary::EvaluateFormula(F, FCubeCoordinate(G.X, G.Y, G.Z)), G.Sd);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeGeneratorOddGridTest, "Cube.Generator.OddGridSolvable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeGeneratorOddGridTest::RunTest(const FString& Parameters)
{
    UCubeGenerator* Gen = NewObject<UCubeGenerator>();
    const int32 Sizes[] = { 11, 13, 15, 27, 33, 10, 32 };
    for (int32 N : Sizes)
    {
        const FCubeManifest M = Gen->GenerateCube(FString::Printf(TEXT("auto_%d"), N), N);
        TestTrue(FString::Printf(TEXT("N=%d valide"), N), M.bIsValid);
        TestTrue(FString::Printf(TEXT("N=%d chemin non vide"), N), M.CriticalPath.Num() > 0);
        if (M.CriticalPath.Num() > 0)
        {
            TestEqual(FString::Printf(TEXT("N=%d depart==Start"), N), M.CriticalPath[0], M.StartRoomID);
            TestEqual(FString::Printf(TEXT("N=%d arrivee==Exit"), N), M.CriticalPath.Last(), M.ExitRoomID);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeShortcutStrictnessTest, "Cube.Rebus.ShortcutStrictness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeShortcutStrictnessTest::RunTest(const FString& Parameters)
{
    UCubeGenerator* Gen = NewObject<UCubeGenerator>();
    const FString Seed = TEXT("shortcut_seed");
    const int32 N = 15;
    const float Rho = 0.35f;

    const FCubeManifest M = Gen->GenerateCube(Seed, N);
    TestTrue(TEXT("Manifest valide"), M.bIsValid);

    const TSet<int32> PathSet(M.CriticalPath);
    const TSet<int32> Shortcuts = UCubeRebusLibrary::ComputeSafeShortcuts(Seed, M.CriticalPath, N, Rho);

    for (int32 C : Shortcuts)
    {
        // Invariant §7.2 : un raccourci n'est jamais sur le chemin ni mortel.
        TestFalse(TEXT("Raccourci hors chemin"), PathSet.Contains(C));
        const FCubeCoordinate Coord = FCubeMath::DecodeCoord(C, N, N);
        TestFalse(TEXT("Raccourci non mortel"),
                  UCubeRebusLibrary::IsLethal(Seed, Coord.X, Coord.Y, Coord.Z, Rho, PathSet, N));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeClueCodeTest, "Cube.Clue.CodeRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeClueCodeTest::RunTest(const FString& Parameters)
{
    FCubeRouteToken XPlus1;  XPlus1.Dir = ECubeDirection::East;    XPlus1.Run = 1;
    FCubeRouteToken ZMinus2; ZMinus2.Dir = ECubeDirection::Bottom; ZMinus2.Run = 2;
    TestEqual(TEXT("X+1 = 010001 = 17"), UCubeRebusLibrary::EncodeRouteToken(XPlus1), 17);
    TestEqual(TEXT("Z-2 = 111010 = 58"), UCubeRebusLibrary::EncodeRouteToken(ZMinus2), 58);
    TestEqual(TEXT("Texte X+1"), UCubeRebusLibrary::RouteTokenToString(XPlus1), FString(TEXT("X+1")));

    for (int32 d = 1; d <= 6; ++d)
    for (int32 Run = 1; Run <= 7; ++Run)
    {
        FCubeRouteToken T;
        T.Dir = static_cast<ECubeDirection>(d);
        T.Run = Run;
        const FCubeRouteToken Back = UCubeRebusLibrary::DecodeRouteToken(UCubeRebusLibrary::EncodeRouteToken(T));
        TestTrue(FString::Printf(TEXT("Aller-retour dir=%d run=%d"), d, Run), Back.Dir == T.Dir && Back.Run == T.Run);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeClueDeterminismTest, "Cube.Clue.Determinism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeClueDeterminismTest::RunTest(const FString& Parameters)
{
    UCubeGenerator* Gen = NewObject<UCubeGenerator>();
    const FString Seed = TEXT("clue_seed");
    const int32 N = 15;
    const int32 Interval = 5;
    const FCubeManifest M = Gen->GenerateCube(Seed, N);
    TestTrue(TEXT("Manifest valide"), M.bIsValid);

    for (int32 i = 0; i < M.CriticalPath.Num() - 1; i += Interval)
    {
        const FCubeClue A = UCubeRebusLibrary::MakeClue(Seed, M.CriticalPath, N, i, Interval, ECubeDifficulty::Impossible);
        const FCubeClue B = UCubeRebusLibrary::MakeClue(Seed, M.CriticalPath, N, i, Interval, ECubeDifficulty::Impossible);
        TestEqual(FString::Printf(TEXT("Rébus stable i=%d"), i), A.PuzzleText, B.PuzzleText);
        TestTrue(FString::Printf(TEXT("Style encodé i=%d"), i), A.Style != ECubeClueStyle::Plain);

        // Rejouer les segments depuis la salle doit retomber exactement sur le chemin.
        FCubeCoordinate Pos = FCubeMath::DecodeCoord(M.CriticalPath[i], N, N);
        int32 Steps = 0;
        for (const int32 Code : A.Codes)
        {
            const FCubeRouteToken T = UCubeRebusLibrary::DecodeRouteToken(Code);
            for (int32 s = 0; s < T.Run; ++s)
            {
                switch (T.Dir)
                {
                    case ECubeDirection::East:   Pos.X += 1; break;
                    case ECubeDirection::West:   Pos.X -= 1; break;
                    case ECubeDirection::North:  Pos.Y += 1; break;
                    case ECubeDirection::South:  Pos.Y -= 1; break;
                    case ECubeDirection::Top:    Pos.Z += 1; break;
                    case ECubeDirection::Bottom: Pos.Z -= 1; break;
                    default: break;
                }
                ++Steps;
                TestEqual(FString::Printf(TEXT("Pas sur le chemin i=%d s=%d"), i, Steps),
                          FCubeMath::EncodeCoord(Pos, N, N), M.CriticalPath[i + Steps]);
            }
        }
        TestEqual(FString::Printf(TEXT("Longueur couverte i=%d"), i), Steps, FMath::Min(Interval, M.CriticalPath.Num() - 1 - i));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeStrictUniquePathTest, "Cube.Rebus.StrictUniquePath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeStrictUniquePathTest::RunTest(const FString& Parameters)
{
    UCubeGenerator* Gen = NewObject<UCubeGenerator>();
    const int32 Sizes[] = { 11, 15, 27 };
    const float Rhos[] = { 0.0f, 0.35f, 0.6f };

    for (const int32 N : Sizes)
    for (const float Rho : Rhos)
    {
        const FString Seed = FString::Printf(TEXT("strict_%d_%.2f"), N, Rho);
        const FCubeManifest M = Gen->GenerateCube(Seed, N);
        if (!TestTrue(FString::Printf(TEXT("%s valide"), *Seed), M.bIsValid)) continue;

        const TSet<int32> PathSet(M.CriticalPath);
        const TSet<int32> Bridging = UCubeRebusLibrary::ComputeBridgingCells(Seed, M.CriticalPath, N, Rho);
        const int32 Total = N * N * N;

        auto IsSafe = [&](int32 ID)
        {
            if (PathSet.Contains(ID)) return true;
            if (Bridging.Contains(ID)) return false;
            const FCubeCoordinate C = FCubeMath::DecodeCoord(ID, N, N);
            return !UCubeRebusLibrary::IsLethal(Seed, C.X, C.Y, C.Z, Rho, PathSet, N);
        };

        // BFS sur les cellules saines depuis le départ.
        TArray<int32> Dist;
        Dist.Init(INDEX_NONE, Total);
        TArray<int32> Queue;
        Queue.Add(M.CriticalPath[0]);
        Dist[M.CriticalPath[0]] = 0;
        static const int32 Delta[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };

        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const FCubeCoordinate C = FCubeMath::DecodeCoord(Queue[Head], N, N);
            for (int32 k = 0; k < 6; ++k)
            {
                const FCubeCoordinate Nb(C.X + Delta[k][0], C.Y + Delta[k][1], C.Z + Delta[k][2]);
                if (Nb.X < 0 || Nb.X >= N || Nb.Y < 0 || Nb.Y >= N || Nb.Z < 0 || Nb.Z >= N) continue;
                const int32 ID = FCubeMath::EncodeCoord(Nb, N, N);
                if (Dist[ID] == INDEX_NONE && IsSafe(ID))
                {
                    Dist[ID] = Dist[Queue[Head]] + 1;
                    Queue.Add(ID);
                }
            }
        }

        // Aucun raccourci : chaque case du chemin est atteinte exactement à son rang.
        bool bUnique = true;
        for (int32 i = 0; i < M.CriticalPath.Num(); ++i)
        {
            bUnique &= (Dist[M.CriticalPath[i]] == i);
        }
        TestTrue(FString::Printf(TEXT("%s chemin unique"), *Seed), bUnique);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
