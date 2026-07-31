// Copyright OPT-MUS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WarframeMovementTypes.generated.h"

/**
 * MOVE_Custom altinda kullanilan alt hareket modlari.
 * UCharacterMovementComponent::CustomMovementMode bir uint8 oldugu icin
 * deger araligi 0-255 ile sinirlidir ve cast edilerek kullanilir.
 */
UENUM(BlueprintType)
enum class EWarframeCustomMovementMode : uint8
{
	/** Kullanilmiyor - CustomMovementMode'un varsayilan degeri. */
	CMOVE_None			= 0		UMETA(Hidden),

	/** Yerde yuksek hizli kayma (Slide). */
	CMOVE_Slide			= 1		UMETA(DisplayName = "Slide"),

	CMOVE_MAX			= 255	UMETA(Hidden)
};

/** SetMovementMode(MOVE_Custom, ...) cagrilari icin kisa yardimci. */
FORCEINLINE uint8 ToCustomMode(EWarframeCustomMovementMode Mode)
{
	return static_cast<uint8>(Mode);
}
