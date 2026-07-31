// Copyright OPT-MUS. All Rights Reserved.

using System;
using UnityEngine;

namespace OptMus.WarframeMovement
{
    /// <summary>
    /// Hareket durumlari. UE tarafindaki EMovementMode + CMOVE_Slide karsiligi.
    /// </summary>
    public enum WarframeMovementMode
    {
        /// <summary>Yerde yuruyor / kosuyor / comelmis.</summary>
        Walking = 0,

        /// <summary>Havada (ziplama, dusus, bullet jump).</summary>
        Falling = 1,

        /// <summary>Yerde yuksek hizli kayma.</summary>
        Sliding = 2
    }

    /// <summary>
    /// Yer ve hava hareketi ayarlari. Birimler metre / saniye (Unity olcegi).
    /// </summary>
    [Serializable]
    public sealed class GroundMovementSettings
    {
        [Header("Hizlar (m/s)")]
        [Tooltip("Normal yurume hizi.")]
        [Min(0f)] public float WalkSpeed = 6f;

        [Tooltip("Sprint hizi.")]
        [Min(0f)] public float SprintSpeed = 9f;

        [Tooltip("Comelmis halde yurume hizi.")]
        [Min(0f)] public float CrouchSpeed = 3.2f;

        [Header("Ivme")]
        [Tooltip("Hedef hiza ulasma ivmesi (m/s^2).")]
        [Min(0f)] public float Acceleration = 24f;

        [Tooltip("Girdi yokken yavaslama ivmesi (m/s^2).")]
        [Min(0f)] public float Deceleration = 16f;

        [Tooltip("Havadayken girdinin etkisi (0 = kontrol yok, 1 = yerdeki kadar).")]
        [Range(0f, 1f)] public float AirControl = 0.35f;

        [Header("Yercekimi ve ziplama")]
        [Tooltip("Yercekimi ivmesi (negatif olmali).")]
        public float Gravity = -25f;

        [Tooltip("Maksimum dusus hizi (m/s).")]
        [Min(0f)] public float TerminalVelocity = 60f;

        [Tooltip("Ziplama yuksekligi (m). Dikey hiz buradan turetilir.")]
        [Min(0f)] public float JumpHeight = 1.4f;

        [Tooltip("Zeminden ayrildiktan sonra ziplamaya izin verilen sure (sn).")]
        [Min(0f)] public float CoyoteTime = 0.1f;

        [Tooltip("Karakteri zemine yapisik tutan asagi yonlu hiz (m/s). Rampa ve merdivenlerde zipla-zipla yurumeyi onler.")]
        [Min(0f)] public float StickToGroundSpeed = 2f;

        [Header("Donus")]
        [Tooltip("Acik ise karakter hareket yonune doner (UE: bOrientRotationToMovement).")]
        public bool OrientRotationToMovement = true;

        [Tooltip("Donus hizi (derece/sn).")]
        [Min(0f)] public float RotationSpeed = 720f;

        [Header("Algilama")]
        [Tooltip("Zemin sayilan katmanlar.")]
        public LayerMask GroundMask = ~0;

        [Tooltip("Ayaga kalkarken tepe bosluk testi icin kullanilan katmanlar.")]
        public LayerMask HeadroomMask = ~0;

        [Tooltip("Zemin isini mesafesi (m).")]
        [Min(0.01f)] public float GroundProbeDistance = 0.2f;
    }

    /// <summary>
    /// Kayma (Slide) ayarlari.
    /// </summary>
    [Serializable]
    public sealed class SlideSettings
    {
        [Tooltip("Acik ise slide yalnizca sprint sirasinda baslatilabilir.")]
        public bool RequireSprint = true;

        [Header("Giris")]
        [Tooltip("Slide'a girebilmek icin gereken minimum yatay hiz (m/s).")]
        [Min(0f)] public float MinEntrySpeed = 4f;

        [Tooltip("Girişte uygulanan taban hiz - 'yuksek ivmeli' giris (m/s).")]
        [Min(0f)] public float EntryImpulse = 16f;

        [Tooltip("Girişte mevcut hizin carpani. 1 = hiz aynen korunur.")]
        [Min(0f)] public float EntryMomentumScale = 1.35f;

        [Tooltip("Ard arda slide icin beklenmesi gereken sure (sn).")]
        [Min(0f)] public float Cooldown = 0.15f;

        [Header("Fizik")]
        [Tooltip("Slide sirasinda izin verilen maksimum hiz (m/s).")]
        [Min(0f)] public float MaxSpeed = 22f;

        [Tooltip("Bu hizin altina dusuldugunde slide sonlanir (m/s).")]
        [Min(0f)] public float MinSpeed = 3.5f;

        [Tooltip("Surtunme katsayisi. Sonumleme: v *= e^(-Friction * dt). Dusuk deger = uzun kayma.")]
        [Range(0f, 8f)] public float Friction = 0.55f;

        [Tooltip("Egime bagli yercekimi carpani. Yokus asagi hizlanmayi belirler.")]
        [Range(0f, 5f)] public float GravityScale = 2f;

        [Tooltip("Slide sirasinda yon degistirme ivmesi (m/s^2). 0 = yon kilitli.")]
        [Min(0f)] public float SteeringAcceleration = 9f;

        [Tooltip("Maksimum slide suresi (sn). 0 = sinirsiz.")]
        [Min(0f)] public float MaxDuration = 1.6f;

        [Header("Kapsul")]
        [Tooltip("Slide sirasinda kapsul yuksekligi carpani (0.5 = yariya iner).")]
        [Range(0.1f, 1f)] public float HeightScale = 0.5f;
    }

    /// <summary>
    /// Bullet Jump ayarlari.
    /// </summary>
    [Serializable]
    public sealed class BulletJumpSettings
    {
        [Tooltip("Firlatma hizinin buyuklugu (m/s).")]
        [Min(0f)] public float Impulse = 19f;

        [Tooltip("Slide hizinin ne kadarinin firlatmaya eklenecegi.")]
        [Range(0f, 1f)] public float MomentumRetention = 0.35f;

        [Tooltip("Kamera asagi bakarken bile uygulanan minimum firlatma acisi (derece).")]
        [Range(-89f, 89f)] public float MinPitch = 22f;

        [Tooltip("Maksimum firlatma acisi (derece).")]
        [Range(-89f, 89f)] public float MaxPitch = 75f;

        [Tooltip("Iki bullet jump arasindaki minimum sure (sn).")]
        [Min(0f)] public float Cooldown = 0.35f;

        [Tooltip("Acik ise havada da (slide olmadan) bir kez kullanilabilir.")]
        public bool AllowInAir = false;
    }
}
