#include "LootPoolSubsystem.h"
#include "LootActor.h"
#include "Engine/World.h"

void ULootPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InactivePool.Empty();
}

void ULootPoolSubsystem::Deinitialize()
{
	InactivePool.Empty();
	Super::Deinitialize();
}

ALootActor* ULootPoolSubsystem::SpawnLoot(UDA_Loot* LootData, FVector Location, FRotator Rotation)
{
	if (!LootData || !GetWorld()) return nullptr;

	ALootActor* LootInstance = nullptr;

	// Chercher un acteur disponible dans le pool
	if (InactivePool.Num() > 0)
	{
		LootInstance = InactivePool.Pop();
		LootInstance->SetActorLocationAndRotation(Location, Rotation);
	}
	else
	{
		// Créer un nouvel acteur s'il n'y en a pas dans le pool
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LootInstance = GetWorld()->SpawnActor<ALootActor>(ALootActor::StaticClass(), Location, Rotation, SpawnParams);
	}

	if (LootInstance)
	{
		LootInstance->InitializeLoot(LootData);
	}

	return LootInstance;
}

void ULootPoolSubsystem::ReturnLoot(ALootActor* LootActor)
{
	if (LootActor)
	{
		LootActor->ResetLoot();
		InactivePool.AddUnique(LootActor);
	}
}
