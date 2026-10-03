#include "GM_CubeGameMode.h"
#include "GS_CubeGameState.h"
#include "DA_CubeDifficulty.h"
#include "CubeGenerator.h"
#include "CubeRebusLibrary.h"
#include "CubeHash.h"
#include "CubeCharacter.h"
#include "PC_CubePlayerController.h"
#include "CubeRoomStreamer.h"
#include "CubeRoom.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

AGM_CubeGameMode::AGM_CubeGameMode()
{
    GameStateClass = AGS_CubeGameState::StaticClass();
    DefaultPawnClass = ACubeCharacter::StaticClass();
    PlayerControllerClass = APC_CubePlayerController::StaticClass();
}

void AGM_CubeGameMode::BeginPlay()
{
    Super::BeginPlay();
    BuildLevel();
}

FString AGM_CubeGameMode::ResolveSeed() const
{
    if (!ForcedSeed.IsEmpty())
    {
        return ForcedSeed;
    }
    // Seed passée par le menu : OpenLevel(..., "Seed=xxx").
    const FString OptionSeed = UGameplayStatics::ParseOption(OptionsString, TEXT("Seed"));
    if (!OptionSeed.IsEmpty())
    {
        return OptionSeed;
    }
    // Seed dérivée du temps réseau : reproductible dans le manifest une fois fixée.
    const uint32 R = FMath::Rand() ^ (uint32)(FPlatformTime::Cycles());
    return FString::Printf(TEXT("seed_%u"), R);
}

ECubeDifficulty AGM_CubeGameMode::ResolveDifficulty() const
{
    const FString Option = UGameplayStatics::ParseOption(OptionsString, TEXT("Difficulty"));
    if (Option.Equals(TEXT("Facile"), ESearchCase::IgnoreCase) || Option.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
    {
        return ECubeDifficulty::Facile;
    }
    if (Option.Equals(TEXT("Impossible"), ESearchCase::IgnoreCase))
    {
        return ECubeDifficulty::Impossible;
    }
    if (Option.Equals(TEXT("WorldChampionship"), ESearchCase::IgnoreCase))
    {
        return ECubeDifficulty::WorldChampionship;
    }
    return StartingDifficulty;
}

int32 AGM_CubeGameMode::PickGridSize(const FString& Seed) const
{
    const int32 Lo = Config ? Config->GridSizeMin : 10;
    const int32 Hi = Config ? Config->GridSizeMax : 32;
    if (Hi <= Lo) return FMath::Clamp(Lo, 10, 64);

    const uint32 H = CubeHash::Combine(CubeHash::Crc32Str(Seed), CubeHash::Crc32Str(TEXT("SIZE")));
    return Lo + (int32)(H % (uint32)(Hi - Lo + 1));
}

void AGM_CubeGameMode::BuildLevel()
{
    AGS_CubeGameState* GS = GetGameState<AGS_CubeGameState>();
    if (GS) { GS->Phase = ECubeMatchPhase::Generating; }

    Difficulty = ResolveDifficulty();
    if (const TObjectPtr<UDA_CubeDifficulty>* Found = DifficultyTable.Find(Difficulty))
    {
        Config = *Found;
    }
    Rho = Config ? Config->Rho : 0.0f;

    const FString Seed = ResolveSeed();
    const int32 N = PickGridSize(Seed);

    Generator = NewObject<UCubeGenerator>(this);
    const float PathLen     = Config ? Config->PathLen : 0.45f;
    const float Verticality = Config ? Config->Verticality : 0.18f;
    const int32 NKeys       = Config ? Config->NKeys : 2;
    Manifest = Generator->GenerateCubeAdvanced(Seed, N, PathLen, Verticality, NKeys, 64);

    if (!Manifest.bIsValid)
    {
        UE_LOG(LogTemp, Error, TEXT("[Cube] Génération échouée pour seed=%s N=%d"), *Seed, N);
        if (GS) { GS->Phase = ECubeMatchPhase::Failed; }
        return;
    }

    // Indexation du chemin critique.
    PathRoomIDs = Manifest.CriticalPath;
    SafeSet = TSet<int32>(PathRoomIDs);
    PathIndexByRoom.Reset();
    for (int32 i = 0; i < PathRoomIDs.Num(); ++i)
    {
        if (!PathIndexByRoom.Contains(PathRoomIDs[i]))
            PathIndexByRoom.Add(PathRoomIDs[i], i);
    }

    // Chemin unique strict : les zones saines qui court-circuitent le chemin deviennent mortelles.
    // Sinon, raccourcis sains (PathFinder_Annexes).
    ShortcutSet.Reset();
    BridgingSet.Reset();
    const bool bStrict = Config && Config->bStrictUniquePath;
    if (bStrict)
    {
        BridgingSet = UCubeRebusLibrary::ComputeBridgingCells(Seed, PathRoomIDs, N, Rho);
    }
    else if (bEnableShortcuts && Rho > 0.0f)
    {
        ShortcutSet = UCubeRebusLibrary::ComputeSafeShortcuts(Seed, PathRoomIDs, N, Rho);
    }

    UE_LOG(LogTemp, Log, TEXT("[Cube] Niveau prêt : seed=%s N=%d |chemin|=%d raccourcis=%d interdits=%d Rho=%.2f strict=%d"),
           *Seed, N, PathRoomIDs.Num(), ShortcutSet.Num(), BridgingSet.Num(), Rho, bStrict ? 1 : 0);

    if (GS)
    {
        GS->Seed = Seed;
        GS->GridSize = N;
        GS->RoomSize = RoomSize;
        GS->Difficulty = Difficulty;
        GS->Phase = ECubeMatchPhase::Playing;
    }

    bLevelReady = true;

    // Streaming des salles (si une classe de salle est fournie).
    if (RoomClass)
    {
        if (UCubeRoomStreamer* Streamer = GetWorld()->GetSubsystem<UCubeRoomStreamer>())
        {
            Streamer->Configure(RoomClass, StreamRadius);
            Streamer->SetFocusCell(GetStartCell());
        }
    }

    PlaceAllPlayersAtStart();
}

FCubeCoordinate AGM_CubeGameMode::GetStartCell() const
{
    const int32 N = Manifest.GridSize;
    if (N <= 0) return FCubeCoordinate(0, 0, 0);
    return FCubeMath::DecodeCoord(Manifest.StartRoomID, N, N);
}

void AGM_CubeGameMode::PlaceAllPlayersAtStart()
{
    if (!bLevelReady) return;
    const FCubeCoordinate Start = GetStartCell();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (ACubeCharacter* Cube = Cast<ACubeCharacter>(PC->GetPawn()))
            {
                Cube->SetCell(Start, false);
                PushRoomHUD(PC, Start);
            }
        }
    }
}

