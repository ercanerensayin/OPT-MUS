// Copyright OPT-MUS. All Rights Reserved.

using System;
using UnityEngine;

namespace OptMus.WarframeMovement
{
    /// <summary>
    /// Warframe tarzi hareket motoru (UE tarafindaki UCharacterMovementComponent karsiligi).
    ///
    /// Durum makinesi: Walking / Falling / Sliding.
    ///  - Slide  : sprint sirasinda crouch ile tetiklenir; yuksek ivmeli giris, dusuk
    ///             surtunme, yariya inen kapsul, zamanla sonumlenen hiz.
    ///  - Bullet Jump : slide sirasinda jump ile kamera yonune capraz (3D) firlatma.
    ///
    /// Girdi okumaz - girdiyi <see cref="WarframeCharacter"/> besler. Boylece motor
    /// hem oyuncu hem yapay zeka tarafindan kullanilabilir.
    ///
    /// Performans: sicak yolda (Update) hicbir tahsis yoktur; fizik sorgulari tekil
    /// (non-alloc) API'lerle yapilir, overlap tamponu bir kez ayrilir.
    /// </summary>
    [RequireComponent(typeof(CharacterController))]
    [DisallowMultipleComponent]
    [AddComponentMenu("Warframe Movement/Warframe Character Motor")]
    public sealed class WarframeCharacterMotor : MonoBehaviour
    {
        private const float MinMoveInputSqr = 1e-4f;
        private const float MinDirectionSqr = 1e-6f;

        [Header("Referanslar")]
        [SerializeField]
        [Tooltip("Kamera-goreli hareket ve bullet jump yonu icin. Bos ise karakterin kendi yonu kullanilir.")]
        private Transform _cameraTransform;

        [Header("Ayarlar")]
        [SerializeField] private GroundMovementSettings _ground = new GroundMovementSettings();
        [SerializeField] private SlideSettings _slide = new SlideSettings();
        [SerializeField] private BulletJumpSettings _bulletJump = new BulletJumpSettings();

        // --- Olaylar (VFX / ses / animasyon icin) -------------------------------
        /// <summary>Slide baslarken true, biterken false.</summary>
        public event Action<bool> SlideStateChanged;

        /// <summary>Bullet jump uygulandiginda firlatma hizi ile tetiklenir.</summary>
        public event Action<Vector3> BulletJumped;

        /// <summary>Yere inildiginde carpma hizi (m/s, pozitif) ile tetiklenir.</summary>
        public event Action<float> Landed;

        // --- Onbelleklenmis referanslar ---------------------------------------
        private CharacterController _controller;
        private Transform _transform;
        private readonly Collider[] _overlapBuffer = new Collider[8];

        // --- Durum -------------------------------------------------------------
        private WarframeMovementMode _mode = WarframeMovementMode.Falling;
        private Vector3 _velocity;
        private Vector3 _groundNormal = Vector3.up;
        private bool _isGrounded;
        private bool _hasGroundContact;

        private Vector2 _moveInput;
        private bool _wantsSprint;
        private bool _wantsSlide;
        private bool _jumpRequested;

        private float _slideTimer;
        private float _lastSlideEndTime = float.NegativeInfinity;
        private float _lastBulletJumpTime = float.NegativeInfinity;
        private float _lastGroundedTime = float.NegativeInfinity;
        private bool _slideInputConsumed;
        private bool _bulletJumpConsumedInAir;

        private float _defaultHeight;
        private Vector3 _defaultCenter;

        // --- Genel erisim ------------------------------------------------------
        public WarframeMovementMode Mode => _mode;
        public bool IsSliding => _mode == WarframeMovementMode.Sliding;
        public bool IsGrounded => _isGrounded;
        public bool IsCrouched => _controller != null && _controller.height < _defaultHeight - 0.01f;
        public bool IsSprinting => _wantsSprint && _mode == WarframeMovementMode.Walking && !IsCrouched && HorizontalSpeed > 0.1f;
        public Vector3 Velocity => _velocity;
        public float HorizontalSpeed => new Vector2(_velocity.x, _velocity.z).magnitude;
        public Transform CameraTransform { get => _cameraTransform; set => _cameraTransform = value; }

