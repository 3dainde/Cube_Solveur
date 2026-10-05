// Ancrages d'entrée / sortie des passerelles (Manuals/Solveur_Documentation.md §7).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubeLogicTypes.h"
#include "CubePortalLibrary.generated.h"

/**
 * Ancrage d'une passerelle : centre de la face EXTÉRIEURE du cube d'entrée ou de sortie.
 * Le socket au bout de la passerelle (Socket_Cube) s'y accroche. Calculé en O(1) depuis les deux
 * premiers / deux derniers états du chemin du solveur : rien n'est stocké dans le manifest.
 * Repère : celui de la grille posée à l'origine (même repère que FCubeMath::CellToWorld), en cm.
 */
USTRUCT(BlueprintType)
struct CUBE_API FCubePortal
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    bool bValid = false;

    /** Face du volume portant l'ancrage (= direction de la normale sortante). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    ECubeDirection Face = ECubeDirection::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    int32 RoomID = -1;

    /** Cube d'entrée / de sortie dans la grille. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    FCubeCoordinate Cell;

    /** Position de l'ancrage (cm). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    FVector Location = FVector::ZeroVector;

    /** Axe X de l'ancrage = normale sortante (vers la passerelle), repère monde UE. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    FVector Forward = FVector::ZeroVector;

    /** Axe Z de l'ancrage : +Z (faces latérales) ou +X (faces Top / Bottom). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    FVector Up = FVector::ZeroVector;
};

UCLASS()
class CUBE_API UCubePortalLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Ancrage d'entrée (bEntry) ou de sortie d'un manifest résolu.
     *   n_entrée = -(p1 - p0), n_sortie = p_k - p_(k-1) ;  P = centre(p) + RoomSize/2 * n
     */
    UFUNCTION(BlueprintPure, Category="Cube|Portal")
    static FCubePortal ComputePortal(const FCubeManifest& Manifest, bool bEntry, float RoomSize = 100.0f);

    /** Transform de l'ancrage : X = Forward, Z = Up, origine = Location (repère de la grille). */
    UFUNCTION(BlueprintPure, Category="Cube|Portal")
    static FTransform GetAnchorTransform(const FCubePortal& Portal);

    /**
     * Transform monde à donner à une passerelle pour que son socket (SocketRelativeToActor, exprimé dans le
     * repère de la passerelle) coïncide avec l'ancrage : Acteur = Socket^-1 * Ancrage (convention UE :
     * Monde = Local * Parent).
     */
    UFUNCTION(BlueprintPure, Category="Cube|Portal")
    static FTransform ComputeBridgeTransform(const FTransform& AnchorWorld, const FTransform& SocketRelativeToActor);
};
