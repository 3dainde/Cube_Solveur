#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CubeCore.h"
#include "CubeRebusTypes.h"
#include "PC_CubePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * PlayerController du mode Cube (grille discrète).
 * Six InputActions directionnelles -> un pas par déclenchement (Triggered).
 * Les Assets d'input sont assignés dans un Blueprint dérivé (aucune dépendance dure).
 * Fournit aussi des hooks HUD (formule / boussole) en BlueprintImplementableEvent.
 */
UCLASS()
class CUBE_API APC_CubePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APC_CubePlayerController();

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    /** Rafraîchit l'affichage de la formule de la salle courante (implémenté en BP/UMG). */
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|HUD")
    void OnFormulaUpdated(const FCubeFormula& Formula, bool bHasFormula);

    /** Met à jour la boussole/indice directionnel (implémenté en BP/UMG). */
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|HUD")
    void OnCompassUpdated(ECubeDirection SafeHint, bool bCompassEnabled);

    /** Recalcule et pousse le rébus/boussole de la salle courante vers le HUD. */
    UFUNCTION(BlueprintCallable, Category="Cube|HUD")
    void RefreshRoomHUD();

    /** Envoyé par le serveur à chaque entrée de salle (fonctionne aussi pour un client pur). */
    UFUNCTION(Client, Reliable)
    void ClientReceiveRoomClue(const FCubeClue& Clue, bool bHasClue, FCubeCoordinate Cell, bool bShowCoordinates);

    /** Indice de progression de la salle (bHasClue = false entre deux indices). Implémenté en BP/UMG. */
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|HUD")
    void OnClueUpdated(const FCubeClue& Clue, bool bHasClue);

    /** Coordonnées XYZ du joueur ; bShowCoordinates = false en Impossible. Implémenté en BP/UMG. */
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|HUD")
    void OnCoordinatesUpdated(FCubeCoordinate Cell, bool bShowCoordinates);

protected:
    // ---- Enhanced Input (assignés dans le BP dérivé) ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Input")
    TObjectPtr<UInputMappingContext> MappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Input")
    TObjectPtr<UInputAction> IA_MoveEast;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Input")
    TObjectPtr<UInputAction> IA_MoveWest;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Input")
    TObjectPtr<UInputAction> IA_MoveNorth;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Input")
    TObjectPtr<UInputAction> IA_MoveSouth;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Input")
    TObjectPtr<UInputAction> IA_MoveTop;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cube|Input")
    TObjectPtr<UInputAction> IA_MoveBottom;

    /** Priorité d'application du MappingContext. */
    UPROPERTY(EditDefaultsOnly, Category="Cube|Input")
    int32 MappingPriority = 0;

private:
    void StepEast(const FInputActionValue& Value);
    void StepWest(const FInputActionValue& Value);
    void StepNorth(const FInputActionValue& Value);
    void StepSouth(const FInputActionValue& Value);
    void StepTop(const FInputActionValue& Value);
    void StepBottom(const FInputActionValue& Value);

    void DoStep(ECubeDirection Dir);
};