        private void Awake()
        {
            _transform = transform;
            _controller = GetComponent<CharacterController>();

            _defaultHeight = _controller.height;
            _defaultCenter = _controller.center;
        }

        // ======================================================================
        // Girdi arayuzu
        // ======================================================================

        /// <summary>Yatay hareket girdisi (x = sag, y = ileri), -1..1.</summary>
        public void SetMoveInput(Vector2 input)
        {
            _moveInput = Vector2.ClampMagnitude(input, 1f);
        }

        /// <summary>Sprint tusu basili mi.</summary>
        public void SetSprint(bool value)
        {
            _wantsSprint = value;
        }

        /// <summary>Crouch/slide tusu basili mi. Sartlar uygunsa slide baslar, degilse normal comelme.</summary>
        public void SetSlide(bool value)
        {
            _wantsSlide = value;
        }

        /// <summary>Ziplama girdisi. Slide sirasinda otomatik olarak bullet jump'a donusur.</summary>
        public void RequestJump()
        {
            _jumpRequested = true;
        }

        /// <summary>Hizi disaridan ezmek icin (teleport, patlama, platform vb.).</summary>
        public void SetVelocity(Vector3 velocity)
        {
            _velocity = velocity;
        }

        // ======================================================================
        // Ana dongu
        // ======================================================================

        private void Update()
        {
            float deltaTime = Time.deltaTime;
            if (deltaTime <= Mathf.Epsilon)
            {
                return;
            }

            ProbeGround();
            UpdateStateBeforeMovement();

            switch (_mode)
            {
                case WarframeMovementMode.Walking:
                    PhysWalking(deltaTime);
                    break;

                case WarframeMovementMode.Falling:
                    PhysFalling(deltaTime);
                    break;

                case WarframeMovementMode.Sliding:
                    PhysSliding(deltaTime);
                    break;
            }
        }

        /// <summary>
        /// Hareketten once calisan durum makinesi
        /// (UE: UpdateCharacterStateBeforeMovement).
        /// </summary>
        private void UpdateStateBeforeMovement()
        {
            // 1) Ziplama / bullet jump istegi - hareket cozumlemesinden once.
            if (_jumpRequested)
            {
                _jumpRequested = false;

                if (!TryBulletJump())
                {
                    TryJump();
                }
            }

            // 2) Crouch tusu birakildiginda yeni bir slide hakki dogar.
            //    (Aksi halde tus basili tutuldukca slide kendini yeniden tetiklerdi.)
            if (!_wantsSlide)
            {
                _slideInputConsumed = false;
            }

            // 3) Slide giris / cikis.
            if (_mode == WarframeMovementMode.Sliding)
            {
                if (ShouldExitSlide())
                {
                    SetMode(_isGrounded ? WarframeMovementMode.Walking : WarframeMovementMode.Falling);
                }
            }
            else if (_wantsSlide)
            {
                TryEnterSlide();
            }

            // 4) Yer / hava gecisleri.
            if (_mode == WarframeMovementMode.Walking && !_isGrounded)
            {
                SetMode(WarframeMovementMode.Falling);
            }
            else if (_mode == WarframeMovementMode.Falling && _isGrounded && _velocity.y <= 0f)
            {
                float impactSpeed = -_velocity.y;
                SetMode(WarframeMovementMode.Walking);
                Landed?.Invoke(impactSpeed);
            }

            // 5) Kapsul boyutu: slide veya comelme istegi varsa kucult,
            //    ayaga kalkarken tepe bosluk (encroachment) testi yap.
            UpdateCapsule();

            // 6) Coyote time ve havadaki bullet jump hakki, yalnizca gercekten
            //    yer modundayken tazelenir. (Ziplamanin ilk karesinde karakter
            //    hala zemine yakindir; _isGrounded'a bakmak cift ziplamaya yol acardi.)
            if (_mode == WarframeMovementMode.Walking || _mode == WarframeMovementMode.Sliding)
            {
                _lastGroundedTime = Time.time;
                _bulletJumpConsumedInAir = false;
            }
        }

