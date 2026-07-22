#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubeGrid.generated.h"

class UDA_BlockType;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class CUBE_API ACubeGrid : public AActor
{
	GENERATED_BODY()
	
public:	
	ACubeGrid();

protected:
	virtual void BeginPlay() override;

public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Grid")
	int32 NbrH = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Grid")
	int32 NbrV = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Grid")
	float BlockSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Grid")
	TArray<UDA_BlockType*> BlockTypes;

	UFUNCTION(BlueprintCallable, Category = "Cube Grid")
	void GenerateGrid();

	UFUNCTION(BlueprintCallable, Category = "Cube Grid")
	void DamageCube(UPrimitiveComponent* HitComponent, int32 InstanceIndex, int32 DamageAmount);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* GridRoot;

	// Liste des HISMs créés dynamiquement
	UPROPERTY()
	TMap<UDA_BlockType*, UHierarchicalInstancedStaticMeshComponent*> HISMMap;

	// Santé des cubes : Map de HISM -> Tableau de HP correspondant à chaque instance
	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<int32>> HealthMap;

	void RemoveCube(UHierarchicalInstancedStaticMeshComponent* HISM, int32 InstanceIndex, UDA_BlockType* BlockData);
};
