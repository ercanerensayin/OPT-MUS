// Copyright OPT-MUS. All Rights Reserved.

using UnityEngine;

namespace OptMus.WarframeMovement
{
    /// <summary>
    /// Girdiyi motora ve kameraya baglayan katman (UE tarafindaki AWarframeCharacter
    /// karsiligi). Fizik icermez; tum hareket karari
    /// <see cref="WarframeCharacterMotor"/> icindedir.
    ///
    /// Girdi -> hareket sozlesmesi:
    ///   Sprint (basili tut)              -> SetSprint
    ///   Crouch (sprint sirasinda basili) -> Slide
    ///   Jump   (slide sirasinda)         -> Bullet Jump
    /// </summary>
    [RequireComponent(typeof(WarframeCharacterMotor))]
    [DisallowMultipleComponent]
    [AddComponentMenu("Warframe Movement/Warframe Character")]
    // Girdi, motorun Update'inden once orneklenmelidir.
    [DefaultExecutionOrder(-10)]
    public sealed class WarframeCharacter : MonoBehaviour
    {
        [Header("Referanslar")]
        [SerializeField] private WarframeCharacterMotor _motor;
        [SerializeField] private WarframeInputReader _input;
        [SerializeField] private WarframeCameraRig _cameraRig;

        [Header("Secenekler")]
        [Tooltip("Acik ise oyun basladiginda fare imleci kilitlenir.")]
        [SerializeField] private bool _lockCursor = true;

        public WarframeCharacterMotor Motor => _motor;

        private void Reset()
        {
            // Editorde bileseni eklerken referanslari otomatik doldur.
            _motor = GetComponent<WarframeCharacterMotor>();
            _input = GetComponent<WarframeInputReader>();
#if UNITY_2023_1_OR_NEWER
            _cameraRig = FindFirstObjectByType<WarframeCameraRig>();
#else
            _cameraRig = FindObjectOfType<WarframeCameraRig>();
#endif
        }

        private void Awake()
        {
            if (_motor == null)
            {
                _motor = GetComponent<WarframeCharacterMotor>();
            }

            if (_input == null)
            {
                _input = GetComponent<WarframeInputReader>();
            }

            if (_cameraRig != null)
            {
                // Kamera-goreli hareket ve bullet jump yonu icin.
                _cameraRig.Target = transform;
                _motor.CameraTransform = _cameraRig.CameraTransform;
            }
        }

        private void Start()
        {
            if (_lockCursor)
            {
                Cursor.lockState = CursorLockMode.Locked;
                Cursor.visible = false;
            }
        }

        private void Update()
        {
            if (_input == null || _motor == null)
            {
                return;
            }

            _input.Sample();

            if (_cameraRig != null)
            {
                _cameraRig.AddLook(_input.Look);
            }

            _motor.SetMoveInput(_input.Move);
            _motor.SetSprint(_input.Sprint);
            _motor.SetSlide(_input.Crouch);

            if (_input.ConsumeJumpPressed())
            {
                // Slide sirasindaysa motor bunu otomatik olarak bullet jump'a cevirir.
                _motor.RequestJump();
            }
        }
    }
}
