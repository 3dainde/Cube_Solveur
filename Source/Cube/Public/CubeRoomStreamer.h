#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CubeCore.h"
#include "CubeRoomStreamer.generated.h"

class ACubeRoom;
class AGM_CubeGameMode;

/**
 * Streaming / pooling des salles autour du joueur.
 * Maintient visible un cube de rayon `Radius` (distance de Tchebychev) de
 * salles réelles autour de la cellule focus, en recyclant les acteurs ACubeRoom.
 * Lit la vérité du niveau depuis AGM_CubeGameMode (paliers locaux Facile/Impossible).
 */
UCLASS()
class CUBE_API UCubeRoomStreamer : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    /** À appeler une fois (GameMode/BeginPlay) pour fournir la classe de salle. */
    UFUNCTION(BlueprintCallable, Category="Cube|Streamer")
    void Configure(TSubclassOf<ACubeRoom> InRoomClass, int32 InRadius = 3);

    /** Déplace le centre de streaming (cellule discrète du joueur). */
    UFUNCTION(BlueprintCallable, Category="Cube|Streamer")
    void SetFocusCell(FCubeCoordinate NewFocus);

    // UTickableWorldSubsystem
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    void RefreshStreaming();
    ACubeRoom* AcquireRoom();

    UPROPERTY() TSubclassOf<ACubeRoom> RoomClass;
    UPROPERTY() TMap<int32, TObjectPtr<ACubeRoom>> ActiveRooms;
    UPROPERTY() TArray<TObjectPtr<ACubeRoom>> Pool;

    int32 Radius = 3;
    bool bConfigured = false;
    bool bDirty = false;
    FCubeCoordinate Focus;
    FCubeCoordinate LastFocus = FCubeCoordinate(-9999, -9999, -9999);
};
