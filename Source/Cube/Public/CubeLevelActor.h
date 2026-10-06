// Acteur « niveau » : porte les ancrages de passerelle Socket_Entree / Socket_Sortie et, en option,
// un aperçu instancié du cube (équivalent de la vue de l'add-on Blender). Manuals/Solveur_Documentation.md §7-§8.
//
// Réseau : le serveur calcule les deux ancrages depuis son manifest et les réplique (deux transforms).
// Le manifest (donc le chemin) ne quitte jamais le serveur, ce qui reste compatible WorldChampionship.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubeLogicTypes.h"
#include "CubePortalLibrary.h"
#include "CubeViewLibrary.h"
#include "CubeLevelActor.generated.h"

class UArrowComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS(Blueprintable)
class CUBE_API ACubeLevelActor : public AActor
{
    GENERATED_BODY()

public:
    ACubeLevelActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * Serveur : calcule les ancrages depuis un manifest résolu (et l'aperçu si bShowPreviewInGame).
     * InRho / InShortcuts : champ mortel et raccourcis déjà calculés par le GameMode (aperçu uniquement).
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cube|Level")
    void InitFromManifest(const FCubeManifest& InManifest, float InRoomSize, float InRho, const TSet<int32>& InShortcuts);

    /** Reconstruit l'aperçu instancié (coupe Z, pièges...) sans régénérer le niveau. */
    UFUNCTION(BlueprintCallable, Category="Cube|Level")
    void RefreshPreview();

    /** Place une passerelle pour que son socket BridgeSocket (None = "Socket_Cube") coïncide avec l'ancrage, puis l'y attache. */
    UFUNCTION(BlueprintCallable, Category="Cube|Portal")
    bool AttachBridge(AActor* Bridge, bool bEntry, FName BridgeSocket = NAME_None);

    UFUNCTION(BlueprintPure, Category="Cube|Portal")
    FCubePortal GetPortal(bool bEntry) const { return bEntry ? EntryPortal : ExitPortal; }

    /** Socket_Entree (bEntry) ou Socket_Sortie. */
    UFUNCTION(BlueprintPure, Category="Cube|Portal")
    USceneComponent* GetAnchor(bool bEntry) const;

    /** Transform monde de l'ancrage (X = vers la passerelle). */
    UFUNCTION(BlueprintPure, Category="Cube|Portal")
    FTransform GetAnchorWorldTransform(bool bEntry) const;

protected:
    UFUNCTION()
    void OnRep_Portals();

    void ApplyAnchors();
    void RegeneratePreviewInEditor();

    /** Ancrages calculés par le serveur ; seule donnée répliquée. */
    UPROPERTY(ReplicatedUsing=OnRep_Portals, VisibleInstanceOnly, BlueprintReadOnly, Category="Cube|Portal")
    FCubePortal EntryPortal;

    UPROPERTY(ReplicatedUsing=OnRep_Portals, VisibleInstanceOnly, BlueprintReadOnly, Category="Cube|Portal")
    FCubePortal ExitPortal;

    // ---- Aperçu ----

    /** Régénère l'aperçu dans l'éditeur à chaque modification (comme le panneau Blender). */
    UPROPERTY(EditAnywhere, Category="Cube|Preview")
    bool bPreviewInEditor = true;

    /** En jeu, instancie aussi le cube complet (serveur / standalone uniquement : le manifest ne se réplique pas). */
    UPROPERTY(EditAnywhere, Category="Cube|Preview")
    bool bShowPreviewInGame = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview")
    FString PreviewSeed = TEXT("1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview", meta=(ClampMin="8", ClampMax="64"))
    int32 PreviewGridSize = 64;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview", meta=(ClampMin="0.0", ClampMax="1.0"))
    float PreviewPathLen = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview", meta=(ClampMin="0.02", ClampMax="1.0"))
    float PreviewVerticality = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview", meta=(ClampMin="0", ClampMax="3"))
    int32 PreviewNKeys = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview")
    FCubeViewParams ViewParams;

    /** Espacement des salles (cm). En jeu, imposé par le GameMode (RoomSize). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview", meta=(ClampMin="10.0"))
    float RoomSize = 100.0f;

    /** Part de la cellule occupée par le mesh (0,96 : joint de 4 % entre salles, comme la R&D). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview", meta=(ClampMin="0.1", ClampMax="1.0"))
    float RoomFill = 0.96f;

    /** Mesh d'une salle, centré, de côté RoomMeshSize (par défaut le cube moteur de 100 cm). Son matériau
     *  lit PerInstanceCustomData[0] = ECubeViewMaterial pour la couleur. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview")
    TObjectPtr<UStaticMesh> RoomMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube|Preview", meta=(ClampMin="1.0"))
    float RoomMeshSize = 100.0f;

    // ---- Composants ----

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Level")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Level")
    TObjectPtr<UInstancedStaticMeshComponent> PreviewRooms;

    /** Ancrage d'entrée : centre de la face extérieure du premier cube, X = vers la passerelle. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    TObjectPtr<USceneComponent> EntryAnchor;

    /** Ancrage de sortie : centre de la face extérieure du dernier cube, X = vers la passerelle. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cube|Portal")
    TObjectPtr<USceneComponent> ExitAnchor;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UArrowComponent> EntryArrow;

    UPROPERTY()
    TObjectPtr<UArrowComponent> ExitArrow;
#endif

private:
    /** Manifest de l'aperçu (serveur / éditeur uniquement, jamais répliqué). */
    FCubeManifest Manifest;
    TSet<int32> Shortcuts;
    TArray<uint8> CellMaterials;
    FCubeViewInstances ViewInstances;
    TArray<FTransform> InstanceTransforms;

    // Clés de cache : un réglage d'affichage ne relance ni le générateur ni les raccourcis.
    FString CachedKey;
    float CachedRho = -1.0f;
    int32 CachedReach = -1;
    bool bCachedShowShortcuts = false;
    bool bMaterialsValid = false;
    /** Raccourcis fournis par le GameMode (en jeu) plutôt que recalculés par l'aperçu. */
    bool bExternalShortcuts = false;
};
