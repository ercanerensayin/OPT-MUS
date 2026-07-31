// Copyright OPT-MUS. All Rights Reserved.

#include "WarframeCharacter.h"

#include "WarframeMovementComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

AWarframeCharacter::AWarframeCharacter(const FObjectInitializer& ObjectInitializer)
	// ACharacter'in varsayilan hareket bilesenini kendi turevimizle degistiriyoruz.
	// Bu, ek bir bilesen olusturmadigi icin en performansli ve GC dostu yoldur.
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UWarframeMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// Karakterin kendi Tick'ine ihtiyaci yok; tum is hareket bileseninde.
	PrimaryActorTick.bCanEverTick = false;

	// Onbelleklenmis tipli isaretci: her karede Cast<> maliyeti odenmez.
	WarframeMovement = Cast<UWarframeMovementComponent>(GetCharacterMovement());

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Karakter kameraya degil, hareket yonune doner.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (WarframeMovement)
	{
		WarframeMovement->bOrientRotationToMovement = true;
		WarframeMovement->bConstrainToPlane = false;
	}

	// Iskelet mesh'i kapsulun icine hizala (UE5 sablon degerleri).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = CameraBoomLength;
	CameraBoom->SocketOffset = CameraBoomOffset;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 18.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AWarframeCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Enhanced Input baglamini yalnizca yerel oyuncu icin ekle.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
			}
		}
	}
}

void AWarframeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWarframeCharacter::Input_Move);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWarframeCharacter::Input_Look);
	}

	if (JumpAction)
	{
		// Jump() sanal oldugu icin bu baglama Bullet Jump ezmesine de ulasir.
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	if (SprintAction)
	{
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AWarframeCharacter::Input_SprintStarted);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AWarframeCharacter::Input_SprintCompleted);
	}

	if (CrouchSlideAction)
	{
		EnhancedInput->BindAction(CrouchSlideAction, ETriggerEvent::Started, this, &AWarframeCharacter::Input_CrouchSlideStarted);
		EnhancedInput->BindAction(CrouchSlideAction, ETriggerEvent::Completed, this, &AWarframeCharacter::Input_CrouchSlideCompleted);
	}
}

void AWarframeCharacter::Jump()
{
	// Slide sirasindaysak ziplama yerine Bullet Jump.
	if (WarframeMovement && WarframeMovement->TryBulletJump())
	{
		return;
	}

	Super::Jump();
}

// ---------------------------------------------------------------------------
// Girdi islemcileri
// ---------------------------------------------------------------------------

void AWarframeCharacter::Input_Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D MoveInput = Value.Get<FVector2D>();
	if (MoveInput.IsNearlyZero())
	{
		return;
	}

	// Girdi kamera yonune gore cozulur (yalnizca yaw).
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), MoveInput.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), MoveInput.X);
}

void AWarframeCharacter::Input_Look(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D LookInput = Value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X * LookSensitivity);
	AddControllerPitchInput(LookInput.Y * LookSensitivity);
}

void AWarframeCharacter::Input_SprintStarted(const FInputActionValue& /*Value*/)
{
	if (WarframeMovement)
	{
		WarframeMovement->SetWantsToSprint(true);
	}
}

void AWarframeCharacter::Input_SprintCompleted(const FInputActionValue& /*Value*/)
{
	if (WarframeMovement)
	{
		WarframeMovement->SetWantsToSprint(false);
	}
}

void AWarframeCharacter::Input_CrouchSlideStarted(const FInputActionValue& /*Value*/)
{
	if (WarframeMovement)
	{
		// Sprint sirasinda slide, aksi halde normal crouch.
		WarframeMovement->SetWantsToSlide(true);
	}
}

void AWarframeCharacter::Input_CrouchSlideCompleted(const FInputActionValue& /*Value*/)
{
	if (WarframeMovement)
	{
		WarframeMovement->SetWantsToSlide(false);
	}
}
