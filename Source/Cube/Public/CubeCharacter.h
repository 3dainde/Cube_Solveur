#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CubeCore.h"
#include "CubeCharacter.generated.h"

class UStaticMeshComponent;
class UCameraComponent;
class USpringArmComponent;

/**
 * Pawn joueur à déplacement DISCRET (salle par salle).
 * Une commande = un pas vers une cellule adjacente dans les bornes de la grille.
 * Le serveur arbitre la létalité/victoire (AGM_CubeGameMode::NotifyRoomEntered).
 */
UCLASS()
class CUBE_API ACubeCharacter : public APawn
{
    GENERATED_BODY()

public:
    ACubeCharacter();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Tente un pas dans une direction ; validé et exécuté côté serveur. */
    UFUNCTION(BlueprintCallable, Category="Cube|Move")
    void MoveInDirection(ECubeDirection Dir);

    UFUNCTION(BlueprintPure, Category="Cube|Move")
    FCubeCoordinate GetCurrentCell() const { return CurrentCell; }

    /** Place le pawn sur une cellule (téléportation logique, ex. spawn/respawn). */
    UFUNCTION(BlueprintCallable, Category="Cube|Move")
    void SetCell(FCubeCoordinate NewCell, bool bNotify);

protected:
    UFUNCTION(Server, Reliable)
    void ServerMove(ECubeDirection Dir);

    UFUNCTION()
    void OnRep_CurrentCell();

    /** Récupère N et RoomSize depuis le GameState répliqué. */
    bool GetGridParams(int32& OutN, float& OutRoomSize) const;

    void ApplyWorldTransform();
    void UpdateStreamerFocus();
    void RefreshLocalHUD();

    UPROPERTY(ReplicatedUsing=OnRep_CurrentCell, BlueprintReadOnly, Category="Cube|Move")
    FCubeCoordinate CurrentCell;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Components")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Components")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Components")
    TObjectPtr<UCameraComponent> Camera;

    /** Durée d'interpolation visuelle entre deux cellules (0 = instantané). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cube|Move")
    float StepLerpTime = 0.15f;
};
