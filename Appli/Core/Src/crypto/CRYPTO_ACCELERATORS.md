# Hardware Crypto Accelerators (mbedTLS Alternates)

## Overview

The STM32N6570-DK implements hardware-accelerated cryptographic operations through mbedTLS alternate implementations (`*_alt.c`/`*_alt.h` files). These leverage the STM32N6570's dedicated crypto peripherals to offload CPU-intensive TLS handshakes and data encryption.

**Location**: `Appli/Core/Src/crypto/`

**Configuration**: Enabled via `mbedtls_config_hw.h` with symbols like `MBEDTLS_AES_ALT`, `MBEDTLS_SHA256_ALT`, etc.

## Hardware Peripherals Used

| Algorithm | Peripheral | Bit Width | Operations |
|-----------|-----------|-----------|------------|
| RNG | RNG | 32-bit | Entropy generation for nonces, IVs, key material |
| AES | CRYP | 128-bit | ECB, CBC encryption/decryption |
| SHA256 | HASH | 32-bit | Hash digest computation |
| ECP (P-256) | PKA | 256-bit | Point addition, doubling, normalization |
| ECDH | PKA | 256-bit | Public key generation, shared secret |
| ECDSA | PKA | 256-bit | Sign, verify (P-256 only) |

---

## Module Descriptions

### 1. Hardware Random Number Generator (Entropy)

**Files**: `hardware_rng.c`

#### Purpose
Provides hardware-accelerated entropy generation via the STM32N6570 RNG peripheral for TLS nonce/IV generation, key derivation, and secure initialization. Also supports STSAFE-A1xx as an alternative entropy source.

#### Key Functions
| Function | Purpose |
|----------|---------|
| `uxRand()` | FreeRTOS-compatible random number generator (32-bit) |
| `mbedtls_hardware_poll()` | mbedTLS entropy callback for entropy pool seeding |

#### Entropy Sources
- **STM32N6570 RNG**: Default hardware random generator via `HAL_RNG_GenerateRandomNumber()`
- **STSAFE-A1xx**: Alternative entropy source when `__USE_STSAFE__` is defined (via `SAFEA1_GenerateRandom()`)

#### Implementation Details
- **Bit Width**: 32-bit per operation, scalable to arbitrary lengths
- **byte Ordering**: Little-endian (LSB-first extraction)
- **FreeRTOS Integration**: Direct 32-bit calls; works with FreeRTOS scheduler
- **Configuration**: Controlled via `MBEDTLS_ENTROPY_HARDWARE_ALT` in `mbedtls_config_hw.h`
- **Return Value**: Always returns 0 (success)

#### Usage in TLS
- **Session Nonces**: Random initialization vectors for CBC mode
- **Key Derivation**: Seed for PRF (pseudo-random function)
- **Handshake**: Random values in ClientHello/ServerHello messages
- **Implicit**: Called automatically by mbedTLS entropy pool during initialization

#### Thread Safety
- **RNG Hardware**: Can be called from multiple contexts (stateless)
- **FreeRTOS Compatibility**: Safe pre-kernel and post-kernel boot
- **No Mutex**: RNG peripheral has internal arbitration; no serialization needed

---

### 2. AES Hardware Acceleration

**Files**: `aes_alt.h`, `aes_alt.c`

#### Purpose
Offloads AES encryption/decryption (128-bit and 256-bit keys) to the STM32N6570 CRYP peripheral, reducing CPU cycles during TLS record encryption.

#### Context Structure
```c
typedef struct {
    uint32_t aes_key[8];    // Key material (up to 256 bits)
    uint32_t keybits;       // 128 or 256
} mbedtls_aes_context;
```

#### Supported Operations
- **ECB Mode**: Electronic Codebook (single block operations)
- **CBC Mode**: Cipher Block Chaining (stream-oriented with IV management)
- **Key Sizes**: 128-bit, 256-bit
- **Directions**: Encryption and decryption

#### Key Functions
| Function | Purpose |
|----------|---------|
| `mbedtls_aes_init()` | Initialize context |
| `mbedtls_aes_free()` | Zeroize and free context |
| `mbedtls_aes_setkey_enc()` | Load encryption key |
| `mbedtls_aes_setkey_dec()` | Load decryption key |
| `mbedtls_aes_crypt_ecb()` | Hardware ECB block cipher |
| `mbedtls_aes_crypt_cbc()` | Hardware CBC streaming |

#### Implementation Notes
- **CRYP Configuration**: Hardware setup on each operation (DeInit → Init → SetConfig)
- **IV Handling**: 32-bit word conversion for CBC mode (big-endian)
- **Data Type**: `CRYP_BYTE_SWAP` for byte transparency
- **Timeout**: `HAL_MAX_DELAY` for robustness
- **Returns**: `MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED` on hardware errors

