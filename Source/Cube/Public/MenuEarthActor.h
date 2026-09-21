#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MenuEarthActor.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTexture;

UCLASS(BlueprintType)
class CUBE_API AMenuEarthActor : public AActor
{
    GENERATED_BODY()

public:
    AMenuEarthActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Menu Earth")
    void SetRotationEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Menu Earth")
    void SetRotationSpeed(float DegreesPerSecond);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu Earth")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu Earth")
    TObjectPtr<UStaticMeshComponent> EarthMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu Earth")
    TObjectPtr<UStaticMeshComponent> AtmosphereMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Earth|Appearance")
    TObjectPtr<UMaterialInterface> EarthMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Earth|Appearance")
    TObjectPtr<UMaterialInterface> AtmosphereMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Earth|Appearance")
    TObjectPtr<UTexture> DayTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Earth|Appearance")
    TObjectPtr<UTexture> NightTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Earth|Motion", meta = (ClampMin = "0.0", ClampMax = "20.0"))
    float RotationSpeedDegrees = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Earth|Motion")
    bool bRotationEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Earth|Scale", meta = (ClampMin = "1.0", ClampMax = "100.0"))
    float EarthScale = 20.0f;

private:
    void ApplyAppearance();
    void UpdateScaleAndStations();
    UStaticMeshComponent* CreateStationPart(const FName& Name, const FVector& RelativeLocation, const FVector& RelativeScale, const FLinearColor& Colour);

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> EarthMaterialInstance;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> AtmosphereMaterialInstance;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> StationParts[9];
};