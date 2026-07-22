#include "BP_Checkpoint.h"
#include "GM_ShooterGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"

ABP_Checkpoint::ABP_Checkpoint()
{
    PrimaryActorTick.bCanEverTick = false;

    // Trigger pour détecter le joueur
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    RootComponent = TriggerBox;

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABP_Checkpoint::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ABP_Checkpoint::OnOverlapEnd);

    // Widget pour le texte au-dessus du bouton
    ButtonWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ButtonWidget"));
    ButtonWidget->SetupAttachment(RootComponent);
    ButtonWidget->SetWidgetSpace(EWidgetSpace::Screen);
    ButtonWidget->SetDrawSize(FVector2D(200, 50));
    ButtonWidget->SetVisibility(false);

    bPlayerInTrigger = false;
}

void ABP_Checkpoint::BeginPlay()
{
    Super::BeginPlay();
}

void ABP_Checkpoint::OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
                                    AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp,
                                    int32 OtherBodyIndex,
                                    bool bFromSweep,
                                    const FHitResult & SweepResult)
{
    if (!OtherActor) return;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn) return;

    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;

    // Affiche le widget
    ButtonWidget->SetVisibility(true);
    bPlayerInTrigger = true;

    BP_CheckpointReached(PC);
}

void ABP_Checkpoint::OnOverlapEnd(UPrimitiveComponent* OverlappedComp,
                                  AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp,
                                  int32 OtherBodyIndex)
{
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn) return;

    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;

    // Cache le widget
    ButtonWidget->SetVisibility(false);
    bPlayerInTrigger = false;
}

void ABP_Checkpoint::BP_CheckpointReached(APlayerController* Player)
{
    if (!Player) return;

    AGM_ShooterGameMode* GM = Cast<AGM_ShooterGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    // Le checkpoint se déclenche automatiquement uniquement si pas encore verrouillé
    if (!GM->bCheckpointLocked)
    {
        // On peut décider ici si on ne verrouille pas encore, juste montrer le bouton
    }
}

void ABP_Checkpoint::InteractButton(APlayerController* Player)
{
    if (!bPlayerInTrigger || !Player) return;

    AGM_ShooterGameMode* GM = Cast<AGM_ShooterGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    if (!GM->bCheckpointLocked)
    {
        GM->LockCheckpoint();
        GM->GM_EndShooterPhase();
    }
}