---

### 3. SHA256 Hardware Acceleration

**Files**: `sha256_alt.h`, `sha256_alt.c`

#### Purpose
Offloads SHA256 digest computation to the STM32N6570 HASH peripheral for TLS handshake hashing and X.509 certificate validation.

#### Design Philosophy: Buffered One-Shot Hashing

**Why NOT incremental HAL HASH?**
- STM32 HAL `HAL_HASH_Accumulate()` requires strict 4-byte alignment and block-size constraints
- Suspend/resume paths are unreliable for chunked updates in real TLS workloads
- TLS transcript hashing (AWS IoT Core, X.509 verify) demands perfect bit-for-bit determinism
- Even a 1-bit mismatch breaks certificate chain validation

**Solution**: Buffer complete message, hash once with `HAL_HASH_Start()` at finish time.

#### Context Structure
```c
typedef struct {
    uint32_t total[2];              // Total bytes processed (mbedTLS compat)
    uint32_t state[8];              // Intermediate digest (compat only)
    unsigned char buffer[64];       // 64-byte block buffer (compat)
    int is224;                      // 0: SHA256, 1: SHA224 (reserved)

    uint8_t *hw_msg_buf;            // Buffered message
    size_t   hw_msg_len;            // Bytes in buffer
    size_t   hw_msg_cap;            // Buffer capacity
} mbedtls_sha256_context;
```

#### Key Functions
| Function | Purpose |
|----------|---------|
| `mbedtls_sha256_init()` | Initialize context (zero state) |
| `mbedtls_sha256_free()` | Zeroize and deallocate message buffer |
| `mbedtls_sha256_clone()` | Deep copy context with buffer |
| `mbedtls_sha256_starts()` | Begin new digest (reset state) |
| `mbedtls_sha256_update()` | Accumulate message data (buffered) |
| `mbedtls_sha256_finish()` | Compute final digest (hardware one-shot) |
| `mbedtls_sha256()` | One-shot helper for static hashing |

#### Buffer Management
- **Initial Capacity**: 256 bytes
- **Growth**: Exponential doubling (256 → 512 → 1024 → …)
- **Max Message**: 1 MB (configurable via `SHA256_ALT_MAX_MESSAGE_SIZE`)
- **Allocation**: FreeRTOS `pvPortMalloc()` (RTOS-aware)

#### Thread Safety
- **Mutex Lock**: Static `SemaphoreHandle_t xHashEngineMutex`
- **Lazy Init**: First hash operation creates mutex
- **Scheduler Check**: Works before FreeRTOS start (early boot)
- **Return Value**: `MBEDTLS_ERR_SHA256_BAD_INPUT_DATA` on failed lock

#### Implementation Notes
- Hardware hash is **one-shot only**: no Accumulate used
- Message buffer is dynamically resized as needed
- FreeRTOS task scheduler state checked for mutex availability
- Zeroization before free prevents key/data leakage

---

### 3. Elliptic Curve Point Arithmetic (ECP)

**Files**: `ecp_alt.h`, `ecp_alt.c`

#### Purpose
Accelerates P-256 elliptic curve point arithmetic (addition, doubling, normalization) for ECDSA signing and ECDH key agreement.

#### Supported Operations
- **Mixed Point Addition** (`mbedtls_internal_ecp_add_mixed`): P + Q
- **Jacobian Point Doubling** (`mbedtls_internal_ecp_double_jac`): 2P
- **Jacobian to Affine Normalization** (`mbedtls_internal_ecp_normalize_jac`)

#### Implementation Notes
- **Curve**: P-256 (secp256r1)
- **Coordinates**: Jacobian representation (X, Y, Z) for efficiency
- **PKA Backend**: Uses STM32N6570 PKA peripheral for modular arithmetic
- **Error Handling**: Returns mbedTLS error codes on hardware failure
- **Configuration**: Requires `MBEDTLS_ECP_INTERNAL_ALT` in `mbedtls_config_hw.h`

---

### 4. ECDSA Signature Operations

**Files**: `ecdsa_alt.c`

#### Purpose
Hardware-accelerated ECDSA signature generation and verification for certificate-based authentication in TLS 1.3.

#### Key Functions
| Function | Purpose |
|----------|---------|
| `mbedtls_ecdsa_sign_alt()` | Sign message digest (hardware PKA) |
| `mbedtls_ecdsa_verify_alt()` | Verify (r, s) signature pair |

#### Features
- **Curve**: P-256 only (32-byte signatures, 64 bytes for r+s)
- **Input**: Pre-computed SHA256 message digest
- **Output**: (r, s) signature components
- **PKA Usage**: Elliptic curve scalar multiplication and point math

