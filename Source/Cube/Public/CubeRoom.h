#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubeRebusTypes.h"
#include "CubeRoom.generated.h"

class UStaticMeshComponent;

/**
 * Salle physique (BP_CUBE). Représente UNE cellule du Cube.
 * Spawnée/poolée à la demande par UCubeRoomStreamer autour du joueur.
 * La logique de vérité reste serveur ; cet acteur est purement présentation.
 */
UCLASS(Blueprintable)
class CUBE_API ACubeRoom : public AActor
{
    GENERATED_BODY()

public:
    ACubeRoom();

    /** (Ré)initialise la salle avec ses données logiques et sa position monde. */
    UFUNCTION(BlueprintCallable, Category="Cube|Room")
    void Setup(const FCubeRoomData& InData, int32 GridSize, float RoomSize);

    /** Remet la salle en pool (masquée, désactivée). */
    UFUNCTION(BlueprintCallable, Category="Cube|Room")
    void ReturnToPool();

    UFUNCTION(BlueprintPure, Category="Cube|Room")
    const FCubeRoomData& GetData() const { return Data; }

    /** Couleur de debug associée à l'état (conventions CubeRebusTypes). */
    UFUNCTION(BlueprintPure, Category="Cube|Room")
    static FLinearColor DebugColorForState(ECubeCellState State);

protected:
    /** Hook présentation : l'artiste branche meshes/portes/panneau-formule ici. */
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Room")
    void OnRoomConfigured(const FCubeRoomData& InData);

    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Room")
    void OnRoomPooled();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Room")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Room")
    TObjectPtr<UStaticMeshComponent> ShellMesh;

    UPROPERTY(BlueprintReadOnly, Category="Cube|Room")
    FCubeRoomData Data;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DebugMID;
};