        // ======================================================================
        // Hareket modlari
        // ======================================================================

        private void PhysWalking(float deltaTime)
        {
            Vector3 wishDirection = GetWishDirection();
            Vector3 horizontal = new Vector3(_velocity.x, 0f, _velocity.z);

            float targetSpeed = GetTargetGroundSpeed();
            Vector3 targetVelocity = wishDirection * targetSpeed;

            // Girdi varsa hizlan, yoksa yavasla. MoveTowards dogrusal ve ongorulebilir.
            float rate = wishDirection.sqrMagnitude > MinDirectionSqr ? _ground.Acceleration : _ground.Deceleration;
            horizontal = Vector3.MoveTowards(horizontal, targetVelocity, rate * deltaTime);

            _velocity.x = horizontal.x;
            _velocity.z = horizontal.z;

            // Zemine yapisik kal (rampalarda ziplamayi onler).
            if (_velocity.y < 0f)
            {
                _velocity.y = -_ground.StickToGroundSpeed;
            }

            MoveAndRotate(deltaTime);
        }

        private void PhysFalling(float deltaTime)
        {
            Vector3 wishDirection = GetWishDirection();
            Vector3 horizontal = new Vector3(_velocity.x, 0f, _velocity.z);

            // Hava kontrolu mevcut momentumu kirpmaz: bullet jump hizi korunur,
            // yalnizca yon degistirmeye izin verilir.
            float speedCap = Mathf.Max(_ground.SprintSpeed, horizontal.magnitude);
            horizontal += wishDirection * (_ground.Acceleration * _ground.AirControl * deltaTime);
            horizontal = Vector3.ClampMagnitude(horizontal, speedCap);

            _velocity.x = horizontal.x;
            _velocity.z = horizontal.z;

            _velocity.y = Mathf.Max(_velocity.y + _ground.Gravity * deltaTime, -_ground.TerminalVelocity);

            MoveAndRotate(deltaTime);
        }

        /// <summary>
        /// Slide fizigi (UE: PhysSlide).
        /// </summary>
        private void PhysSliding(float deltaTime)
        {
            _slideTimer += deltaTime;

            // 1) Egim yercekimi: dunya yercekimini kayma duzlemine izdusur.
            //    Yokus asagi hizlandirir, yokus yukari yavaslatir.
            Vector3 gravityAcceleration = Vector3.up * (_ground.Gravity * _slide.GravityScale);
            _velocity += Vector3.ProjectOnPlane(gravityAcceleration, _groundNormal) * deltaTime;

            // 2) Sinirli yon kontrolu (steering).
            Vector3 wishDirection = GetWishDirection();
            if (_slide.SteeringAcceleration > 0f && wishDirection.sqrMagnitude > MinDirectionSqr)
            {
                Vector3 steerDirection = Vector3.ProjectOnPlane(wishDirection, _groundNormal);
                if (steerDirection.sqrMagnitude > MinDirectionSqr)
                {
                    _velocity += steerDirection.normalized * (_slide.SteeringAcceleration * deltaTime);
                }
            }

            // 3) Surtunme / sonumleme (decay): kare adimindan bagimsiz ussel azalma.
            //    v(t) = v0 * e^(-Friction * t)
            _velocity *= Mathf.Exp(-_slide.Friction * deltaTime);

            // 4) Hizi kayma duzlemine kilitle ve ust sinira kirp.
            _velocity = Vector3.ClampMagnitude(Vector3.ProjectOnPlane(_velocity, _groundNormal), _slide.MaxSpeed);

            // 5) Zemine bastir - basamak ve egim gecislerinde havalanmayi onler.
            _velocity -= _groundNormal * _ground.StickToGroundSpeed;

            MoveAndRotate(deltaTime);
        }

