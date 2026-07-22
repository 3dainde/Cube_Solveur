#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DA_BlockType.generated.h"

class UDA_LootTable;

/**
 * Décrit un type de cube (Standard, Rare, Épique, Légendaire, etc.).
 */
UCLASS(BlueprintType)
class CUBE_API UDA_BlockType : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Info")
	FString BlockName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Visuals")
	class UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Visuals")
	class UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Visuals")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Stats")
	int32 HealthPoints = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Stats")
	int32 XP = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Stats")
	int32 Score = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Effects")
	class UNiagaraSystem* DestructionVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Effects")
	class USoundBase* DestructionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnProbability = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube Loot")
	UDA_LootTable* LootTable;
};