void AGM_CubeGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);

    // Si le niveau est déjà prêt (rejoueur tardif ou respawn), replacer au départ.
    if (bLevelReady)
    {
        if (ACubeCharacter* Cube = Cast<ACubeCharacter>(NewPlayer ? NewPlayer->GetPawn() : nullptr))
        {
            Cube->SetCell(GetStartCell(), false);
            PushRoomHUD(NewPlayer, GetStartCell());
        }
    }
}

ECubeCellState AGM_CubeGameMode::GetCellState(FCubeCoordinate Cell) const
{
    const int32 N = Manifest.GridSize;
    if (N <= 0) return ECubeCellState::Inactive;

    const int32 ID = FCubeMath::EncodeCoord(Cell, N, N);
    if (ID == Manifest.StartRoomID) return ECubeCellState::Start;
    if (ID == Manifest.ExitRoomID)  return ECubeCellState::Exit;
    if (SafeSet.Contains(ID))        return ECubeCellState::SafePath;
    if (ShortcutSet.Contains(ID))    return ECubeCellState::SafeShortcut;
    if (IsCellLethal(Cell))          return ECubeCellState::Lethal;
    return ECubeCellState::Inactive; // En mode strict : cul-de-sac sain (pièce safe hors chemin).
}

bool AGM_CubeGameMode::IsCellLethal(FCubeCoordinate Cell) const
{
    const int32 N = Manifest.GridSize;
    if (N <= 0) return false;
    if (BridgingSet.Contains(FCubeMath::EncodeCoord(Cell, N, N))) return true;
    return UCubeRebusLibrary::IsLethal(Manifest.Seed, Cell.X, Cell.Y, Cell.Z, Rho, SafeSet, N);
}

bool AGM_CubeGameMode::GetRoomClue(FCubeCoordinate Cell, FCubeClue& OutClue) const
{
    OutClue = FCubeClue();

    const int32 N = Manifest.GridSize;
    if (N <= 0) return false;

    const int32* IdxPtr = PathIndexByRoom.Find(FCubeMath::EncodeCoord(Cell, N, N));
    if (!IdxPtr) return false;

    const int32 Index = *IdxPtr;
    const int32 Interval = Config ? FMath::Max(1, Config->ClueInterval) : 5;
    if (Index % Interval != 0 || Index >= PathRoomIDs.Num() - 1) return false;

    OutClue = UCubeRebusLibrary::MakeClue(Manifest.Seed, PathRoomIDs, N, Index, Interval, Difficulty);
    return OutClue.Tokens.Num() > 0;
}