#### Configuration
- Requires: `MBEDTLS_ECDSA_SIGN_ALT` and `MBEDTLS_ECDSA_VERIFY_ALT`
- Depends on: `ecp_alt.c` point operations

#### Implementation Notes
- **Error Code**: `MBEDTLS_ERR_ECP_HW_ACCEL_FAILED` for PKA failures
- **Thread Safety**: Protected by PKA mutex (if applicable)
- **Logging**: Debug/error level via `logging.h`

---

### 5. ECDH Key Agreement

**Files**: `ecdh_alt.c`

#### Purpose
Hardware-accelerated ECDH public key generation and shared secret derivation for TLS 1.3 key exchange.

#### Key Functions
| Function | Purpose |
|----------|---------|
| `mbedtls_ecdh_gen_public_alt()` | Generate public key from private key |
| `mbedtls_ecdh_compute_shared_alt()` | Derive shared secret from peer public key |

#### Features
- **Curve**: P-256 (secp256r1)
- **Public Key Generation**: Scalar × Generator point
- **Shared Secret**: Scalar × Peer Public Point → (x_coord)

#### P-256 Constants
```c
#define EC_P256_BYTES 32
// Prime modulus, generator point, cofactor, etc. defined in module
```

#### Implementation Notes
- **Public Key Format**: Uncompressed (0x04 || X || Y)
- **Shared Secret**: 32-byte X-coordinate (used as ECDH secret)
- **Logging Level**: `LOG_ERROR` for warnings only
- **PKA Interaction**: Direct hardware calls via HAL API

---

## Mutual Dependencies

```plaintext
RNG ──────→ RNG HAL Driver (entropy source for all TLS)

ECDSA ─────┐
           ├──→ ECP (ecp_alt.c)
ECDH ──────┤    └──→ Bignum (mbedTLS core)
           └─────→ PKA HAL Driver

AES ──────→ CRYP HAL Driver
SHA256 ───→ HASH HAL Driver + FreeRTOS (mutex)
```

---

## Configuration & Enablement

### Main Configuration File
**`mbedtls_config_hw.h`** - Enable alternates:

```c
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_AES_ALT
#define MBEDTLS_SHA256_ALT
#define MBEDTLS_ECP_INTERNAL_ALT
#define MBEDTLS_ECDSA_SIGN_ALT
#define MBEDTLS_ECDSA_VERIFY_ALT
#define MBEDTLS_ECDH_GEN_PUBLIC_ALT
#define MBEDTLS_ECDH_COMPUTE_SHARED_ALT
```

### HAL Requirements
External handle declarations (in `stm32n6xx_hal.c` or init code):
```c
extern RNG_HandleTypeDef hrng;          // For entropy/RNG
extern CRYP_HandleTypeDef hcryp;        // For AES
extern HASH_HandleTypeDef hhash;        // For SHA256
extern PKA_HandleTypeDef hpka;          // For ECP/ECDSA/ECDH
```

### STSAFE Alternative (Optional)
If using STSAFE-A1xx for entropy instead of STM32N6570 RNG, define:
```c
#define __USE_STSAFE__
```

---

## Thread Safety & Concurrency

### RNG
- **Hardware RNG**: Stateless, no context; safe for concurrent calls
- **No Mutex**: RNG peripheral has internal arbitration
- **Pre-kernel**: Works before FreeRTOS scheduler starts

### AES & SHA256
- **Separate Hardware**: CRYP and HASH are independent; no mutual exclusion needed
- **SHA256 Mutex**: Protects single HASH engine from concurrent `finish()` operations

### ECP, ECDSA, ECDH (PKA)
- **Single PKA Engine**: Shared across all ECP-based operations
- **Mutex Required**: Must coordinate ECDSA, ECDH calls (future work if needed)
- **Current**: No mutex; assumes single task or external serialization

### FreeRTOS Integration
- Scheduler state checked before mutex creation (supports pre-kernel hashing)
- All allocations use FreeRTOS heap (`pvPortMalloc` / `vPortFree`)
- Semaphore type: Static (`xSemaphoreCreateMutexStatic`)

---

## Error Handling

All functions return **mbedTLS error codes**:

| Error | Meaning | Origin |
|-------|---------|--------|
| `0` | Success | All |
| `MBEDTLS_ERR_*_BAD_INPUT_DATA` | NULL pointers, invalid params | Input validation |
| `MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED` | Hardware failure (timeout, etc.) | HAL layer |
| `MBEDTLS_ERR_SHA256_BAD_INPUT_DATA` | Mutex lock failed, buffer overflow | SHA256 specific |

**Logging**: Via `logging.h` framework:
- RNG: No logging (transparent)
- AES: No logging (transparent)
- SHA256: `LOG_INFO`
- ECP: Silent
- ECDSA: `LOG_DEBUG`
- ECDH: `LOG_ERROR` (warnings only)