        // ======================================================================
        // Slide
        // ======================================================================

        /// <summary>Slide sartlari saglaniyor mu (UI / animasyon icin de kullanilabilir).</summary>
        public bool CanSlide()
        {
            if (IsSliding || _slideInputConsumed || !_isGrounded)
            {
                return false;
            }

            if (_slide.RequireSprint && !_wantsSprint)
            {
                return false;
            }

            if (HorizontalSpeed < _slide.MinEntrySpeed)
            {
                return false;
            }

            return Time.time - _lastSlideEndTime >= _slide.Cooldown;
        }

        private bool TryEnterSlide()
        {
            if (!CanSlide())
            {
                return false;
            }

            Vector3 direction = new Vector3(_velocity.x, 0f, _velocity.z);
            if (direction.sqrMagnitude < MinDirectionSqr)
            {
                direction = _transform.forward;
            }
            direction.Normalize();

            // "Yuksek ivmeli" giris: mevcut momentumu olcekle, taban itmenin altina dusme.
            float entrySpeed = Mathf.Max(HorizontalSpeed * _slide.EntryMomentumScale, _slide.EntryImpulse);
            _velocity = direction * Mathf.Min(entrySpeed, _slide.MaxSpeed);

            SetMode(WarframeMovementMode.Sliding);
            return true;
        }

        private bool ShouldExitSlide()
        {
            if (!_wantsSlide || !_hasGroundContact)
            {
                return true;
            }

            if (HorizontalSpeed < _slide.MinSpeed)
            {
                return true;
            }

            return _slide.MaxDuration > 0f && _slideTimer >= _slide.MaxDuration;
        }

        // ======================================================================
        // Ziplama ve Bullet Jump
        // ======================================================================

        private bool TryJump()
        {
            bool withinCoyoteTime = Time.time - _lastGroundedTime <= _ground.CoyoteTime;
            if (_mode == WarframeMovementMode.Sliding || (!_isGrounded && !withinCoyoteTime))
            {
                return false;
            }

            // Comelmisken ancak tepede bosluk varsa ziplanir.
            if (IsCrouched && !TryResizeCapsule(_defaultHeight))
            {
                return false;
            }

            _velocity.y = Mathf.Sqrt(2f * Mathf.Abs(_ground.Gravity) * _ground.JumpHeight);
            _lastGroundedTime = float.NegativeInfinity;
            SetMode(WarframeMovementMode.Falling);
            return true;
        }

        /// <summary>
        /// Slide sirasinda (veya izin veriliyorsa havada) kamera yonune capraz firlatma.
        /// </summary>
        /// <returns>Bullet jump uygulandiysa true; bu durumda normal ziplama atlanmalidir.</returns>
        public bool TryBulletJump()
        {
            if (Time.time - _lastBulletJumpTime < _bulletJump.Cooldown)
            {
                return false;
            }

            bool fromSlide = _mode == WarframeMovementMode.Sliding;
            bool fromAir = _bulletJump.AllowInAir
                           && _mode == WarframeMovementMode.Falling
                           && !_bulletJumpConsumedInAir;

            if (!fromSlide && !fromAir)
            {
                return false;
            }

            Vector3 launchVelocity = ComputeBulletJumpVelocity();
            if (launchVelocity.sqrMagnitude < MinDirectionSqr)
            {
                return false;
            }

            _velocity = launchVelocity;
            _lastBulletJumpTime = Time.time;
            _bulletJumpConsumedInAir = fromAir;

            // Slide'dan cikis: kapsul (bosluk varsa) geri acilir.
            SetMode(WarframeMovementMode.Falling);
            UpdateCapsule();

            BulletJumped?.Invoke(launchVelocity);
            return true;
        }

