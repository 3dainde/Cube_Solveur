#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LootPoolSubsystem.generated.h"

class ALootActor;
class UDA_Loot;

/**
 * Gère le pool d'acteurs de loot pour éviter les instanciations répétées
 */
UCLASS()
class CUBE_API ULootPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Fait apparaitre un loot à la position donnée, en le récupérant du pool si possible */
	UFUNCTION(BlueprintCallable, Category = "Loot Pool")
	ALootActor* SpawnLoot(UDA_Loot* LootData, FVector Location, FRotator Rotation = FRotator::ZeroRotator);

	/** Remet un acteur dans le pool */
	UFUNCTION(BlueprintCallable, Category = "Loot Pool")
	void ReturnLoot(ALootActor* LootActor);

private:
	UPROPERTY()
	TArray<ALootActor*> InactivePool;
};
