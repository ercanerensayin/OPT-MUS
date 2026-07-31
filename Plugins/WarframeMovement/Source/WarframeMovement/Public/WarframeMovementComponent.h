// Copyright OPT-MUS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WarframeMovementTypes.h"
#include "WarframeMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarframeSlideStateChanged, bool, bIsSliding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarframeBulletJump, FVector, LaunchVelocity);

/**
 * Warframe tarzi hareket bileseni.
 *
 * Ozellikler:
 *  - Sprint (basili tutulan hiz modu)
 *  - Slide: sprint sirasinda crouch ile tetiklenen, dusuk surtunmeli, kapsulu
 *    yariya indiren, hizi zamanla sonumlenen ozel bir hareket modu (MOVE_Custom).
 *  - Bullet Jump: slide sirasinda jump ile kamera yonune uygulanan 3D firlatma.
 *
 * Ag (network) tarafi: tum girdi niyetleri sikistirilmis bayraklar (compressed
 * flags) uzerinden sunucuya tasinir, boylece client-side prediction bozulmaz.
 */
UCLASS(ClassGroup = (Warframe), meta = (BlueprintSpawnableComponent))
class WARFRAMEMOVEMENT_API UWarframeMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_Warframe;

public:
	UWarframeMovementComponent();

	//~ Begin UMovementComponent / UCharacterMovementComponent interface
	virtual void BeginPlay() override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual bool IsMovingOnGround() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	//~ End interface

	// ---------------------------------------------------------------------
	// Girdi arayuzu (Character tarafindan cagrilir)
	// ---------------------------------------------------------------------

	/** Sprint niyetini ayarlar. Gercek hiz degisimi GetMaxSpeed uzerinden olur. */
	UFUNCTION(BlueprintCallable, Category = "Movement|Sprint")
	void SetWantsToSprint(bool bNewWantsToSprint);

	/** Crouch tusuna basildiginda cagrilir; sartlar uygunsa slide baslatir. */
	UFUNCTION(BlueprintCallable, Category = "Movement|Slide")
	void SetWantsToSlide(bool bNewWantsToSlide);

	/**
	 * Slide sirasinda jump girdisi. Sartlar uygunsa bullet jump niyetini kaydeder
	 * ve bir sonraki hareket guncellemesinde firlatmayi uygular.
	 * @return Bullet jump tetiklendiyse true (bu durumda normal ziplama atlanmali).
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement|BulletJump")
	bool TryBulletJump();

	// ---------------------------------------------------------------------
	// Durum sorgulari
	// ---------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Movement|Slide")
	bool IsSliding() const;

	UFUNCTION(BlueprintPure, Category = "Movement|Sprint")
	bool IsSprinting() const;

	/** Slide sartlarini saglayip saglamadigimizi test eder (UI/animasyon icin de kullanilabilir). */
	UFUNCTION(BlueprintPure, Category = "Movement|Slide")
	bool CanSlide() const;

	/** true ise slide yalnizca sprint sirasinda baslatilabilir. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	bool bRequireSprintToSlide = true;

	// ---------------------------------------------------------------------
	// Olaylar (VFX / ses / animasyon icin)
	// ---------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Movement|Slide")
	FOnWarframeSlideStateChanged OnSlideStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Movement|BulletJump")
	FOnWarframeBulletJump OnBulletJump;

	// ---------------------------------------------------------------------
	// Sprint parametreleri
	// ---------------------------------------------------------------------

	/** Sprint sirasindaki maksimum yatay hiz (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Sprint", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SprintMaxSpeed = 900.f;

	// ---------------------------------------------------------------------
	// Slide parametreleri
	// ---------------------------------------------------------------------

	/** Slide'a girebilmek icin gereken minimum yatay hiz (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinSlideEntrySpeed = 400.f;

	/** Slide'a girerken uygulanan taban hiz - "yuksek ivmeli" giris (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SlideEntryImpulse = 1600.f;

	/** Girisdeki mevcut hizin ne kadarinin korunacagi (1.0 = tamami). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float SlideEntryMomentumScale = 1.35f;

	/** Slide sirasinda izin verilen maksimum hiz (cm/s). Yokus asagi hizlanmayi sinirlar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxSlideSpeed = 2200.f;

	/** Bu hizin altina dusuldugunde slide sonlanir (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinSlideSpeed = 350.f;

	/**
	 * Slide sirasindaki surtunme katsayisi. Hem ussel hiz sonumlemesinde
	 * (v *= e^(-SlideFriction * dt)) hem de GroundFriction degeri olarak kullanilir.
	 * Varsayilan yurume surtunmesi (~8.0) yerine cok dusuk bir deger beklenir.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "8.0"))
	float SlideFriction = 0.55f;

	/** Slide sirasinda egime bagli yercekimi ivmesinin carpani. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float SlideGravityScale = 2.0f;

	/** Slide sirasinda yon degistirme (steering) ivmesi (cm/s^2). 0 = tam kilitli yon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SlideSteeringAcceleration = 900.f;

	/** Slide'in maksimum suresi (sn). 0 = sinirsiz (yalnizca hiz esigi sonlandirir). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxSlideDuration = 1.6f;

	/** Slide sirasinda kapsul yuksekligi carpani (0.5 = yariya iner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float SlideCapsuleHalfHeightScale = 0.5f;

	/** Slide bittikten sonra tekrar slide'a girebilmek icin beklenen sure (sn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SlideCooldown = 0.15f;

	// ---------------------------------------------------------------------
	// Bullet Jump parametreleri
	// ---------------------------------------------------------------------

	/** Firlatma hizinin buyuklugu (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|BulletJump", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BulletJumpImpulse = 1900.f;

	/** Slide hizinin ne kadarinin firlatmaya eklenecegi (momentum korunumu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|BulletJump", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float BulletJumpMomentumRetention = 0.35f;

	/** Kamera cok asagi bakiyorsa bile uygulanan minimum firlatma acisi (derece). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|BulletJump", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MinBulletJumpPitch = 22.f;

	/** Dikey firlatmayi sinirlayan maksimum aci (derece). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|BulletJump", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MaxBulletJumpPitch = 75.f;

	/** Iki bullet jump arasindaki minimum sure (sn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|BulletJump", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BulletJumpCooldown = 0.35f;

	/** true ise bullet jump havada da (slide olmadan) bir kez kullanilabilir. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|BulletJump")
	bool bAllowBulletJumpWhileFalling = false;

protected:
	/** Slide fizigi - PhysCustom icinden CMOVE_Slide icin cagrilir. */
	void PhysSlide(float DeltaTime, int32 Iterations);

	/** Slide zemini icin capsule altina isin atar. */
	bool FindSlideSurface(FHitResult& OutHit) const;

	/** Slide'a girmeyi dener; basarili olursa MOVE_Custom/CMOVE_Slide'a gecer. */
	bool TryEnterSlide();

	/** Slide'dan cikip verilen moda gecer (varsayilan: yerdeyse walking, degilse falling). */
	void ExitSlide();

	/** Kamera/kontrol rotasyonundan firlatma hizini hesaplar. */
	FVector ComputeBulletJumpVelocity() const;

	/** Bullet jump'i fiilen uygular (prediction icinde, hareket guncellemesinden once). */
	void PerformBulletJump();

	/** CustomMovementMode karsilastirmasi icin kisayol. */
	bool IsCustomMovementMode(EWarframeCustomMovementMode Mode) const;

	//~ Sikistirilmis bayraklardan girdi niyetlerini geri yukler (sunucu / replay).
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	// ---------------------------------------------------------------------
	// Calisma zamani durumu (replike edilmez; prediction ile tasinir)
	// ---------------------------------------------------------------------

	/** Sprint tusu basili mi. */
	uint8 bWantsToSprint : 1;

	/** Crouch tusu basili mi (slide niyeti). */
	uint8 bWantsToSlide : 1;

	/** Bir sonraki guncellemede bullet jump uygulanacak mi. */
	uint8 bWantsToBulletJump : 1;

	/** Havada bullet jump hakkinin kullanilip kullanilmadigi. */
	uint8 bBulletJumpConsumedInAir : 1;

	/**
	 * Bu crouch basisi ile bir slide zaten yapildi mi.
	 * Tus basili tutuldugu surece slide'in kendini tekrar tetiklemesini engeller;
	 * yeni bir slide icin tusun birakilip tekrar basilmasi gerekir.
	 */
	uint8 bSlideInputConsumed : 1;

	/** Mevcut slide'in gecen suresi (sn). */
	float SlideElapsedTime = 0.f;

	/** Son slide'in bitis zamani (World time, sn). */
	float LastSlideEndTime = -1.e30f;

	/** Son bullet jump zamani (World time, sn). */
	float LastBulletJumpTime = -1.e30f;

private:
	/** Slide sirasinda degistirilen GroundFriction'i geri yuklemek icin saklanir. */
	float DefaultGroundFriction = 8.f;

	/** Kapsulun tasarim zamanindaki (CDO) yari yuksekligi. */
	float DefaultCapsuleHalfHeight = 88.f;
};

/**
 * Warframe hareketleri icin kaydedilmis hamle (client-side prediction).
 * Girdi niyetlerini ve slide zamanlayicisini saklayarak sunucu tarafinda
 * birebir yeniden oynatmayi (replay) mumkun kilar.
 */
class WARFRAMEMOVEMENT_API FSavedMove_Warframe : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	FSavedMove_Warframe();

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToSlide : 1;
	uint8 bSavedWantsToBulletJump : 1;

	/** Girdi bayragi degil, turetilmis durum: duzeltme sonrasi replay icin saklanir. */
	uint8 bSavedSlideInputConsumed : 1;

	float SavedSlideElapsedTime = 0.f;
};

/** Kaydedilmis hamleleri uretmekten sorumlu client prediction verisi. */
class WARFRAMEMOVEMENT_API FNetworkPredictionData_Client_Warframe : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Warframe(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
