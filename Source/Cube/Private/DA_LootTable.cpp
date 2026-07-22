#include "DA_LootTable.h"
#include "DA_Loot.h"
#include "Math/UnrealMathUtility.h"

UDA_Loot* UDA_LootTable::RollLoot() const
{
	float TotalWeight = NoLootProbability;
	for (const FLootTableEntry& Entry : LootEntries)
	{
		TotalWeight += Entry.Probability;
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float RandomRoll = FMath::FRandRange(0.0f, TotalWeight);
	
	// Si on tombe dans la zone du "Pas de loot"
	if (RandomRoll <= NoLootProbability)
	{
		return nullptr;
	}

	RandomRoll -= NoLootProbability;

	for (const FLootTableEntry& Entry : LootEntries)
	{
		if (RandomRoll <= Entry.Probability)
		{
			return Entry.LootItem;
		}
		RandomRoll -= Entry.Probability;
	}

	return nullptr;
}