void AGM_CubeGameMode::PushRoomHUD(AController* Controller, const FCubeCoordinate& Cell) const
{
    APC_CubePlayerController* PC = Cast<APC_CubePlayerController>(Controller);
    if (!PC) return;

    FCubeClue Clue;
    const bool bHasClue = GetRoomClue(Cell, Clue);

    // Hors Facile, le client ne reçoit que le rébus, jamais la solution.
    if (Difficulty != ECubeDifficulty::Facile)
    {
        Clue.StripSolution();
    }

    const bool bShowCoordinates = Config ? Config->bShowCoordinates : (Difficulty == ECubeDifficulty::Facile);
    PC->ClientReceiveRoomClue(Clue, bHasClue, Cell, bShowCoordinates);
}

static ECubeDirection DeltaToDirection(const FCubeCoordinate& D)
{
    if (D.X ==  1 && D.Y == 0 && D.Z == 0) return ECubeDirection::East;
    if (D.X == -1 && D.Y == 0 && D.Z == 0) return ECubeDirection::West;
    if (D.X == 0 && D.Y ==  1 && D.Z == 0) return ECubeDirection::North;
    if (D.X == 0 && D.Y == -1 && D.Z == 0) return ECubeDirection::South;
    if (D.X == 0 && D.Y == 0 && D.Z ==  1) return ECubeDirection::Top;
    if (D.X == 0 && D.Y == 0 && D.Z == -1) return ECubeDirection::Bottom;
    return ECubeDirection::None;
}

bool AGM_CubeGameMode::GetRoomRebus(FCubeCoordinate Cell, FCubeFormula& OutFormula, ECubeDirection& OutSafeHint) const
{
    OutFormula = FCubeFormula();
    OutSafeHint = ECubeDirection::None;

    const int32 N = Manifest.GridSize;
    if (N <= 0) return false;

    const int32 ID = FCubeMath::EncodeCoord(Cell, N, N);
    const int32* IdxPtr = PathIndexByRoom.Find(ID);
    if (!IdxPtr) return false;

    const int32 Index = *IdxPtr;
    if (Index < 0 || Index >= PathRoomIDs.Num() - 1) return false; // Sortie : pas de rébus.

    const FCubeCoordinate Next = FCubeMath::DecodeCoord(PathRoomIDs[Index + 1], N, N);
    const FCubeCoordinate D(Next.X - Cell.X, Next.Y - Cell.Y, Next.Z - Cell.Z);
    OutSafeHint = DeltaToDirection(D);
    if (OutSafeHint == ECubeDirection::None) return false;

    const int32 Tier       = Config ? Config->FormulaTier : 1;
    const bool  bDecoy     = Config ? Config->bEnableDecoy : false;
    const int32 SafeDirIdx = UCubeRebusLibrary::DirectionToIndex(OutSafeHint);

    OutFormula = UCubeRebusLibrary::MakeFormula(Manifest.Seed, Index, Cell, SafeDirIdx, Tier, bDecoy);
    return true;
}

void AGM_CubeGameMode::NotifyRoomEntered(AController* Controller, FCubeCoordinate Cell)
{
    if (!HasAuthority() || !Manifest.bIsValid) return;

    const int32 N = Manifest.GridSize;
    const int32 ID = FCubeMath::EncodeCoord(Cell, N, N);
    AGS_CubeGameState* GS = GetGameState<AGS_CubeGameState>();

    if (ID == Manifest.ExitRoomID)
    {
        if (GS) { GS->Phase = ECubeMatchPhase::Solved; }
        UE_LOG(LogTemp, Log, TEXT("[Cube] Sortie atteinte — niveau résolu."));
        return;
    }

    if (IsCellLethal(Cell))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Cube] Cellule mortelle (%d,%d,%d) — mort du joueur."),
               Cell.X, Cell.Y, Cell.Z);
        // Conséquence : renvoi au départ (checkpoint minimal). Un PlayerState pourra
        // enrichir ceci (vies, score) sans changer l'arbitrage serveur.
        if (Controller)
        {
            if (ACubeCharacter* Cube = Cast<ACubeCharacter>(Controller->GetPawn()))
            {
                Cube->SetCell(GetStartCell(), false);
                PushRoomHUD(Controller, GetStartCell());
            }
        }
        return;
    }

    PushRoomHUD(Controller, Cell);
}
