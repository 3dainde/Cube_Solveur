#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DA_Loot.generated.h"

UENUM(BlueprintType)
enum class ELootType : uint8
{
	None UMETA(DisplayName="Aucun"),
	Health UMETA(DisplayName="Vie"),
	Ammo UMETA(DisplayName="Munitions"),
	Armor UMETA(DisplayName="Armure"),
	XP UMETA(DisplayName="XP"),
	Currency UMETA(DisplayName="Monnaie")
};

/**
 * Décrit un objet ramassable (Loot).
 */
UCLASS(BlueprintType)
class CUBE_API UDA_Loot : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FString LootName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	ELootType LootType = ELootType::Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	float Value = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Visuals")
	class UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Visuals")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Visuals")
	FVector Size = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Effects")
	class USoundBase* PickupSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Effects")
	class UNiagaraSystem* PickupVFX;
};
