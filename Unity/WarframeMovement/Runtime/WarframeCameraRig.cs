// Copyright OPT-MUS. All Rights Reserved.

using UnityEngine;

namespace OptMus.WarframeMovement
{
    /// <summary>
    /// 3. sahis kamera kolu (UE'deki USpringArmComponent + UCameraComponent karsiligi).
    /// Kameranin kendi GameObject'ine eklenir; hedefi takip eder, duvarlarda
    /// iceri kacar ve yumusak gecis (lag) uygular.
    /// </summary>
    [DisallowMultipleComponent]
    [AddComponentMenu("Warframe Movement/Warframe Camera Rig")]
    public sealed class WarframeCameraRig : MonoBehaviour
    {
        [Header("Hedef")]
        [SerializeField] private Transform _target;

        [Tooltip("Hedefe gore donus merkezi (omuz hizasi).")]
        [SerializeField] private Vector3 _pivotOffset = new Vector3(0f, 1.5f, 0f);

        [Tooltip("Kameranin yanal kaydirmasi (omuz kamerasi).")]
        [SerializeField] private Vector3 _socketOffset = new Vector3(0.6f, 0f, 0f);

        [Header("Kol")]
        [Tooltip("Kamera kolunun uzunlugu (m).")]
        [Min(0f)] [SerializeField] private float _distance = 3.5f;

        [Tooltip("Kamera takip yumusakligi. Buyuk deger = daha sert takip.")]
        [Min(0.01f)] [SerializeField] private float _followSharpness = 18f;

        [Header("Bakis")]
        [Tooltip("Bakis hassasiyeti (derece / girdi birimi).")]
        [Min(0f)] [SerializeField] private float _sensitivity = 0.12f;

        [SerializeField] private float _minPitch = -55f;
        [SerializeField] private float _maxPitch = 70f;

        [Tooltip("Acik ise dikey bakis ters cevrilir.")]
        [SerializeField] private bool _invertY = false;

        [Header("Carpisma")]
        [Tooltip("Kamera kolunun carpisacagi katmanlar.")]
        [SerializeField] private LayerMask _collisionMask = ~0;

        [Tooltip("Kamera carpisma kuresinin yaricapi (m).")]
        [Min(0.01f)] [SerializeField] private float _collisionRadius = 0.25f;

        private Transform _transform;
        private float _yaw;
        private float _pitch;
        private Vector3 _currentPosition;

        /// <summary>Motorun kamera-goreli hareket icin kullanacagi transform.</summary>
        public Transform CameraTransform => _transform != null ? _transform : transform;

        public Transform Target
        {
            get => _target;
            set => _target = value;
        }

        private void Awake()
        {
            _transform = transform;

            Vector3 angles = _transform.eulerAngles;
            _yaw = angles.y;
            _pitch = angles.x;

            _currentPosition = _transform.position;
        }

        /// <summary>Bakis girdisi (x = yatay, y = dikey).</summary>
        public void AddLook(Vector2 lookInput)
        {
            _yaw += lookInput.x * _sensitivity;
            _pitch += (_invertY ? lookInput.y : -lookInput.y) * _sensitivity;
            _pitch = Mathf.Clamp(_pitch, _minPitch, _maxPitch);
        }

        private void LateUpdate()
        {
            if (_target == null)
            {
                return;
            }

            Quaternion rotation = Quaternion.Euler(_pitch, _yaw, 0f);
            Vector3 pivot = _target.position + _pivotOffset;
            Vector3 desired = pivot + rotation * (_socketOffset - Vector3.forward * _distance);

            // Kamera kolu carpismasi: duvar varsa kamera iceri kayar.
            Vector3 direction = desired - pivot;
            float length = direction.magnitude;

            if (length > Mathf.Epsilon && Physics.SphereCast(pivot, _collisionRadius, direction / length,
                    out RaycastHit hit, length, _collisionMask, QueryTriggerInteraction.Ignore))
            {
                desired = pivot + direction / length * hit.distance;
            }

            // Kare adimindan bagimsiz yumusatma.
            float t = 1f - Mathf.Exp(-_followSharpness * Time.deltaTime);
            _currentPosition = Vector3.Lerp(_currentPosition, desired, t);

            _transform.SetPositionAndRotation(_currentPosition, rotation);
        }
    }
}
