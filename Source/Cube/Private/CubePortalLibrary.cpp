#include "CubePortalLibrary.h"
#include "Math/RotationMatrix.h"

FCubePortal UCubePortalLibrary::ComputePortal(const FCubeManifest& M, bool bEntry, float RoomSize)
{
    FCubePortal Portal;
    const TArray<int32>& Path = M.CriticalPath;
    const int32 N = M.GridSize;
    if (Path.Num() < 2 || N <= 0)
    {
        return Portal;
    }

    const int32 Room = bEntry ? Path[0] : Path.Last();
    const FCubeCoordinate A = FCubeMath::DecodeCoord(bEntry ? Path[0] : Path[Path.Num() - 2], N, N);
    const FCubeCoordinate B = FCubeMath::DecodeCoord(bEntry ? Path[1] : Path.Last(), N, N);
    const ECubeDirection Face = FCubeMath::DirectionFromDelta(bEntry ? (A - B) : (B - A));
    if (Face == ECubeDirection::None)
    {
        return Portal;                                         // chemin non unitaire : invalide
    }

    const double S = static_cast<double>(RoomSize);
    const FVector Normal = FCubeMath::DirectionToWorld(Face);  // Y inversé comme CellToWorld
    const bool bVerticalFace = (Face == ECubeDirection::Top || Face == ECubeDirection::Bottom);

    Portal.bValid = true;
    Portal.Face = Face;
    Portal.RoomID = Room;
    Portal.Cell = FCubeMath::DecodeCoord(Room, N, N);
    Portal.Location = FCubeMath::CellToWorld(Portal.Cell, N, S) + Normal * (0.5 * S);
    Portal.Forward = Normal;
    Portal.Up = bVerticalFace ? FVector(1.0, 0.0, 0.0) : FVector(0.0, 0.0, 1.0);
    return Portal;
}

FTransform UCubePortalLibrary::GetAnchorTransform(const FCubePortal& Portal)
{
    if (!Portal.bValid)
    {
        return FTransform::Identity;
    }
    const FQuat Rotation = FRotationMatrix::MakeFromXZ(Portal.Forward, Portal.Up).ToQuat();
    return FTransform(Rotation, Portal.Location);
}

FTransform UCubePortalLibrary::ComputeBridgeTransform(const FTransform& AnchorWorld, const FTransform& SocketRelativeToActor)
{
    return SocketRelativeToActor.Inverse() * AnchorWorld;
}
