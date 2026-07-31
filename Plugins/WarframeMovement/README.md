# Warframe Movement (UE5 C++)

Warframe tarzı hızlı 3. şahıs hareket sistemi: **Sprint → Slide → Bullet Jump**.
Kendi kendine yeten bir Runtime eklentisidir; herhangi bir UE5 projesinin
`Plugins/` klasörüne kopyalanıp kullanılabilir.

Hedef sürüm: **UE 5.3+** (5.1+ ile de derlenmesi beklenir; `SetCrouchedHalfHeight`
erişimcisi 5.1 ile geldi).

## Dosyalar

| Dosya | İçerik |
|---|---|
| `Public/WarframeMovementTypes.h` | `EWarframeCustomMovementMode` (özel hareket modu enum'u) |
| `Public/WarframeMovementComponent.h` / `Private/WarframeMovementComponent.cpp` | `UCharacterMovementComponent` türevi: slide fiziği, bullet jump, network prediction |
| `Public/WarframeCharacter.h` / `Private/WarframeCharacter.cpp` | `ACharacter` türevi: kamera, Enhanced Input, girdi → hareket köprüsü |
| `WarframeMovement.Build.cs` | Modül bağımlılıkları (`EnhancedInput` dahil) |

## Kurulum

1. Eklentiyi `<Proje>/Plugins/WarframeMovement/` altına kopyalayın.
2. `.uproject` dosyasını sağ tıklayıp **Generate Visual Studio project files** deyin, derleyin.
3. `AWarframeCharacter` türevi bir Blueprint (`BP_WarframeCharacter`) oluşturun.
4. Blueprint'in **Details → Movement|Input** bölümünde şu varlıkları atayın:
   - `DefaultMappingContext` → `IMC_Default`
   - `MoveAction` (Axis2D), `LookAction` (Axis2D), `JumpAction`, `SprintAction`, `CrouchSlideAction` (Digital/bool)
5. GameMode'un `DefaultPawnClass` değerini bu Blueprint'e ayarlayın.

Bir `IMC_Default` yoksa: Content Browser → **Input → Input Mapping Context** ve
her aksiyon için **Input → Input Action** oluşturun (`MoveAction`/`LookAction`
için Value Type = `Axis2D (Vector2D)`, diğerleri için `Digital (bool)`).

## Nasıl çalışır

### 1. Slide (`MOVE_Custom` / `CMOVE_Slide`)

Sprint sırasında crouch tuşuna basıldığında `TryEnterSlide()` çağrılır. Koşullar:
yerde olmak, `bRequireSprintToSlide` ise sprint hâlinde olmak, yatay hızın
`MinSlideEntrySpeed` üzerinde olması ve `SlideCooldown`'un dolmuş olması.

Girişte:
- Hız `max(mevcut hız × SlideEntryMomentumScale, SlideEntryImpulse)` değerine çekilir (yüksek ivmeli giriş).
- `GroundFriction`, `SlideFriction` ile değiştirilir (çıkışta tasarım zamanı değerine geri yüklenir).
- `bWantsToCrouch = true` → motorun crouch mekanizması kapsülü `SlideCapsuleHalfHeightScale` (varsayılan 0.5 → yarısı) oranında küçültür. Ayağa kalkarken sıkışma (encroachment) testi motorda kaldığı için tavan altında kalma sorunu oluşmaz.

`PhysSlide()` her karede sırasıyla:
1. Dünya yerçekimini kayma düzlemine izdüşürür (`SlideGravityScale`) → yokuş aşağı hızlanma.
2. Girdi ivmesini düzleme izdüşürüp sınırlı yön kontrolü uygular (`SlideSteeringAcceleration`).
3. **Sönümleme (decay):** `v *= e^(-SlideFriction · dt)` — kare adımından bağımsız üstel azalma.
4. Hızı `MaxSlideSpeed` ile kırpar, `SafeMoveUpdatedComponent` + `SlideAlongSurface` ile hareket eder.
5. `FindFloor`/`AdjustFloorHeight` ile zemine yapışık kalır.

Çıkış koşulları: zemin kaybı, `MinSlideSpeed` altına düşme, `MaxSlideDuration`
dolması veya crouch tuşunun bırakılması. Slide bittiğinde tuş hâlâ basılıysa
karakter çömelmiş kalır; **yeni bir slide için tuşun bırakılıp tekrar basılması
gerekir** (aksi halde tuş basılı tutuldukça slide kendini yeniden tetiklerdi).

### 2. Bullet Jump

Slide sırasında `Jump` girdisi `AWarframeCharacter::Jump()` içinde yakalanır ve
normal ziplama yerine `TryBulletJump()` çağrılır. Bu yalnızca niyeti işaretler;
fırlatma bir sonraki `UpdateCharacterStateBeforeMovement()` içinde,
yani hareket çözümlemesinden hemen önce uygulanır.

`ComputeBulletJumpVelocity()`:
- Yön = kontrol (kamera) rotasyonu; dikey açı `MinBulletJumpPitch`/`MaxBulletJumpPitch`
  arasına kırpılır, böylece yere bakarken bile yukarı doğru bir çapraz yay oluşur.
- `LaunchDirection × BulletJumpImpulse + Velocity × BulletJumpMomentumRetention`
  → slide momentumunun bir kısmı korunur.

Uygulama `LaunchCharacter(..., bXYOverride=true, bZOverride=true)` ile yapılır;
`PendingLaunchVelocity` aynı hareket güncellemesinde işlendiği için client-side
prediction ile uyumludur. `bAllowBulletJumpWhileFalling` açıksa havada bir kez
daha kullanılabilir (yere değince hak yenilenir).

### 3. Ağ (network)

`FSavedMove_Warframe` üç girdi niyetini (`sprint`, `slide`, `bullet jump`)
sıkıştırılmış bayraklarla (`FLAG_Custom_0..2`) sunucuya taşır ve
`SlideElapsedTime`'ı kaydeder; `CanCombineWith` niyet değiştiğinde hamleleri
birleştirmez. Böylece sunucu tekrar oynatması (replay) istemciyle aynı sonucu
üretir.

Bilinen sınırlar: `LastBulletJumpTime` / `LastSlideEndTime` bekleme süreleri
kaydedilmiş hamleye dâhil değildir (yalnızca istemci/sunucu yerel saatlerine
bakar) ve `OnSlideStateChanged`/`OnBulletJump` delegeleri prediction tekrar
oynatmalarında birden fazla tetiklenebilir — bu yüzden VFX/ses tarafında
tekrar-güvenli (idempotent) davranın.

## Parametreler

Tümü `EditAnywhere, BlueprintReadWrite` olarak `Movement` kategorisi altındadır
ve editörden ayarlanabilir.

| Kategori | Parametre | Varsayılan |
|---|---|---|
| Sprint | `SprintMaxSpeed` | 900 |
| Slide | `MinSlideEntrySpeed` | 400 |
| Slide | `SlideEntryImpulse` | 1600 |
| Slide | `SlideEntryMomentumScale` | 1.35 |
| Slide | `MaxSlideSpeed` | 2200 |
| Slide | `MinSlideSpeed` | 350 |
| Slide | `SlideFriction` | 0.55 |
| Slide | `SlideGravityScale` | 2.0 |
| Slide | `SlideSteeringAcceleration` | 900 |
| Slide | `MaxSlideDuration` | 1.6 sn |
| Slide | `SlideCapsuleHalfHeightScale` | 0.5 |
| Slide | `SlideCooldown` | 0.15 sn |
| Slide | `bRequireSprintToSlide` | true |
| Bullet Jump | `BulletJumpImpulse` | 1900 |
| Bullet Jump | `BulletJumpMomentumRetention` | 0.35 |
| Bullet Jump | `MinBulletJumpPitch` / `MaxBulletJumpPitch` | 22° / 75° |
| Bullet Jump | `BulletJumpCooldown` | 0.35 sn |
| Bullet Jump | `bAllowBulletJumpWhileFalling` | false |

## Bellek / GC notları

- Bileşenler `CreateDefaultSubobject` ile oluşturulup `TObjectPtr` + `UPROPERTY`
  ile tutulur → GC bunları izler, elle `delete` yoktur.
- Hareket bileşeni ek bir bileşen oluşturmadan
  `ObjectInitializer.SetDefaultSubobjectClass<>()` ile değiştirilir.
- `ClientPredictionData`'nın sahipliği taban sınıftadır; burada elle serbest
  bırakılmaz (çift serbest bırakma olurdu).
- `FSavedMovePtr` paylaşımlı işaretçidir, hamle havuzunu motor yönetir.
- Sıcak yolda (`PhysSlide`) heap tahsisi, `TArray` veya `Cast<>` yoktur;
  tipli hareket bileşeni işaretçisi karakterde bir kez önbelleklenir.

## Blueprint kancaları

- `OnSlideStateChanged(bool bIsSliding)` — slide VFX/ses/animasyon geçişi için.
- `OnBulletJump(FVector LaunchVelocity)` — patlama efekti / kamera sarsıntısı için.
- `IsSliding()`, `IsSprinting()`, `CanSlide()` — Anim Blueprint sorguları için.
