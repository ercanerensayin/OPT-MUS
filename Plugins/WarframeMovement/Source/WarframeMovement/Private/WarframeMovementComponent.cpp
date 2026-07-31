// Copyright OPT-MUS. All Rights Reserved.

#include "WarframeMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

namespace WarframeMovementConstants
{
	/** Zemin isini icin kapsul yari yuksekliginin carpani. */
	static constexpr float SlideSurfaceTraceScale = 1.5f;
}

UWarframeMovementComponent::UWarframeMovementComponent()
{
	bWantsToSprint = false;
	bWantsToSlide = false;
	bWantsToBulletJump = false;
	bBulletJumpConsumedInAir = false;
	bSlideInputConsumed = false;

	// Crouch/Slide icin gerekli.
	NavAgentProps.bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
	bUseSeparateBrakingFriction = true;

	// Hizli, cevik bir taban his.
	MaxWalkSpeed = 600.f;
	MaxWalkSpeedCrouched = 320.f;
	MaxAcceleration = 2400.f;
	BrakingDecelerationWalking = 1600.f;
	GroundFriction = 8.f;
	JumpZVelocity = 640.f;
	AirControl = 0.35f;
	RotationRate = FRotator(0.f, 720.f, 0.f);

	// Slide sirasinda kapsul yariya iner (BeginPlay'de gercek kapsulden yeniden hesaplanir).
	SetCrouchedHalfHeight(44.f);
}

void UWarframeMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// Slide bitiminde geri yuklemek uzere tasarim zamani surtunmesini sakla.
	DefaultGroundFriction = GroundFriction;

	if (const ACharacter* Char = CharacterOwner)
	{
		if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			DefaultCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		}
	}

	// "Kapsul yuksekligi yariya iner" gereksinimi: crouch yuksekligini kapsulden turet.
	SetCrouchedHalfHeight(DefaultCapsuleHalfHeight * SlideCapsuleHalfHeightScale);
}

// ---------------------------------------------------------------------------
// Durum sorgulari
// ---------------------------------------------------------------------------

bool UWarframeMovementComponent::IsCustomMovementMode(EWarframeCustomMovementMode Mode) const
{
	return (MovementMode == MOVE_Custom) && (CustomMovementMode == ToCustomMode(Mode));
}

bool UWarframeMovementComponent::IsSliding() const
{
	return IsCustomMovementMode(EWarframeCustomMovementMode::CMOVE_Slide);
}

bool UWarframeMovementComponent::IsSprinting() const
{
	return bWantsToSprint && Super::IsMovingOnGround() && !IsCrouching() && (Velocity.SizeSquared2D() > FMath::Square(1.f));
}

bool UWarframeMovementComponent::IsMovingOnGround() const
{
	// Slide bir "yerde" hareket modudur; animasyon, ziplama ve crouch mantigi
	// bunu bilmezse yanlis kararlar verir.
	return Super::IsMovingOnGround() || IsSliding();
}

bool UWarframeMovementComponent::CanCrouchInCurrentState() const
{
	// IsMovingOnGround() slide sirasinda da true dondugu icin taban uygulama
	// yeterlidir; yine de niyeti acikca ifade ediyoruz.
	return Super::CanCrouchInCurrentState()
		|| (NavAgentProps.bCanCrouch && IsSliding() && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics());
}

float UWarframeMovementComponent::GetMaxSpeed() const
{
	if (IsSliding())
	{
		return MaxSlideSpeed;
	}

	if (bWantsToSprint && Super::IsMovingOnGround() && !IsCrouching())
	{
		return SprintMaxSpeed;
	}

	return Super::GetMaxSpeed();
}

float UWarframeMovementComponent::GetMaxBrakingDeceleration() const
{
	// Slide'da frenleme, PhysSlide icindeki ussel sonumleme ile yapilir.
	return IsSliding() ? 0.f : Super::GetMaxBrakingDeceleration();
}

// ---------------------------------------------------------------------------
// Girdi arayuzu
// ---------------------------------------------------------------------------

void UWarframeMovementComponent::SetWantsToSprint(bool bNewWantsToSprint)
{
	bWantsToSprint = bNewWantsToSprint;
}

