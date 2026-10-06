#pragma once

#include "CoreMinimal.h"
#include "CubeCore.generated.h"

CUBE_API DECLARE_LOG_CATEGORY_EXTERN(LogCubeSolver, Log, All);

/**
 * Six directions de la grille. L'index sénaire (0..5) = valeur - 1 ; c'est l'ordre DIRECTIONS de la
 * R&D Python et l'ordre de parcours des voisins du solveur (contractuel : il fixe les parents du BFS).
 */
UENUM(BlueprintType)
enum class ECubeDirection : uint8
{
    None,
    East = 1,   // +X
    West,       // -X
    North,      // +Y grille (= -Y monde UE, voir FCubeMath::CellToWorld)
    South,      // -Y grille (= +Y monde UE)
    Top,        // +Z
    Bottom      // -Z
};

/** Contenu d'une salle dans FCubeManifest::Cells (un octet par salle). */
UENUM(BlueprintType)
enum class ECubeCellType : uint8
{
    Floor = 0,
    Wall  = 1,
    Key   = 2,
    Gate  = 3
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

    FCubeCoordinate operator-(const FCubeCoordinate& Other) const
    {
        return FCubeCoordinate(X - Other.X, Y - Other.Y, Z - Other.Z);
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

/** S = <RoomID, Orientation, Inventory, Flags> (Manuals/Solveur_Documentation.md §3). */
USTRUCT(BlueprintType)
struct CUBE_API FCubeState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|State")
    int32 RoomID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|State")
    int32 Orientation = 0;

    /** Bit j = clé j détenue. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|State")
    int32 Inventory = 0;

    /** Bit j = porte j franchie. */
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

    bool operator!=(const FCubeState& Other) const
    {
        return !(*this == Other);
    }

    friend uint32 GetTypeHash(const FCubeState& State)
    {
        uint32 Hash = ::GetTypeHash(State.RoomID);
        Hash = HashCombine(Hash, ::GetTypeHash(State.Orientation));
        Hash = HashCombine(Hash, ::GetTypeHash(State.Inventory));
        Hash = HashCombine(Hash, ::GetTypeHash(State.Flags));
        return Hash;
    }

    /** Niveau de clé (couleur de segment) : position du bit de poids fort de l'inventaire, 0 sans clé. */
    int32 KeyLevel() const
    {
        return Inventory <= 0 ? 0 : 32 - static_cast<int32>(FMath::CountLeadingZeros(static_cast<uint32>(Inventory)));
    }
};

class CUBE_API FCubeMath
{
public:
    /** ID(X, Y, Z) = X + Y*Nx + Z*Nx*Ny */
    static int32 EncodeCoord(const FCubeCoordinate& Coord, int32 SizeX, int32 SizeY)
    {
        return Coord.X + (Coord.Y * SizeX) + (Coord.Z * SizeX * SizeY);
    }

    /** Z = ID div (Nx*Ny) ; R = ID mod (Nx*Ny) ; Y = R div Nx ; X = R mod Nx */
    static FCubeCoordinate DecodeCoord(int32 RoomID, int32 SizeX, int32 SizeY)
    {
        const int32 Z = RoomID / (SizeX * SizeY);
        const int32 Remainder = RoomID % (SizeX * SizeY);
        const int32 Y = Remainder / SizeX;
        const int32 X = Remainder % SizeX;
        return FCubeCoordinate(X, Y, Z);
    }

    static bool IsInGrid(const FCubeCoordinate& C, int32 N)
    {
        return C.X >= 0 && C.X < N && C.Y >= 0 && C.Y < N && C.Z >= 0 && C.Z < N;
    }

    /**
     * Position monde du centre d'une cellule (Manuals/Solveur_Documentation.md §1).
     * Grille centrée en X/Y, posée au sol en Z. La R&D (Blender) est en repère main droite, UE en
     * main gauche : on inverse Y pour que le niveau soit identique (et non en miroir) dans les deux.
     *   World = ( (X - h) * S,  -(Y - h) * S,  (Z + 0.5) * S ),  h = (N - 1) / 2
     */
    static FVector CellToWorld(const FCubeCoordinate& Coord, int32 N, double RoomSize)
    {
        const double Half = (N - 1) * 0.5;
        return FVector(
            (Coord.X - Half) * RoomSize,
            -(Coord.Y - Half) * RoomSize,
            (Coord.Z + 0.5) * RoomSize);
    }

    /** Conversion inverse (monde -> cellule discrète la plus proche). */
    static FCubeCoordinate WorldToCell(const FVector& World, int32 N, double RoomSize)
    {
        const double Half = (N - 1) * 0.5;
        return FCubeCoordinate(
            FMath::RoundToInt(World.X / RoomSize + Half),
            FMath::RoundToInt(-World.Y / RoomSize + Half),
            FMath::RoundToInt(World.Z / RoomSize - 0.5));
    }

    /** Vecteur grille d'une direction ((0,0,0) pour None). */
    static FCubeCoordinate DirectionDelta(ECubeDirection Dir)
    {
        switch (Dir)
        {
            case ECubeDirection::East:   return FCubeCoordinate( 1, 0, 0);
            case ECubeDirection::West:   return FCubeCoordinate(-1, 0, 0);
            case ECubeDirection::North:  return FCubeCoordinate( 0, 1, 0);
            case ECubeDirection::South:  return FCubeCoordinate( 0,-1, 0);
            case ECubeDirection::Top:    return FCubeCoordinate( 0, 0, 1);
            case ECubeDirection::Bottom: return FCubeCoordinate( 0, 0,-1);
            default:                     return FCubeCoordinate( 0, 0, 0);
        }
    }

    /** Direction d'un pas unitaire, None si le vecteur n'est pas un pas d'axe. */
    static ECubeDirection DirectionFromDelta(const FCubeCoordinate& D)
    {
        if (D.X ==  1 && D.Y == 0 && D.Z == 0) return ECubeDirection::East;
        if (D.X == -1 && D.Y == 0 && D.Z == 0) return ECubeDirection::West;
        if (D.X == 0 && D.Y ==  1 && D.Z == 0) return ECubeDirection::North;
        if (D.X == 0 && D.Y == -1 && D.Z == 0) return ECubeDirection::South;
        if (D.X == 0 && D.Y == 0 && D.Z ==  1) return ECubeDirection::Top;
        if (D.X == 0 && D.Y == 0 && D.Z == -1) return ECubeDirection::Bottom;
        return ECubeDirection::None;
    }

    static ECubeDirection Opposite(ECubeDirection Dir)
    {
        switch (Dir)
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

    /** Vecteur monde UE (unitaire) d'une direction grille : même inversion de Y que CellToWorld. */
    static FVector DirectionToWorld(ECubeDirection Dir)
    {
        const FCubeCoordinate D = DirectionDelta(Dir);
        return FVector(static_cast<double>(D.X), static_cast<double>(-D.Y), static_cast<double>(D.Z));
    }

    /**
     * Arrondi « au pair le plus proche » (round() de Python 3). FMath::RoundToInt arrondit x,5 vers
     * le haut et décalerait une clé quand (j+1)/(K+1)*(L-1) tombe sur un demi-entier.
     */
    static int32 RoundHalfToEven(double Value)
    {
        const double Floor = FMath::FloorToDouble(Value);
        const double Frac = Value - Floor;
        int64 Result = static_cast<int64>(Floor);
        if (Frac > 0.5 || (Frac == 0.5 && (Result & 1) != 0))
        {
            ++Result;
        }
        return static_cast<int32>(Result);
    }
};
