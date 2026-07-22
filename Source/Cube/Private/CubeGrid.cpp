#include "CubeGrid.h"
#include "DA_BlockType.h"
#include "DA_LootTable.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Math/UnrealMathUtility.h"
#include "LootPoolSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

ACubeGrid::ACubeGrid()
{
	PrimaryActorTick.bCanEverTick = false;

	GridRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GridRoot"));
	RootComponent = GridRoot;
}

void ACubeGrid::BeginPlay()
{
	Super::BeginPlay();
	GenerateGrid();
}

void ACubeGrid::GenerateGrid()
{
	// Nettoyage existant
	for (auto& Pair : HISMMap)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	HISMMap.Empty();
	HealthMap.Empty();

	if (BlockTypes.Num() == 0) return;

	// Calculer le poids total des probabilités
	float TotalWeight = 0.0f;
	for (UDA_BlockType* BT : BlockTypes)
	{
		if (BT) TotalWeight += BT->SpawnProbability;
	}

	if (TotalWeight <= 0.0f) return;

	// Création des HISMs
	for (UDA_BlockType* BT : BlockTypes)
	{
		if (!BT) continue;

		UHierarchicalInstancedStaticMeshComponent* NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		NewHISM->RegisterComponent();
		NewHISM->AttachToComponent(GridRoot, FAttachmentTransformRules::KeepRelativeTransform);
		
		if (BT->Mesh) NewHISM->SetStaticMesh(BT->Mesh);
		if (BT->Material) NewHISM->SetMaterial(0, BT->Material);
		
		// Activer les custom data si on veut utiliser des variations (3 pour RGB)
		NewHISM->NumCustomDataFloats = 3; 

		HISMMap.Add(BT, NewHISM);
		HealthMap.Add(NewHISM, TArray<int32>());
	}

	// Génération de la grille
	for (int32 X = 0; X < NbrH; ++X)
	{
		for (int32 Y = 0; Y < NbrH; ++Y) // Assuming a 3D grid: X, Y for horizontal, Z for vertical ? Or X horizontal, Z vertical ?
		{
			for (int32 Z = 0; Z < NbrV; ++Z)
			{
				float RandomRoll = FMath::FRandRange(0.0f, TotalWeight);
				UDA_BlockType* SelectedBT = nullptr;

				for (UDA_BlockType* BT : BlockTypes)
				{
					if (RandomRoll <= BT->SpawnProbability)
					{
						SelectedBT = BT;
						break;
					}
					RandomRoll -= BT->SpawnProbability;
				}

				if (!SelectedBT) SelectedBT = BlockTypes.Last(); // Fallback
				if (!SelectedBT) continue;

				UHierarchicalInstancedStaticMeshComponent* HISM = HISMMap[SelectedBT];
				if (HISM)
				{
					FVector Location = FVector(X * BlockSize, Y * BlockSize, Z * BlockSize);
					FTransform InstanceTransform(FRotator::ZeroRotator, Location, FVector(1.0f));
					
					int32 InstanceIndex = HISM->AddInstance(InstanceTransform);
					
					// Assigner la couleur personnalisée (Custom Data)
					HISM->SetCustomDataValue(InstanceIndex, 0, SelectedBT->Color.R, false);
					HISM->SetCustomDataValue(InstanceIndex, 1, SelectedBT->Color.G, false);
					HISM->SetCustomDataValue(InstanceIndex, 2, SelectedBT->Color.B, true);

					// Stocker la vie initiale
					HealthMap[HISM].Add(SelectedBT->HealthPoints);
				}
			}
		}
	}
}

void ACubeGrid::DamageCube(UPrimitiveComponent* HitComponent, int32 InstanceIndex, int32 DamageAmount)
{
	UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitComponent);
	if (!HISM) return;

	// Retrouver le type de bloc associé
	UDA_BlockType* HitBlockType = nullptr;
	for (auto& Pair : HISMMap)
	{
		if (Pair.Value == HISM)
		{
			HitBlockType = Pair.Key;
			break;
		}
	}

	if (!HitBlockType || !HealthMap.Contains(HISM)) return;

	if (HealthMap[HISM].IsValidIndex(InstanceIndex))
	{
		HealthMap[HISM][InstanceIndex] -= DamageAmount;

		if (HealthMap[HISM][InstanceIndex] <= 0)
		{
			RemoveCube(HISM, InstanceIndex, HitBlockType);
		}
	}
}

void ACubeGrid::RemoveCube(UHierarchicalInstancedStaticMeshComponent* HISM, int32 InstanceIndex, UDA_BlockType* BlockData)
{
	FTransform InstanceTransform;
	HISM->GetInstanceTransform(InstanceIndex, InstanceTransform, true);
	
	// Effets visuels et sonores
	if (BlockData->DestructionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BlockData->DestructionSound, InstanceTransform.GetLocation());
	}
	if (BlockData->DestructionVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BlockData->DestructionVFX, InstanceTransform.GetLocation());
	}

	// Apparition du Loot
	if (BlockData->LootTable)
	{
		UDA_Loot* LootItem = BlockData->LootTable->RollLoot();
		if (LootItem)
		{
			if (ULootPoolSubsystem* PoolSys = GetWorld()->GetSubsystem<ULootPoolSubsystem>())
			{
				PoolSys->SpawnLoot(LootItem, InstanceTransform.GetLocation());
			}
		}
	}

	// Ajouter XP et Score au joueur
	FString Msg = FString::Printf(TEXT("Cube Détruit ! Score +%d | XP +%d"), BlockData->Score, BlockData->XP);
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, Msg);

	// Logique pour enlever l'instance et mettre à jour la map de santé
	int32 LastIndex = HISM->GetInstanceCount() - 1;
	
	if (InstanceIndex != LastIndex)
	{
		// Le dernier élément va remplacer celui qu'on supprime (comportement d'Unreal)
		HealthMap[HISM][InstanceIndex] = HealthMap[HISM][LastIndex];
	}
	
	HealthMap[HISM].RemoveAt(LastIndex);
	HISM->RemoveInstance(InstanceIndex);
}