void UWarframeMovementComponent::SetWantsToSlide(bool bNewWantsToSlide)
{
	// Yalnizca niyeti kaydeder; slide/crouch karari
	// UpdateCharacterStateBeforeMovement icinde verilir. Boylece sunucu,
	// sikistirilmis bayraklardan gelen ayni niyetle ayni sonuca ulasir.
	bWantsToSlide = bNewWantsToSlide;
}

bool UWarframeMovementComponent::TryBulletJump()
{
	if (!CharacterOwner)
	{
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		if ((World->GetTimeSeconds() - LastBulletJumpTime) < BulletJumpCooldown)
		{
			return false;
		}
	}

	const bool bFromSlide = IsSliding();
	const bool bFromAir = bAllowBulletJumpWhileFalling && IsFalling() && !bBulletJumpConsumedInAir;

	if (!bFromSlide && !bFromAir)
	{
		return false;
	}

	// Fiili firlatma, prediction icinde (UpdateCharacterStateBeforeMovement) uygulanir.
	bWantsToBulletJump = true;
	return true;
}

// ---------------------------------------------------------------------------
// Durum makinesi
// ---------------------------------------------------------------------------

bool UWarframeMovementComponent::CanSlide() const
{
	if (!CharacterOwner || !UpdatedComponent || IsSliding())
	{
		return false;
	}

	// Ayni crouch basisi ile ikinci kez slide yok (tus basili tutulurken
	// slide'in kendini surekli yeniden tetiklemesini engeller).
	if (bSlideInputConsumed)
	{
		return false;
	}

	if (!Super::IsMovingOnGround())
	{
		return false;
	}

	if (bRequireSprintToSlide && !bWantsToSprint)
	{
		return false;
	}

	if (Velocity.SizeSquared2D() < FMath::Square(MinSlideEntrySpeed))
	{
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		if ((World->GetTimeSeconds() - LastSlideEndTime) < SlideCooldown)
		{
			return false;
		}
	}

	return true;
}

bool UWarframeMovementComponent::TryEnterSlide()
{
	if (!CanSlide())
	{
		return false;
	}

	FVector SlideDir = Velocity.GetSafeNormal2D();
	if (SlideDir.IsNearlyZero())
	{
		SlideDir = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	}
	if (SlideDir.IsNearlyZero())
	{
		return false;
	}

	// "Yuksek ivmeli" giris: mevcut momentumu olcekle, taban itmenin altina dusme.
	const float EntrySpeed = FMath::Max(Velocity.Size2D() * SlideEntryMomentumScale, SlideEntryImpulse);
	Velocity = SlideDir * FMath::Min(EntrySpeed, MaxSlideSpeed);

	SetMovementMode(MOVE_Custom, ToCustomMode(EWarframeCustomMovementMode::CMOVE_Slide));
	return IsSliding();
}

void UWarframeMovementComponent::ExitSlide()
{
	if (!IsSliding())
	{
		return;
	}

	FHitResult Surface;
	SetMovementMode(FindSlideSurface(Surface) ? MOVE_Walking : MOVE_Falling);
}

void UWarframeMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// 1) Bullet jump, hareket cozumlemesinden once uygulanmali ki firlatma
	//    hizi ayni kare icinde islensin.
	if (bWantsToBulletJump)
	{
		bWantsToBulletJump = false;
		PerformBulletJump();
	}

	// 2) Crouch tusu birakildiginda yeni bir slide hakki dogar.
	if (!bWantsToSlide)
	{
		bSlideInputConsumed = false;
	}

	// 3) Slide giris/cikis niyeti.
	if (bWantsToSlide && !IsSliding())
	{
		TryEnterSlide();
	}
	else if (!bWantsToSlide && IsSliding())
	{
		ExitSlide();
	}

	// 4) Slide sartlari saglanmiyorsa crouch tusu normal crouch gibi davranir.
	//    Niyetten turetildigi icin istemci ve sunucu ayni sonucu uretir.
	if (!IsSliding())
	{
		bWantsToCrouch = bWantsToSlide;
	}

	// Super, bWantsToCrouch'a gore Crouch()/UnCrouch() cagirir; kapsul boyutlandirmasi
	// ve ayaga kalkarken yapilan sikisma (encroachment) testi motor tarafinda kalir.
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UWarframeMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	const bool bWasSliding = (PreviousMovementMode == MOVE_Custom)
		&& (PreviousCustomMode == ToCustomMode(EWarframeCustomMovementMode::CMOVE_Slide));

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	const bool bIsSlidingNow = IsSliding();

	if (!bWasSliding && bIsSlidingNow)
	{
		// Giris: surtunmeyi dusur, kapsulu yariya indir, sayaci sifirla.
		GroundFriction = SlideFriction;
		bWantsToCrouch = true;
		SlideElapsedTime = 0.f;

		OnSlideStateChanged.Broadcast(true);
	}
	else if (bWasSliding && !bIsSlidingNow)
	{
		// Cikis: her seyi tasarim zamani degerlerine geri yukle.
		GroundFriction = DefaultGroundFriction;
		bWantsToCrouch = bWantsToSlide; // crouch tusu hala basiliysa cokme devam etsin
		SlideElapsedTime = 0.f;
		bSlideInputConsumed = true;    // yeni slide icin tusa tekrar basilmali

		if (const UWorld* World = GetWorld())
		{
			LastSlideEndTime = World->GetTimeSeconds();
		}

		OnSlideStateChanged.Broadcast(false);
	}

	// Yere basildiginda havadaki bullet jump hakki tazelenir.
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || bIsSlidingNow)
	{
		bBulletJumpConsumedInAir = false;
	}
}

// ---------------------------------------------------------------------------
// Slide fizigi
// ---------------------------------------------------------------------------

void UWarframeMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	// Taban uygulama yalnizca Blueprint olayini tetikler; once cagirmak,
	// PhysSlide icinde mod degistigimizde yanlis bildirim gonderilmesini onler.
	Super::PhysCustom(DeltaTime, Iterations);

	switch (static_cast<EWarframeCustomMovementMode>(CustomMovementMode))
	{
	case EWarframeCustomMovementMode::CMOVE_Slide:
		PhysSlide(DeltaTime, Iterations);
		break;

	default:
		break;
	}
}

bool UWarframeMovementComponent::FindSlideSurface(FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World || !CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + FVector::DownVector *
		(Capsule->GetScaledCapsuleHalfHeight() * WarframeMovementConstants::SlideSurfaceTraceScale);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WarframeFindSlideSurface), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(QueryParams, ResponseParams);
	QueryParams.AddIgnoredActor(CharacterOwner);

	return World->LineTraceSingleByChannel(
		OutHit, Start, End, UpdatedComponent->GetCollisionObjectType(), QueryParams, ResponseParams);
}