        private Vector3 ComputeBulletJumpVelocity()
        {
            Vector3 aim = _cameraTransform != null ? _cameraTransform.forward : _transform.forward;
            aim.Normalize();

            Vector3 horizontal = new Vector3(aim.x, 0f, aim.z);
            if (horizontal.sqrMagnitude < MinDirectionSqr)
            {
                horizontal = new Vector3(_transform.forward.x, 0f, _transform.forward.z);
            }
            if (horizontal.sqrMagnitude < MinDirectionSqr)
            {
                horizontal = Vector3.forward;
            }
            horizontal.Normalize();

            // Dikey aciyi sinirla: asagi bakarken bile yukari dogru bir yay olusur.
            float minPitch = Mathf.Min(_bulletJump.MinPitch, _bulletJump.MaxPitch);
            float maxPitch = Mathf.Max(_bulletJump.MinPitch, _bulletJump.MaxPitch);
            float aimPitch = Mathf.Asin(Mathf.Clamp(aim.y, -1f, 1f)) * Mathf.Rad2Deg;
            float pitch = Mathf.Clamp(aimPitch, minPitch, maxPitch) * Mathf.Deg2Rad;

            // Capraz (3D) firlatma vektoru.
            Vector3 launchDirection = horizontal * Mathf.Cos(pitch) + Vector3.up * Mathf.Sin(pitch);
            launchDirection.Normalize();

            // Slide momentumunun bir kismi korunur - hizli girisin odulu.
            return launchDirection * _bulletJump.Impulse + _velocity * _bulletJump.MomentumRetention;
        }

        // ======================================================================
        // Yardimcilar
        // ======================================================================

        private void SetMode(WarframeMovementMode next)
        {
            if (next == _mode)
            {
                return;
            }

            WarframeMovementMode previous = _mode;
            _mode = next;

            if (next == WarframeMovementMode.Sliding)
            {
                _slideTimer = 0f;
                SlideStateChanged?.Invoke(true);
            }
            else if (previous == WarframeMovementMode.Sliding)
            {
                _slideTimer = 0f;
                _lastSlideEndTime = Time.time;
                _slideInputConsumed = true; // yeni slide icin tusa tekrar basilmali
                SlideStateChanged?.Invoke(false);
            }
        }

        /// <summary>
        /// Kapsul yuksekligini istege gore ayarlar. Buyutme yalnizca tepede yer
        /// varsa yapilir (UE'deki UnCrouch encroachment testinin karsiligi).
        /// </summary>
        private void UpdateCapsule()
        {
            bool wantsSmall = IsSliding || (_wantsSlide && _mode != WarframeMovementMode.Falling);
            float targetHeight = wantsSmall ? _defaultHeight * _slide.HeightScale : _defaultHeight;

            if (Mathf.Approximately(targetHeight, _controller.height))
            {
                return;
            }

            TryResizeCapsule(targetHeight);
        }

        private bool TryResizeCapsule(float targetHeight)
        {
            // Kucultmek her zaman guvenlidir; buyutmeden once tepe boslugu test edilir.
            if (targetHeight > _controller.height && !HasHeadroom(targetHeight))
            {
                return false;
            }

            _controller.height = targetHeight;
            _controller.center = new Vector3(
                _defaultCenter.x,
                _defaultCenter.y - (_defaultHeight - targetHeight) * 0.5f,
                _defaultCenter.z);

            return true;
        }

        private bool HasHeadroom(float targetHeight)
        {
            float radius = Mathf.Max(_controller.radius - 0.01f, 0.01f);
            Vector3 feet = _transform.position + _controller.center - Vector3.up * (_controller.height * 0.5f);

            Vector3 bottom = feet + Vector3.up * (radius + _controller.skinWidth);
            Vector3 top = feet + Vector3.up * Mathf.Max(targetHeight - radius, radius + _controller.skinWidth);

            int count = Physics.OverlapCapsuleNonAlloc(
                bottom, top, radius, _overlapBuffer, _ground.HeadroomMask, QueryTriggerInteraction.Ignore);

            for (int i = 0; i < count; i++)
            {
                Transform hit = _overlapBuffer[i].transform;
                if (hit != _transform && !hit.IsChildOf(_transform))
                {
                    return false;
                }
            }

            return true;
        }

