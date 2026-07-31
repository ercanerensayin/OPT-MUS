# Warframe Movement (Unity C#)

Warframe tarzı hızlı 3. şahıs hareket sistemi: **Sprint → Slide → Bullet Jump**.
Unity'nin yerleşik `CharacterController`'ı üzerine kuruludur; Rigidbody, DOTS
veya üçüncü parti bir karakter denetleyicisi gerektirmez.

Hedef sürüm: **Unity 2021.3 LTS ve üzeri** (2020.3 ile de derlenir).

## Dosyalar

| Dosya | İçerik |
|---|---|
| `Runtime/WarframeMovementTypes.cs` | `WarframeMovementMode` enum'u + `[Serializable]` ayar sınıfları |
| `Runtime/WarframeCharacterMotor.cs` | Hareket motoru: durum makinesi, slide fiziği, bullet jump, kapsül boyutlandırma |
| `Runtime/WarframeCharacter.cs` | Girdi → motor → kamera bağlantısı |
| `Runtime/WarframeCameraRig.cs` | 3. şahıs kamera kolu (çarpışma + yumuşatma) |
| `Runtime/WarframeInputReader.cs` | Girdi soyutlaması (yeni Input System / eski Input Manager) |

`.asmdef` bilinçli olarak **yok**: böylece kod `Assembly-CSharp` içine girer ve
yeni Input System paketi kuruluysa `#if ENABLE_INPUT_SYSTEM` bloğu ek bir
assembly referansı ayarlamaya gerek kalmadan derlenir.

## Kurulum

1. `Unity/WarframeMovement/Runtime` klasörünü projenizin `Assets/` altına kopyalayın.
2. Sahnede bir GameObject oluşturun ve şunları ekleyin:
   - `CharacterController` (Height 2, Radius 0.3, Skin Width 0.03 önerilir)
   - `WarframeCharacterMotor`
   - `WarframeInputReader`
   - `WarframeCharacter`
3. Main Camera'ya `WarframeCameraRig` ekleyin; `Target` alanına karakteri sürükleyin.
   (`WarframeCharacter` üzerindeki `Camera Rig` alanını da doldurun — motor kamera
   yönünü buradan alır.)
4. Girdi:
   - **Yeni Input System** kuruluysa: `Move`/`Look` (Value → Vector2), `Jump`/`Sprint`/`Crouch`
     (Button) aksiyonlarını oluşturup `WarframeInputReader`'daki alanlara atayın.
   - **Eski Input Manager** kullanılıyorsa alan gerekmez; varsayılanlar
     `Horizontal`/`Vertical`/`Mouse X`/`Mouse Y` + `Space`/`LeftShift`/`LeftControl`.
5. Zemin objelerinin katmanının motordaki `Ground Mask` içinde olduğundan emin olun.

## Nasıl çalışır

Motor üç durumlu bir makinedir: `Walking`, `Falling`, `Sliding`. Her karede sırayla
zemin algılanır → durum geçişleri değerlendirilir → ilgili `Phys*` metodu çalışır.

### 1. Slide

`Update` içinde crouch tuşu basılıyken `TryEnterSlide()` denenir. Koşullar:
yerde olmak, `RequireSprint` ise sprint hâlinde olmak, yatay hızın `MinEntrySpeed`
üzerinde olması, `Cooldown`'un dolmuş olması ve bu tuş basımıyla daha önce slide
yapılmamış olması.

Girişte hız `max(mevcut hız × EntryMomentumScale, EntryImpulse)` değerine çekilir
(yüksek ivmeli giriş) ve kapsül yüksekliği `HeightScale` (0.5 → yarısı) oranında
küçültülür; `center` de yarı fark kadar aşağı kaydırılır, böylece ayaklar yerinde kalır.

`PhysSliding()` her karede:
1. Dünya yerçekimini zemin normaline göre kayma düzlemine izdüşürür (`GravityScale`) → yokuş aşağı hızlanma.
2. Girdi yönünü düzleme izdüşürüp sınırlı yön kontrolü uygular (`SteeringAcceleration`).
3. **Sönümleme (decay):** `v *= Mathf.Exp(-Friction * dt)` — kare adımından bağımsız üstel azalma.
4. Hızı düzleme kilitler ve `MaxSpeed` ile kırpar.
5. Zemine bastırır (`StickToGroundSpeed`), sonra `CharacterController.Move` ile hareket eder.

Çıkış: zemin kaybı, `MinSpeed` altına düşme, `MaxDuration` dolması veya tuşun
bırakılması. Tuş hâlâ basılıysa karakter çömelmiş kalır; **yeni bir slide için
tuşun bırakılıp tekrar basılması gerekir**. Ayağa kalkma yalnızca `HasHeadroom()`
testi geçerse yapılır (tavan altında sıkışma olmaz).

### 2. Bullet Jump

`RequestJump()` önce `TryBulletJump()` denenerek işlenir; slide (veya `AllowInAir`
ile havada bir kez) değilse normal ziplamaya düşer.

- Yön = kamera ileri vektörü; dikey açı `MinPitch`–`MaxPitch` arasına kırpılır,
  böylece yere bakarken bile yukarı doğru bir çapraz yay oluşur.