void UWarframeMovementComponent::PhysSlide(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController
		&& !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity()
		&& (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	// --- Cikis kosullari (hareketten once: kalan sureyi normal fizige devret) ---
	FHitResult SurfaceHit;
	const bool bHasSurface = FindSlideSurface(SurfaceHit);
	const bool bTimedOut = (MaxSlideDuration > 0.f) && (SlideElapsedTime >= MaxSlideDuration);
	const bool bTooSlow = Velocity.SizeSquared() < FMath::Square(MinSlideSpeed);

	if (!bHasSurface || bTimedOut || bTooSlow)
	{
		// Zemin sonucu zaten elimizde; ExitSlide() cagirip isini tekrarlamiyoruz.
		SetMovementMode(bHasSurface ? MOVE_Walking : MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	SlideElapsedTime += DeltaTime;
	++Iterations;
	bJustTeleported = false;

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
	const FVector SurfaceNormal = SurfaceHit.ImpactNormal;

	// 1) Egim yercekimi: dunya yercekimini kayma duzlemine izdusur.
	//    Yokus asagi hizlandirir, yokus yukari yavaslatir.
	const FVector GravityAcceleration = FVector(0.f, 0.f, GetGravityZ()) * SlideGravityScale;
	Velocity += FVector::VectorPlaneProject(GravityAcceleration, SurfaceNormal) * DeltaTime;

	// 2) Sinirli yon kontrolu (steering) - girdi ivmesi kayma duzlemine izdusurulur.
	if (SlideSteeringAcceleration > 0.f && !Acceleration.IsNearlyZero())
	{
		const FVector SteerDirection =
			FVector::VectorPlaneProject(Acceleration.GetSafeNormal(), SurfaceNormal).GetSafeNormal();
		Velocity += SteerDirection * SlideSteeringAcceleration * DeltaTime;
	}

	// 3) Surtunme / sonumleme (decay): kare adimindan bagimsiz ussel azalma.
	//    v(t) = v0 * e^(-SlideFriction * t)
	Velocity *= FMath::Exp(-SlideFriction * DeltaTime);

	// 4) Hizi kayma duzlemine kilitle ve ust sinira kirp.
	Velocity = FVector::VectorPlaneProject(Velocity, SurfaceNormal).GetClampedToMaxSize(MaxSlideSpeed);

	ApplyRootMotionToVelocity(DeltaTime);

	// 5) Hareket + carpisma cozumlemesi.
	const FVector Delta = Velocity * DeltaTime;
	FHitResult MoveHit(1.f);
	SafeMoveUpdatedComponent(Delta, OldRotation, /*bSweep=*/true, MoveHit);

	if (MoveHit.Time < 1.f)
	{
		HandleImpact(MoveHit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - MoveHit.Time, MoveHit.Normal, MoveHit, /*bHandleImpact=*/true);
	}

	// 6) Zemine yapisik kal (basamak ve egim gecisleri icin).
	FFindFloorResult FloorResult;
	FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);

	if (FloorResult.IsWalkableFloor())
	{
		CurrentFloor = FloorResult;
		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);
	}
	else
	{
		// Ucurumdan ciktik: bu kare icin sure tuketildi, dususe gec.
		SetMovementMode(MOVE_Falling);
		return;
	}

	// 7) Hizi gercek yer degistirmeden turet (duvar/rampa carpismalarini yansitir).
	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

// ---------------------------------------------------------------------------
// Bullet Jump
// ---------------------------------------------------------------------------

FVector UWarframeMovementComponent::ComputeBulletJumpVelocity() const
{
	if (!CharacterOwner)
	{
		return FVector::ZeroVector;
	}

	// Kamera (kontrol) yonu; kontrolcu yoksa aktor yonu.
	FVector AimDirection = CharacterOwner->GetActorForwardVector();
	if (const AController* OwnerController = CharacterOwner->GetController())
	{
		AimDirection = OwnerController->GetControlRotation().Vector();
	}
	AimDirection = AimDirection.GetSafeNormal();

	FVector HorizontalDirection = AimDirection.GetSafeNormal2D();
	if (HorizontalDirection.IsNearlyZero())
	{
		HorizontalDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	}
	if (HorizontalDirection.IsNearlyZero())
	{
		HorizontalDirection = FVector::ForwardVector;
	}

	// Dikey aciyi sinirla: asagi bakarken bile yukari dogru bir yay olusur.
	const float MinPitch = FMath::Min(MinBulletJumpPitch, MaxBulletJumpPitch);
	const float MaxPitch = FMath::Max(MinBulletJumpPitch, MaxBulletJumpPitch);
	const float AimPitchDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(AimDirection.Z, -1.f, 1.f)));
	const float PitchRad = FMath::DegreesToRadians(FMath::Clamp(AimPitchDeg, MinPitch, MaxPitch));

	// Capraz (3D) firlatma vektoru.
	const FVector LaunchDirection =
		(HorizontalDirection * FMath::Cos(PitchRad) + FVector::UpVector * FMath::Sin(PitchRad)).GetSafeNormal();

	// Slide momentumunun bir kismi korunur - hizli girisin odulu.
	return LaunchDirection * BulletJumpImpulse + Velocity * BulletJumpMomentumRetention;
}