        /// <summary>
        /// Zemin algilama: kapsulun alt kuresinden asagi tekil (tahsissiz) sphere cast.
        /// </summary>
        private void ProbeGround()
        {
            const float probeOffset = 0.1f;

            float radius = Mathf.Max(_controller.radius - _controller.skinWidth, 0.01f);

            // Isin, kapsulun alt kuresinin merkezinden biraz yukaridan baslar:
            // zemine gomulu baslayan bir sphere cast gecerli normal dondurmez.
            Vector3 origin = _transform.position + _controller.center
                             - Vector3.up * (_controller.height * 0.5f - _controller.radius - probeOffset);

            float distance = _ground.GroundProbeDistance + _controller.skinWidth + probeOffset;

            if (Physics.SphereCast(origin, radius, Vector3.down, out RaycastHit hit, distance,
                    _ground.GroundMask, QueryTriggerInteraction.Ignore))
            {
                _hasGroundContact = true;
                _groundNormal = hit.normal;
                _isGrounded = Vector3.Angle(hit.normal, Vector3.up) <= _controller.slopeLimit;
            }
            else
            {
                _hasGroundContact = false;
                _groundNormal = Vector3.up;
                _isGrounded = false;
            }
        }

        private Vector3 GetWishDirection()
        {
            if (_moveInput.sqrMagnitude < MinMoveInputSqr)
            {
                return Vector3.zero;
            }

            Vector3 forward;
            Vector3 right;

            if (_cameraTransform != null)
            {
                forward = new Vector3(_cameraTransform.forward.x, 0f, _cameraTransform.forward.z);
                right = new Vector3(_cameraTransform.right.x, 0f, _cameraTransform.right.z);
            }
            else
            {
                forward = new Vector3(_transform.forward.x, 0f, _transform.forward.z);
                right = new Vector3(_transform.right.x, 0f, _transform.right.z);
            }

            forward.Normalize();
            right.Normalize();

            return Vector3.ClampMagnitude(forward * _moveInput.y + right * _moveInput.x, 1f);
        }

        private float GetTargetGroundSpeed()
        {
            if (IsCrouched)
            {
                return _ground.CrouchSpeed;
            }

            return _wantsSprint ? _ground.SprintSpeed : _ground.WalkSpeed;
        }

        private void MoveAndRotate(float deltaTime)
        {
            _controller.Move(_velocity * deltaTime);
            RotateTowardsVelocity(deltaTime);
        }

        private void RotateTowardsVelocity(float deltaTime)
        {
            if (!_ground.OrientRotationToMovement)
            {
                return;
            }

            Vector3 flat = new Vector3(_velocity.x, 0f, _velocity.z);
            if (flat.sqrMagnitude < 0.04f)
            {
                return;
            }

            Quaternion target = Quaternion.LookRotation(flat, Vector3.up);
            _transform.rotation = Quaternion.RotateTowards(
                _transform.rotation, target, _ground.RotationSpeed * deltaTime);
        }

        /// <summary>
        /// Duvara / zemine carpildiginda hizin yuzeye giren bilesenini siler
        /// (UE tarafindaki SlideAlongSurface + zemine oturma davranisinin karsiligi).
        /// </summary>
        private void OnControllerColliderHit(ControllerColliderHit hit)
        {
            if (Vector3.Dot(_velocity, hit.normal) < 0f)
            {
                _velocity = Vector3.ProjectOnPlane(_velocity, hit.normal);
            }
        }

#if UNITY_EDITOR
        private void OnValidate()
        {
            // Editorde deger degistirilirken tutarsiz araliklari duzelt.
            if (_slide.MinSpeed > _slide.MaxSpeed)
            {
                _slide.MinSpeed = _slide.MaxSpeed;
            }

            if (_ground.Gravity > 0f)
            {
                _ground.Gravity = -_ground.Gravity;
            }
        }
#endif
    }
}
