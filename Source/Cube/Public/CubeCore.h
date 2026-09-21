#pragma once

#include "CoreMinimal.h"
#include "CubeCore.generated.h"

UENUM(BlueprintType)
enum class ECubeDirection : uint8
{
    None,
    East = 1,   // +X
    West,       // -X
    North,      // +Y
    South,      // -Y
    Top,        // +Z
    Bottom      // -Z
};

UENUM(BlueprintType)
enum class ERoomArchetype : uint8
{
    Start,
    Normal,
    Hazard,
    Clue,
    Rotation,
    Puzzle,
    Checkpoint,
    Exit
};

USTRUCT(BlueprintType)
struct CUBE_API FCubeCoordinate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Coordinate")
    int32 X = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Coordinate")
    int32 Y = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Coordinate")
    int32 Z = 0;

    FCubeCoordinate() {}
    FCubeCoordinate(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}

    bool operator==(const FCubeCoordinate& Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }

    bool operator!=(const FCubeCoordinate& Other) const
    {
        return !(*this == Other);
    }

    FCubeCoordinate operator+(const FCubeCoordinate& Other) const
    {
        return FCubeCoordinate(X + Other.X, Y + Other.Y, Z + Other.Z);
    }

    friend uint32 GetTypeHash(const FCubeCoordinate& Coord)
    {
        return HashCombine(HashCombine(::GetTypeHash(Coord.X), ::GetTypeHash(Coord.Y)), ::GetTypeHash(Coord.Z));
    }

    int32 ManhattanDistance(const FCubeCoordinate& Other) const
    {
        return FMath::Abs(X - Other.X) + FMath::Abs(Y - Other.Y) + FMath::Abs(Z - Other.Z);
    }
};

USTRUCT(BlueprintType)
struct CUBE_API FCubeState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|State")
    int32 RoomID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|State")
    int32 Orientation = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|State")
    int32 Inventory = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|State")
    int32 Flags = 0;

    FCubeState() {}
    FCubeState(int32 InRoomID, int32 InOrientation, int32 InInventory, int32 InFlags)
        : RoomID(InRoomID), Orientation(InOrientation), Inventory(InInventory), Flags(InFlags) {}

    bool operator==(const FCubeState& Other) const
    {
        return RoomID == Other.RoomID && Orientation == Other.Orientation && 
               Inventory == Other.Inventory && Flags == Other.Flags;
    }

    friend uint32 GetTypeHash(const FCubeState& State)
    {
        uint32 Hash = ::GetTypeHash(State.RoomID);
        Hash = HashCombine(Hash, ::GetTypeHash(State.Orientation));
        Hash = HashCombine(Hash, ::GetTypeHash(State.Inventory));
        Hash = HashCombine(Hash, ::GetTypeHash(State.Flags));
        return Hash;
    }
};

class FCubeMath
{
public:
    static int32 EncodeCoord(const FCubeCoordinate& Coord, int32 SizeX, int32 SizeY)
    {
        return Coord.X + (Coord.Y * SizeX) + (Coord.Z * SizeX * SizeY);
    }

    static FCubeCoordinate DecodeCoord(int32 RoomID, int32 SizeX, int32 SizeY)
    {
        int32 Z = RoomID / (SizeX * SizeY);
        int32 Remainder = RoomID % (SizeX * SizeY);
        int32 Y = Remainder / SizeX;
        int32 X = Remainder % SizeX;
        return FCubeCoordinate(X, Y, Z);
    }
};
