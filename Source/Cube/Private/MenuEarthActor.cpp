#include "MenuEarthActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace MenuEarth
{
    constexpr float SphereMeshRadius = 50.0f;
    constexpr float StationOrbitMultiplier = 1.18f;
}

AMenuEarthActor::AMenuEarthActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    EarthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Earth"));
    EarthMesh->SetupAttachment(SceneRoot);
    EarthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AtmosphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Atmosphere"));
    AtmosphereMesh->SetupAttachment(SceneRoot);
    AtmosphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AtmosphereMesh->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (SphereMesh.Succeeded())
    {
        EarthMesh->SetStaticMesh(SphereMesh.Object);
        AtmosphereMesh->SetStaticMesh(SphereMesh.Object);
    }

    const FVector StationDirections[] = {
        FVector(0.72f, -0.76f, 0.47f).GetSafeNormal(),
        FVector(-0.88f, -0.32f, -0.32f).GetSafeNormal(),
        FVector(0.23f, 0.83f, 0.67f).GetSafeNormal(),
    };
    const FLinearColor StationColours[] = {
        FLinearColor(0.30f, 0.68f, 1.0f),
        FLinearColor(1.0f, 0.66f, 0.22f),
        FLinearColor(0.32f, 0.96f, 0.70f),
    };

    for (int32 StationIndex = 0; StationIndex < UE_ARRAY_COUNT(StationDirections); ++StationIndex)
    {
        const FVector Direction = StationDirections[StationIndex];
        const FVector Tangent = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
        const int32 PartOffset = StationIndex * 3;
        StationParts[PartOffset] = CreateStationPart(
            *FString::Printf(TEXT("Station%dCore"), StationIndex + 1),
            Direction,
            FVector(0.065f, 0.065f, 0.065f),
            FLinearColor::White);
        StationParts[PartOffset + 1] = CreateStationPart(
            *FString::Printf(TEXT("Station%dSolarLeft"), StationIndex + 1),
            Direction - Tangent * 0.12f,
            FVector(0.055f, 0.012f, 0.10f),
            StationColours[StationIndex]);
        StationParts[PartOffset + 2] = CreateStationPart(
            *FString::Printf(TEXT("Station%dSolarRight"), StationIndex + 1),
            Direction + Tangent * 0.12f,
            FVector(0.055f, 0.012f, 0.10f),
            StationColours[StationIndex]);
    }

    if (CubeMesh.Succeeded())
    {
        for (TObjectPtr<UStaticMeshComponent> StationPart : StationParts)
        {
            StationPart->SetStaticMesh(CubeMesh.Object);
        }
    }
}

void AMenuEarthActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyAppearance();
    UpdateScaleAndStations();
}

void AMenuEarthActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (bRotationEnabled)
    {
        SceneRoot->AddLocalRotation(FRotator(0.0f, RotationSpeedDegrees * DeltaTime, 0.0f));
    }
}

void AMenuEarthActor::SetRotationEnabled(bool bEnabled)
{
    bRotationEnabled = bEnabled;
}

void AMenuEarthActor::SetRotationSpeed(float DegreesPerSecond)
{
    RotationSpeedDegrees = FMath::Max(0.0f, DegreesPerSecond);
}

UStaticMeshComponent* AMenuEarthActor::CreateStationPart(
    const FName& Name,
    const FVector& RelativeLocation,
    const FVector& RelativeScale,
    const FLinearColor& Colour)
{
    UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Part->SetupAttachment(SceneRoot);
    Part->SetRelativeLocation(RelativeLocation);
    Part->SetRelativeScale3D(RelativeScale);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetCastShadow(false);
    Part->SetRenderCustomDepth(false);
    Part->SetCustomPrimitiveDataVector4(0, FVector4(Colour.R, Colour.G, Colour.B, 1.0f));
    return Part;
}

void AMenuEarthActor::ApplyAppearance()
{
    if (EarthMaterial)
    {
        EarthMaterialInstance = UMaterialInstanceDynamic::Create(EarthMaterial, this);
        EarthMesh->SetMaterial(0, EarthMaterialInstance);
        if (DayTexture)
        {
            EarthMaterialInstance->SetTextureParameterValue(TEXT("DayTexture"), DayTexture);
        }
        if (NightTexture)
        {
            EarthMaterialInstance->SetTextureParameterValue(TEXT("NightTexture"), NightTexture);
        }
    }

    if (AtmosphereMaterial)
    {
        AtmosphereMaterialInstance = UMaterialInstanceDynamic::Create(AtmosphereMaterial, this);
        AtmosphereMesh->SetMaterial(0, AtmosphereMaterialInstance);
    }
}

void AMenuEarthActor::UpdateScaleAndStations()
{
    EarthMesh->SetRelativeScale3D(FVector(EarthScale));
    AtmosphereMesh->SetRelativeScale3D(FVector(EarthScale * 1.055f));

    const float OrbitRadius = MenuEarth::SphereMeshRadius * EarthScale * MenuEarth::StationOrbitMultiplier;
    for (TObjectPtr<UStaticMeshComponent> StationPart : StationParts)
    {
        const FVector Direction = StationPart->GetRelativeLocation().GetSafeNormal();
        StationPart->SetRelativeLocation(Direction * OrbitRadius);
    }
}