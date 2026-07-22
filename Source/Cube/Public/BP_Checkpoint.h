#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "BP_Checkpoint.generated.h"

UCLASS()
class CUBE_API ABP_Checkpoint : public AActor
{
	GENERATED_BODY()

public:
	ABP_Checkpoint();

	/** Appelé quand le joueur arrive sur le trigger */
	UFUNCTION(BlueprintCallable)
	void BP_CheckpointReached(APlayerController* Player);

	/** Appelé quand le joueur interagit avec le bouton */
	UFUNCTION(BlueprintCallable)
	void InteractButton(APlayerController* Player);

protected:
	/** Box pour détecter la présence du joueur */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	UBoxComponent* TriggerBox;

	/** Widget affichant le texte au-dessus du bouton */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	UWidgetComponent* ButtonWidget;

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
						AActor* OtherActor,
						UPrimitiveComponent* OtherComp,
						int32 OtherBodyIndex,
						bool bFromSweep,
						const FHitResult & SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp,
					  AActor* OtherActor,
					  UPrimitiveComponent* OtherComp,
					  int32 OtherBodyIndex);

	/** Permet de savoir si le joueur est dans la zone */
	bool bPlayerInTrigger;
};
