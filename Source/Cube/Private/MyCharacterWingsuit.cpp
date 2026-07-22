#include "MyCharacterWingsuit.h"
#include "GameFramework/CharacterMovementComponent.h"

AMyCharacterWingsuit::AMyCharacterWingsuit()
{
    PrimaryActorTick.bCanEverTick = true;

    // Camera setup
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 300.f;
    SpringArm->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    // Defaults
    bIsWingsuitActive = false;
    GlideGravityScale = 0.3f;
    GlideForwardForce = 600.f;

    bIsThirdPerson = true;
    CameraDistances = {300.f, 500.f, 800.f};
    CurrentCameraDistanceIndex = 0;
}

void AMyCharacterWingsuit::BeginPlay()
{
    Super::BeginPlay();
    UpdateCamera();
}

void AMyCharacterWingsuit::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    HandleWingsuit(DeltaTime);
}

// ================= INPUTS =================
void AMyCharacterWingsuit::MoveForward(float Value)
{
    if (Controller && Value != 0.f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void AMyCharacterWingsuit::MoveRight(float Value)
{
    if (Controller && Value != 0.f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        AddMovementInput(Direction, Value);
    }
}

void AMyCharacterWingsuit::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void AMyCharacterWingsuit::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

// ================= TOGGLES =================
void AMyCharacterWingsuit::ToggleWingsuit()
{
    bIsWingsuitActive = !bIsWingsuitActive;

    if (bIsWingsuitActive)
    {
        GetCharacterMovement()->GravityScale = GlideGravityScale;
    }
    else
    {
        GetCharacterMovement()->GravityScale = 1.0f;
    }
}

void AMyCharacterWingsuit::ToggleCameraMode()
{
    bIsThirdPerson = !bIsThirdPerson;
    UpdateCamera();
}

void AMyCharacterWingsuit::CycleCameraDistance()
{
    if (CameraDistances.Num() > 0)
    {
        CurrentCameraDistanceIndex = (CurrentCameraDistanceIndex + 1) % CameraDistances.Num();
        UpdateCamera();
    }
}

// ================= PRIVATE =================
void AMyCharacterWingsuit::UpdateCamera()
{
    if (bIsThirdPerson)
    {
        SpringArm->TargetArmLength = CameraDistances.IsValidIndex(CurrentCameraDistanceIndex)
            ? CameraDistances[CurrentCameraDistanceIndex]
            : 300.f;
        Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    }
    else
    {
        SpringArm->TargetArmLength = 0.f;
        Camera->SetupAttachment(RootComponent);
    }
}

void AMyCharacterWingsuit::HandleWingsuit(float DeltaTime)
{
    if (bIsWingsuitActive && !GetCharacterMovement()->IsMovingOnGround())
    {
        FVector ForwardForce = GetActorForwardVector() * GlideForwardForce * DeltaTime;
        GetCharacterMovement()->AddForce(ForwardForce);
    }
}
