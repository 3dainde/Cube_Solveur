#include "LootActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "PS_PlayerStateCustom.h"
#include "LootPoolSubsystem.h"
#include "CubeGameInstance.h"

ALootActor::ALootActor()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->InitSphereRadius(50.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);

	LootMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootMesh"));
	LootMesh->SetupAttachment(RootComponent);
	LootMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Pas de physique, juste de l'affichage

	HoverTime = 0.0f;
}

void ALootActor::BeginPlay()
{
	Super::BeginPlay();
	
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ALootActor::OnOverlapBegin);
	StartHoverLocation = GetActorLocation();
}

void ALootActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Lévitation simple (sinusoïdale)
	HoverTime += DeltaTime;
	float HoverOffset = FMath::Sin(HoverTime * 3.0f) * 10.0f; // Vitesse 3, Amplitude 10

	// Rotation continue
	FRotator NewRotation = LootMesh->GetRelativeRotation();
	NewRotation.Yaw += 45.0f * DeltaTime; // 45 degrés par seconde

	LootMesh->SetRelativeLocation(FVector(0, 0, HoverOffset));
	LootMesh->SetRelativeRotation(NewRotation);
}

void ALootActor::InitializeLoot(UDA_Loot* InLootData)
{
	if (!InLootData) return;

	CurrentLootData = InLootData;
	
	if (CurrentLootData->Mesh)
	{
		LootMesh->SetStaticMesh(CurrentLootData->Mesh);
	}
	
	LootMesh->SetRelativeScale3D(CurrentLootData->Size);
	StartHoverLocation = GetActorLocation();
	HoverTime = 0.0f;

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void ALootActor::ResetLoot()
{
	CurrentLootData = nullptr;
	LootMesh->SetStaticMesh(nullptr);
	
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void ALootActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
								UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
								bool bFromSweep, const FHitResult& SweepResult)
{
	// On vérifie que c'est bien le joueur (Character)
	if (OtherActor && OtherActor != this && OtherActor->IsA(ACharacter::StaticClass()))
	{
		PickUp(OtherActor);
	}
}

void ALootActor::PickUp(AActor* Picker)
{
	if (!CurrentLootData) return;

	// Jouer les effets
	if (CurrentLootData->PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CurrentLootData->PickupSound, GetActorLocation());
	}
	if (CurrentLootData->PickupVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CurrentLootData->PickupVFX, GetActorLocation());
	}

	// Ajouter les stats au joueur
	if (ACharacter* Character = Cast<ACharacter>(Picker))
	{
		if (APS_PlayerStateCustom* PS = Character->GetPlayerState<APS_PlayerStateCustom>())
		{
			switch (CurrentLootData->LootType)
			{
				case ELootType::Health:
					PS->AddHealth(CurrentLootData->Value);
					break;
				case ELootType::Ammo:
					PS->AddAmmo(FMath::RoundToInt(CurrentLootData->Value));
					break;
				case ELootType::Armor:
					PS->AddArmor(CurrentLootData->Value);
					break;
				case ELootType::XP:
					PS->AddXP(FMath::RoundToInt(CurrentLootData->Value));
					break;
				case ELootType::Currency:
					PS->AddCurrency(FMath::RoundToInt(CurrentLootData->Value));
					break;
				default:
					break;
			}
		}
	}

	FString Msg = FString::Printf(TEXT("Ramassé : %s (+%f)"), *CurrentLootData->LootName, CurrentLootData->Value);
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, Msg);

	// Retourner au pool
	if (ULootPoolSubsystem* PoolSys = GetWorld()->GetSubsystem<ULootPoolSubsystem>())
	{
		PoolSys->ReturnLoot(this);
	}
	else
	{
		Destroy(); // Au cas où le subsystem n'est pas dispo
	}
}