- `launchDirection × Impulse + velocity × MomentumRetention` → slide momentumunun
  bir kısmı korunur.
- Durum `Falling`'e geçer, kapsül (boşluk varsa) geri açılır, `BulletJumped` olayı yayınlanır.

Havadaki `AirControl` mevcut hızı **kırpmaz** (`speedCap = max(SprintSpeed, mevcut hız)`);
aksi halde bullet jump hızı ilk karede sprint hızına düşerdi.

### 3. Duvar ve zemin teması

`OnControllerColliderHit` içinde hızın yüzeye giren bileşeni silinir
(`Vector3.ProjectOnPlane`). Bu, UE tarafındaki `SlideAlongSurface` + zemine
oturma davranışının karşılığıdır ve yerçekiminin karede birikmesini önler.

## Parametreler

Tümü Inspector'dan ayarlanabilir; birimler **metre/saniye** (UE sürümündeki
santimetre değerlerinin 1/100'ü).

| Grup | Parametre | Varsayılan |
|---|---|---|
| Ground | `WalkSpeed` / `SprintSpeed` / `CrouchSpeed` | 6 / 9 / 3.2 |
| Ground | `Acceleration` / `Deceleration` | 24 / 16 |
| Ground | `AirControl` | 0.35 |
| Ground | `Gravity` / `TerminalVelocity` | −25 / 60 |
| Ground | `JumpHeight` / `CoyoteTime` | 1.4 m / 0.1 sn |
| Ground | `StickToGroundSpeed` | 2 |
| Ground | `OrientRotationToMovement` / `RotationSpeed` | true / 720°/sn |
| Ground | `GroundMask` / `HeadroomMask` / `GroundProbeDistance` | Everything / Everything / 0.2 |
| Slide | `RequireSprint` | true |
| Slide | `MinEntrySpeed` / `EntryImpulse` / `EntryMomentumScale` | 4 / 16 / 1.35 |
| Slide | `MaxSpeed` / `MinSpeed` | 22 / 3.5 |
| Slide | `Friction` / `GravityScale` / `SteeringAcceleration` | 0.55 / 2 / 9 |
| Slide | `MaxDuration` / `Cooldown` / `HeightScale` | 1.6 sn / 0.15 sn / 0.5 |
| Bullet Jump | `Impulse` / `MomentumRetention` | 19 / 0.35 |
| Bullet Jump | `MinPitch` / `MaxPitch` | 22° / 75° |
| Bullet Jump | `Cooldown` / `AllowInAir` | 0.35 sn / false |

## Bellek / GC notları

- `Update` içinde **hiç tahsis yok**: `Physics.SphereCast(out RaycastHit)` ve
  `Physics.OverlapCapsuleNonAlloc` tekil/tahsissiz API'lerdir; overlap tamponu
  (`Collider[8]`) bir kez ayrılır.
- `transform` ve `CharacterController` `Awake`'te önbelleklenir — her karede
  `GetComponent`/`transform` erişimi yoktur.
- Olaylar düz C# `event Action<T>`; `?.Invoke` çağrısı tahsis yapmaz.
- LINQ, string birleştirme, `foreach` üzerinden struct enumerator kutulaması yok.
- Ayar sınıfları `[Serializable]` referans tipidir: sahne yüklenirken bir kez
  oluşturulur, çalışma sırasında yeniden üretilmez.

## Animasyon / VFX kancaları

```csharp
motor.SlideStateChanged += isSliding => animator.SetBool(SlideHash, isSliding);
motor.BulletJumped      += launchVelocity => cameraShake.Play(launchVelocity.magnitude);
motor.Landed            += impactSpeed => footstepAudio.PlayLanding(impactSpeed);
```

Sorgular: `motor.Mode`, `motor.IsSliding`, `motor.IsSprinting`, `motor.IsGrounded`,
`motor.IsCrouched`, `motor.Velocity`, `motor.HorizontalSpeed`, `motor.CanSlide()`.

## UE5 sürümüyle farklar

Aynı depodaki `Plugins/WarframeMovement` (UE5 C++) sürümüyle davranış eşleştirmesi:

| UE5 | Unity |
|---|---|
| `MOVE_Custom` / `CMOVE_Slide` | `WarframeMovementMode.Sliding` |
| `PhysSlide()` | `PhysSliding()` |
| `bWantsToCrouch` + engine crouch | `UpdateCapsule()` + `HasHeadroom()` |
| `LaunchCharacter()` | `_velocity = launchVelocity` |
| `SlideAlongSurface()` | `OnControllerColliderHit` + `ProjectOnPlane` |
| `FSavedMove_Warframe` (client-side prediction) | **yok** — bu sürüm tek oyuncu içindir |

Ağ tarafı bilinçli olarak dışarıda bırakıldı: Unity'de bunun tek bir standart
karşılığı yok (Netcode for GameObjects / Fish-Net / Mirror farklı prediction
modelleri kullanır). Motor girdiyi durumdan ayırdığı için (`SetMoveInput`,
`SetSprint`, `SetSlide`, `RequestJump`) seçilen kütüphanenin girdi yapısına
bağlanabilir.
