// Copyright OPT-MUS. All Rights Reserved.

using UnityEngine;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif

namespace OptMus.WarframeMovement
{
    /// <summary>
    /// Girdi soyutlamasi. Yeni Input System kuruluysa onu, degilse eski
    /// Input Manager'i kullanir; ikisi de yoksa disaridan beslenebilir.
    ///
    /// Kendi Update'i yoktur: <see cref="WarframeCharacter"/> her karede
    /// <see cref="Sample"/> cagirir. Boylece calisma sirasi (execution order)
    /// belirsizligi ve bir kare gecikme olusmaz.
    /// </summary>
    [DisallowMultipleComponent]
    [AddComponentMenu("Warframe Movement/Warframe Input Reader")]
    public sealed class WarframeInputReader : MonoBehaviour
    {
        public Vector2 Move { get; private set; }
        public Vector2 Look { get; private set; }
        public bool Sprint { get; private set; }
        public bool Crouch { get; private set; }

        private bool _jumpPressed;

        /// <summary>Ziplama basimini okur ve tuketir (tek karede bir kez).</summary>
        public bool ConsumeJumpPressed()
        {
            bool pressed = _jumpPressed;
            _jumpPressed = false;
            return pressed;
        }

#if ENABLE_INPUT_SYSTEM
        [Header("Input System aksiyonlari")]
        [SerializeField] private InputActionReference _moveAction;
        [SerializeField] private InputActionReference _lookAction;
        [SerializeField] private InputActionReference _jumpAction;
        [SerializeField] private InputActionReference _sprintAction;
        [SerializeField] private InputActionReference _crouchAction;

        private void OnEnable()
        {
            EnableAction(_moveAction);
            EnableAction(_lookAction);
            EnableAction(_jumpAction);
            EnableAction(_sprintAction);
            EnableAction(_crouchAction);
        }

        private void OnDisable()
        {
            DisableAction(_moveAction);
            DisableAction(_lookAction);
            DisableAction(_jumpAction);
            DisableAction(_sprintAction);
            DisableAction(_crouchAction);
        }

        private static void EnableAction(InputActionReference reference)
        {
            if (reference != null && reference.action != null)
            {
                reference.action.Enable();
            }
        }

        private static void DisableAction(InputActionReference reference)
        {
            if (reference != null && reference.action != null)
            {
                reference.action.Disable();
            }
        }

        /// <summary>Girdiyi bu kare icin orneklendirir.</summary>
        public void Sample()
        {
            Move = ReadVector2(_moveAction);
            Look = ReadVector2(_lookAction);
            Sprint = IsPressed(_sprintAction);
            Crouch = IsPressed(_crouchAction);

            if (_jumpAction != null && _jumpAction.action != null && _jumpAction.action.WasPressedThisFrame())
            {
                _jumpPressed = true;
            }
        }

        private static Vector2 ReadVector2(InputActionReference reference)
        {
            return reference != null && reference.action != null
                ? reference.action.ReadValue<Vector2>()
                : Vector2.zero;
        }

        private static bool IsPressed(InputActionReference reference)
        {
            return reference != null && reference.action != null && reference.action.IsPressed();
        }

#elif ENABLE_LEGACY_INPUT_MANAGER
        [Header("Eski Input Manager eksenleri")]
        [SerializeField] private string _horizontalAxis = "Horizontal";
        [SerializeField] private string _verticalAxis = "Vertical";
        [SerializeField] private string _mouseXAxis = "Mouse X";
        [SerializeField] private string _mouseYAxis = "Mouse Y";

        [Header("Tuslar")]
        [SerializeField] private KeyCode _jumpKey = KeyCode.Space;
        [SerializeField] private KeyCode _sprintKey = KeyCode.LeftShift;
        [SerializeField] private KeyCode _crouchKey = KeyCode.LeftControl;

        /// <summary>Girdiyi bu kare icin orneklendirir.</summary>
        public void Sample()
        {
            Move = new Vector2(Input.GetAxisRaw(_horizontalAxis), Input.GetAxisRaw(_verticalAxis));
            Look = new Vector2(Input.GetAxisRaw(_mouseXAxis), Input.GetAxisRaw(_mouseYAxis));
            Sprint = Input.GetKey(_sprintKey);
            Crouch = Input.GetKey(_crouchKey);

            if (Input.GetKeyDown(_jumpKey))
            {
                _jumpPressed = true;
            }
        }

#else
        /// <summary>Hicbir girdi sistemi etkin degil - degerler disaridan beslenmelidir.</summary>
        public void Sample()
        {
        }
#endif

        /// <summary>
        /// Girdiyi disaridan beslemek icin (yapay zeka, replay, otomatik test).
        /// </summary>
        public void Feed(Vector2 move, Vector2 look, bool sprint, bool crouch, bool jumpPressed)
        {
            Move = move;
            Look = look;
            Sprint = sprint;
            Crouch = crouch;

            if (jumpPressed)
            {
                _jumpPressed = true;
            }
        }
    }
}
