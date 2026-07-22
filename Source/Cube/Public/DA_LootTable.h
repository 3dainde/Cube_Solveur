#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DA_LootTable.generated.h"

class UDA_Loot;

USTRUCT(BlueprintType)
struct FLootTableEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	UDA_Loot* LootItem = nullptr;

	// Probabilité d'apparition de cet objet (peut être un pourcentage ou un poids)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Probability = 0.0f;
};

/**
 * Décrit les récompenses possibles lors de la destruction d'un cube.
 */
UCLASS(BlueprintType)
class CUBE_API UDA_LootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Table")
	TArray<FLootTableEntry> LootEntries;

	// Probabilité de ne rien avoir (en pourcentage ou poids)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Table", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float NoLootProbability = 40.0f;

	/** Sélectionne un loot de manière aléatoire en fonction des probabilités */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	UDA_Loot* RollLoot() const;
};
