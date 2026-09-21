#include "CubeGenerator.h"

FCubeManifest UCubeGenerator::GenerateCube(FString Seed, int32 GridSize)
{
    FCubeManifest Manifest;
    Manifest.Seed = Seed;
    Manifest.GridSize = GridSize;
    
    // Hashage stable pour créer les sous-seeds déterministes.
    uint32 SeedHash = FCrc::StrCrc32(*Seed);
    
    // Sous-seed purement pour la ROUTE critique (ne sera pas affecté par un changement d'indices)
    FRandomStream PathStream(HashCombine(SeedHash, FCrc::StrCrc32(TEXT("PATH"))));

    GenerateCriticalPath(Manifest, PathStream);

    return Manifest;
}

void UCubeGenerator::GenerateCriticalPath(FCubeManifest& Manifest, FRandomStream& Stream)
{
    // Fixe le START au centre d'une face, par exemple (0, Milieu, Milieu)
    FCubeCoordinate StartCoord(0, Manifest.GridSize / 2, Manifest.GridSize / 2);
    int32 StartID = FCubeMath::EncodeCoord(StartCoord, Manifest.GridSize, Manifest.GridSize);
    
    Manifest.StartRoomID = StartID;

    FCubeCoordinate CurrentCoord = StartCoord;
    int32 CurrentID = StartID;

    TArray<ECubeDirection> AllDirs = { 
        ECubeDirection::East, ECubeDirection::West, 
        ECubeDirection::North, ECubeDirection::South, 
        ECubeDirection::Top, ECubeDirection::Bottom 
    };

    FCubeLogicalRoom StartRoom;
    StartRoom.RoomID = StartID;
    StartRoom.Coordinate = StartCoord;
    StartRoom.Archetype = ERoomArchetype::Start;
    StartRoom.bIsCriticalPath = true;
    Manifest.Rooms.Add(StartID, StartRoom);

    // Longueur de R&D : dynamique selon une vraie config "RouteProfile", codé dur pour le moment
    int32 PathLength = FMath::Min(80, (Manifest.GridSize * Manifest.GridSize) / 2);

    for (int32 i = 0; i < PathLength; ++i)
    {
        // Fisher-Yates shuffle déterministe pour choisir la prochaine direction
        for (int32 k = 0; k < AllDirs.Num(); ++k)
        {
            int32 SwapIndex = Stream.RandRange(0, AllDirs.Num() - 1);
            AllDirs.Swap(k, SwapIndex);
        }

        bool bMoved = false;
        for (ECubeDirection Dir : AllDirs)
        {
            FCubeCoordinate NextCoord = CurrentCoord;
            switch (Dir)
            {
                case ECubeDirection::East:   NextCoord.X += 1; break;
                case ECubeDirection::West:   NextCoord.X -= 1; break;
                case ECubeDirection::North:  NextCoord.Y += 1; break;
                case ECubeDirection::South:  NextCoord.Y -= 1; break;
                case ECubeDirection::Top:    NextCoord.Z += 1; break;
                case ECubeDirection::Bottom: NextCoord.Z -= 1; break;
                default: break;
            }

            // Maintien dans la grille logique
            if (NextCoord.X >= 0 && NextCoord.X < Manifest.GridSize &&
                NextCoord.Y >= 0 && NextCoord.Y < Manifest.GridSize &&
                NextCoord.Z >= 0 && NextCoord.Z < Manifest.GridSize)
            {
                int32 NextID = FCubeMath::EncodeCoord(NextCoord, Manifest.GridSize, Manifest.GridSize);
                
                // Prévention de l'auto-intersection (Self-Avoiding Walk)
                if (!Manifest.Rooms.Contains(NextID))
                {
                    Manifest.Rooms[CurrentID].Doors.Add(Dir, true);
                    
                    FCubeLogicalRoom NextRoom;
                    NextRoom.RoomID = NextID;
                    NextRoom.Coordinate = NextCoord;
                    NextRoom.bIsCriticalPath = true;
                    
                    ECubeDirection OppositeDir = ECubeDirection::None;
                    switch(Dir) {
                        case ECubeDirection::East: OppositeDir = ECubeDirection::West; break;
                        case ECubeDirection::West: OppositeDir = ECubeDirection::East; break;
                        case ECubeDirection::North: OppositeDir = ECubeDirection::South; break;
                        case ECubeDirection::South: OppositeDir = ECubeDirection::North; break;
                        case ECubeDirection::Top: OppositeDir = ECubeDirection::Bottom; break;
                        case ECubeDirection::Bottom: OppositeDir = ECubeDirection::Top; break;
                        default: break;
                    }
                    NextRoom.Doors.Add(OppositeDir, true);

                    Manifest.Rooms.Add(NextID, NextRoom);
                    
                    CurrentCoord = NextCoord;
                    CurrentID = NextID;
                    bMoved = true;
                    break;
                }
            }
        }

        if (!bMoved)
        {
            // Impasse atteinte avant la longueur cible. Géré par le solveur plus tard.
            break;
        }
    }

    Manifest.Rooms[CurrentID].Archetype = ERoomArchetype::Exit;
    Manifest.ExitRoomID = CurrentID;
}