void UWarframeMovementComponent::PerformBulletJump()
{
	if (!CharacterOwner)
	{
		return;
	}

	const FVector LaunchVelocity = ComputeBulletJumpVelocity();
	if (LaunchVelocity.IsNearlyZero())
	{
		return;
	}

	const bool bWasFalling = IsFalling();

	// Slide'dan cikis: surtunme geri yuklenir, kapsul ayaga kaldirilir.
	if (IsSliding())
	{
		SetMovementMode(MOVE_Falling);
	}

	// LaunchCharacter, hizi PendingLaunchVelocity olarak kuyruga alir ve
	// HandlePendingLaunch icinde ayni hareket guncellemesinde uygulanir;
	// bu sayede client-side prediction ile uyumlu kalir.
	CharacterOwner->LaunchCharacter(LaunchVelocity, /*bXYOverride=*/true, /*bZOverride=*/true);

	if (const UWorld* World = GetWorld())
	{
		LastBulletJumpTime = World->GetTimeSeconds();
	}

	bBulletJumpConsumedInAir = bWasFalling;

	OnBulletJump.Broadcast(LaunchVelocity);
}

// ---------------------------------------------------------------------------
// Ag (network) tarafi - client-side prediction
// ---------------------------------------------------------------------------

void UWarframeMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint     = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToSlide      = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsToBulletJump = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
}

FNetworkPredictionData_Client* UWarframeMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		// Not: ClientPredictionData'nin sahipligi taban sinifta olup yikicida
		// silinir; burada elle delete edilmemeli (cift serbest birakma olur).
		UWarframeMovementComponent* MutableThis = const_cast<UWarframeMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Warframe(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}

// ---------------------------------------------------------------------------
// FSavedMove_Warframe
// ---------------------------------------------------------------------------

FSavedMove_Warframe::FSavedMove_Warframe()
	: bSavedWantsToSprint(0)
	, bSavedWantsToSlide(0)
	, bSavedWantsToBulletJump(0)
	, bSavedSlideInputConsumed(0)
{
}

void FSavedMove_Warframe::Clear()
{
	Super::Clear();

	bSavedWantsToSprint = 0;
	bSavedWantsToSlide = 0;
	bSavedWantsToBulletJump = 0;
	bSavedSlideInputConsumed = 0;
	SavedSlideElapsedTime = 0.f;
}

uint8 FSavedMove_Warframe::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedWantsToSprint)     { Result |= FLAG_Custom_0; }
	if (bSavedWantsToSlide)      { Result |= FLAG_Custom_1; }
	if (bSavedWantsToBulletJump) { Result |= FLAG_Custom_2; }

	return Result;
}

bool FSavedMove_Warframe::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Warframe* Other = static_cast<const FSavedMove_Warframe*>(NewMove.Get());
	if (!Other)
	{
		return false;
	}

	// Girdi niyeti degistiyse hamleler birlestirilemez, aksi halde sunucu
	// tekrar oynatirken farkli sonuc uretir.
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint
		|| bSavedWantsToSlide != Other->bSavedWantsToSlide
		|| bSavedWantsToBulletJump != Other->bSavedWantsToBulletJump
		|| bSavedSlideInputConsumed != Other->bSavedSlideInputConsumed)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Warframe::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UWarframeMovementComponent* MoveComp =
		C ? Cast<UWarframeMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		bSavedWantsToSprint      = MoveComp->bWantsToSprint;
		bSavedWantsToSlide       = MoveComp->bWantsToSlide;
		bSavedWantsToBulletJump  = MoveComp->bWantsToBulletJump;
		bSavedSlideInputConsumed = MoveComp->bSlideInputConsumed;
		SavedSlideElapsedTime    = MoveComp->SlideElapsedTime;
	}
}

void FSavedMove_Warframe::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UWarframeMovementComponent* MoveComp =
		C ? Cast<UWarframeMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->bWantsToSprint      = bSavedWantsToSprint;
		MoveComp->bWantsToSlide       = bSavedWantsToSlide;
		MoveComp->bWantsToBulletJump  = bSavedWantsToBulletJump;
		MoveComp->bSlideInputConsumed = bSavedSlideInputConsumed;
		MoveComp->SlideElapsedTime    = SavedSlideElapsedTime;
	}
}

// ---------------------------------------------------------------------------
// FNetworkPredictionData_Client_Warframe
// ---------------------------------------------------------------------------

FNetworkPredictionData_Client_Warframe::FNetworkPredictionData_Client_Warframe(
	const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_Warframe::AllocateNewMove()
{
	// FSavedMovePtr paylasimli isaretcidir; bellek yonetimi motor havuzunda kalir.
	return MakeShared<FSavedMove_Warframe>();
}
