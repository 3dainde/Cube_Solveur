#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "MyCharacterWingsuit.generated.h"

UCLASS()
class CUBE_API AMyCharacterWingsuit : public ACharacter
{
	GENERATED_BODY()

public:
	AMyCharacterWingsuit();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// ========== INPUTS ==========
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);

	// ========== TOGGLES ==========
	void ToggleWingsuit();
	void ToggleCameraMode();
	void CycleCameraDistance();

	// ========== CAMERA ==========
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	UCameraComponent* Camera;

	// ========== WINGSUIT ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wingsuit")
	bool bIsWingsuitActive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wingsuit")
	float GlideGravityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wingsuit")
	float GlideForwardForce;

	// ========== CAMERA MODES ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	bool bIsThirdPerson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	TArray<float> CameraDistances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	int32 CurrentCameraDistanceIndex;

private:
	void UpdateCamera();
	void HandleWingsuit(float DeltaTime);
};
