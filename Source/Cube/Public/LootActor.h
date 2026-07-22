#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DA_Loot.h"
#include "LootActor.generated.h"

UCLASS()
class CUBE_API ALootActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ALootActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	/** Initialise le loot avec les données du Data Asset */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void InitializeLoot(UDA_Loot* InLootData);

	/** Fonction appelée quand un joueur ramasse le loot */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void PickUp(class AActor* Picker);

	/** Reset pour retourner dans le pool */
	void ResetLoot();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* LootMesh;

	/** Overlap pour détection automatique du joueur */
	UFUNCTION()
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, 
						class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
						bool bFromSweep, const FHitResult& SweepResult);

private:
	UPROPERTY()
	UDA_Loot* CurrentLootData;

	float HoverTime;
	FVector StartHoverLocation;
};
