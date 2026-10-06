// Sélection des salles à afficher (enveloppe visible, coupe Z, matériaux) : portage exact de
// build_grid_mesh (R&D Blender). Alimente un ISM : une instance par salle visible, matériau passé en
// PerInstanceCustomData[0] (index ECubeViewMaterial). Manuals/Solveur_Documentation.md §8.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubeLogicTypes.h"
#include "CubeViewLibrary.generated.h"

UENUM(BlueprintType)
enum class ECubeWallMode : uint8
{
	Full UMETA(DisplayName = "Grille pleine"),
	Cut  UMETA(DisplayName = "Coupe Z"),
	None UMETA(DisplayName = "Chemin seul"),
};

/** Index de matériau d'une instance (même ordre que la palette de la R&D). */
UENUM(BlueprintType)
enum class ECubeViewMaterial : uint8
{
	Wall     = 0,
	Floor    = 1,
	Lethal   = 2,
	Shortcut = 3,
	Path0    = 4 UMETA(DisplayName = "Path (no key)"),
	Path1    = 5 UMETA(DisplayName = "Path (key 1)"),
	Path2    = 6 UMETA(DisplayName = "Path (key 2)"),
	Path3    = 7 UMETA(DisplayName = "Path (key 3)"),
};

USTRUCT(BlueprintType)
struct CUBE_API FCubeViewParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube|View")
	ECubeWallMode WallMode = ECubeWallMode::Cut;

	/** Dernière couche Z affichée en mode Coupe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube|View", meta = (ClampMin = "0", ClampMax = "63"))
	int32 CutZ = 32;

	/** false : chemin creusé (tunnel). true : une instance colorée par salle du chemin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube|View")
	bool bShowPathCubes = false;

	/** N'instancie que les salles qui touchent du vide : image identique, ~14x moins d'instances. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube|View")
	bool bOptimize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube|View")
	bool bShowShortcuts = true;

	/** Densité de pièges mortels (0 = aucun). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube|View", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Rho = 0.25f;

	/** Distance maximale (en salles) entre un raccourci et le chemin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube|View", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ShortcutReach = 2;
};

/** Salles à instancier et leur matériau (tableaux parallèles). */
USTRUCT(BlueprintType)
struct CUBE_API FCubeViewInstances
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cube|View")
	TArray<int32> RoomIDs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cube|View")
	TArray<uint8> Materials;
};

UCLASS()
class CUBE_API UCubeViewLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Matériau de chaque salle hors chemin (Wall / Lethal / Shortcut), à calculer une fois par seed.
	 *  Shortcuts : sortie de UCubeRebusLibrary::ComputeSafeShortcuts (ignorée si bShowShortcuts est faux). */
	static void BuildCellMaterials(const FCubeManifest& Manifest, const FCubeViewParams& Params,
		const TSet<int32>& Shortcuts, TArray<uint8>& OutCellMaterials);

	/** Instances à afficher pour un réglage d'affichage (un manifest résolu est requis). */
	UFUNCTION(BlueprintCallable, Category = "Cube|View")
	static FCubeViewInstances BuildView(const FCubeManifest& Manifest, const FCubeViewParams& Params, const TSet<int32>& Shortcuts);

	/** Variante sans recalcul des matériaux par salle (CellMaterials : sortie de BuildCellMaterials). */
	static void BuildViewCached(const FCubeManifest& Manifest, const FCubeViewParams& Params,
		const TSet<int32>& Shortcuts, const TArray<uint8>& CellMaterials, FCubeViewInstances& Out);
};
