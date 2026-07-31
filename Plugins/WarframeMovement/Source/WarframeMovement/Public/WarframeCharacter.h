// Copyright OPT-MUS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WarframeCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UWarframeMovementComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Warframe tarzi 3. sahis karakteri.
 *
 * Girdi -> hareket sozlesmesi:
 *   Sprint (basili tut)              -> UWarframeMovementComponent::SetWantsToSprint
 *   Crouch (sprint sirasinda basili) -> Slide (MOVE_Custom / CMOVE_Slide)
 *   Jump (slide sirasinda)           -> Bullet Jump (kamera yonune 3D firlatma)
 *
 * Tum alt bilesenler CreateDefaultSubobject ile olusturulur ve UPROPERTY ile
 * tutulur; boylece Unreal GC tarafindan izlenirler ve elle silme gerekmez.
 */
UCLASS(Blueprintable, ClassGroup = (Warframe))
class WARFRAMEMOVEMENT_API AWarframeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit AWarframeCharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin APawn/ACharacter interface
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	/** Slide sirasinda normal ziplama yerine Bullet Jump uygular. */
	virtual void Jump() override;
	//~ End interface

	/** Ozel hareket bilesenine tip guvenli erisim (cast maliyeti yok). */
	UFUNCTION(BlueprintPure, Category = "Movement")
	UWarframeMovementComponent* GetWarframeMovement() const { return WarframeMovement; }

	UFUNCTION(BlueprintPure, Category = "Movement")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "Movement")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:
	// ---------------------------------------------------------------------
	// Girdi islemcileri
	// ---------------------------------------------------------------------

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_SprintStarted(const FInputActionValue& Value);
	void Input_SprintCompleted(const FInputActionValue& Value);
	void Input_CrouchSlideStarted(const FInputActionValue& Value);
	void Input_CrouchSlideCompleted(const FInputActionValue& Value);

	// ---------------------------------------------------------------------
	// Girdi varliklari (Enhanced Input)
	// ---------------------------------------------------------------------

	/** Oyuncu ele gectiginde eklenen girdi eslestirme baglami. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr;

	/** Eslestirme baglaminin oncelik degeri. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Input")
	int32 MappingContextPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Input")
	TObjectPtr<UInputAction> MoveAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Input")
	TObjectPtr<UInputAction> LookAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Input")
	TObjectPtr<UInputAction> JumpAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Input")
	TObjectPtr<UInputAction> SprintAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Input")
	TObjectPtr<UInputAction> CrouchSlideAction = nullptr;

	// ---------------------------------------------------------------------
	// Kamera ayarlari
	// ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Camera", meta = (ClampMin = "0.0"))
	float CameraBoomLength = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Camera")
	FVector CameraBoomOffset = FVector(0.f, 60.f, 60.f);

	/** Fare/analog bakis hassasiyeti. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Camera", meta = (ClampMin = "0.0"))
	float LookSensitivity = 1.f;

private:
	/**
	 * Alt bilesenler: motor tarafindan olusturulup UPROPERTY ile tutulur.
	 * Bilincli olarak VisibleAnywhere/BlueprintReadOnly'dir - bir bilesen
	 * isaretcisini editorden veya Blueprint'ten degistirmek olusturulmus
	 * alt nesneyi sahipsiz birakir (dangling/sizinti riski). Ayarlanabilir
	 * olmasi gereken degerler yukaridaki EditAnywhere alanlarindadir.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	/** ACharacter::CharacterMovement'in onbelleklenmis, tipli hali. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWarframeMovementComponent> WarframeMovement = nullptr;
};
