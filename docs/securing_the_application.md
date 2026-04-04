# Securing the Application

The STM32N6570 provides a strong security foundation for embedded IoT systems.  
This reference firmware enables the hardware cryptographic accelerators and outlines additional security mechanisms that developers should activate when moving toward production‑grade deployments.

---

## Hardware Security

The STM32N6570 includes several built‑in security capabilities:

### **Secure Engine (SE)**  
Dedicated hardware accelerators for:
- RNG (true hardware entropy)
- SHA‑256 hashing
- AES encryption/decryption
- PKA (elliptic‑curve arithmetic)

### **Memory Protection**  
Hardware enforcement of:
- Flash/RAM access control  
- Privilege levels  
- Execution boundaries  

### **TrustZone‑M (Arm Cortex‑M55)**  
Hardware‑enforced isolation between **Secure** and **Non‑Secure** worlds, enabling:
- Separation of trusted services from application code  
- Protection of cryptographic keys and sensitive operations  
- Reduced attack surface through compartmentalization  

TrustZone‑M provides the foundation for secure boot, secure firmware update, and secure key storage by ensuring that critical operations execute only in the Secure domain.

### **Memory Cipher Engine (MCE)**  
The STM32N6570 includes a **Memory Cipher Engine** capable of encrypting and decrypting external memory regions on‑the‑fly.  
MCE provides:

- **Transparent XIP encryption** for external flash  
- **AES‑based inline encryption/decryption**  
- **Protection of sensitive assets at rest**, including:
  - Configuration data  
  - Credentials  
  - PKCS#11 objects (if stored externally)  
- **Low‑latency operation** suitable for real‑time workloads  

MCE is recommended for deployments where external flash contains sensitive material or where physical access to the device is a realistic threat.

For more details:  
*AN6088 – How to use MCE for encryption/decryption on STM32 MCUs*

### Independent Watchdog (IWDG)

This firmware also enables the **Independent Watchdog (IWDG)** to ensure the system cannot remain stuck in a blocked state.  
The watchdog is **refreshed from the FreeRTOS Idle Task**, ensuring that only a healthy, running scheduler can keep the system alive.

The **IWDG Early Wakeup Interrupt (EWU)** is also enabled.  
It triggers shortly before the watchdog expires, allowing the firmware to capture diagnostic information.

### Additional Resources  
- [Security features on STM32N6 MCUs](https://wiki.st.com/stm32mcu/wiki/Security:Security_features_on_STM32N6_MCUs)  

## Software Security

### **Secure Boot**  
Cryptographic verification of firmware integrity and authenticity at startup.

### **OEMuRoT (OEM micro‑Root of Trust)**  
Lightweight hardware‑anchored Root of Trust supporting:
- Secure provisioning  
- Secure firmware updates  
- Anti‑rollback protection  

### Additional Resources  
- [OEMuRoT for STM32N6](https://wiki.st.com/stm32mcu/wiki/Security:OEMuRoT_for_STM32N6)

---

## Application‑Level Security

This firmware applies a defense‑in‑depth approach to protect device identity, communication, and cryptographic operations.

### **Cryptographic Operations**  
All sensitive operations — including TLS handshakes, hashing, and key generation — use hardware accelerators through the mbedTLS ALT layer when available.

### **Key Management**  
Device keys and certificates are provisioned securely and managed via the PKCS#11 abstraction layer.

### **Secure Communication**  
All MQTT traffic is protected using TLS 1.2+ with mutual authentication.

### **Certificate Validation**  
Server certificates are validated against trusted CA certificates stored on the device.

### **Secure Provisioning**  
The CLI‑based provisioning workflow supports secure onboarding, including AWS IoT Core auto‑provisioning and certificate‑based identity enrollment.

---

## Deployment Recommendations

To harden the system for production environments, the following measures are strongly recommended:

- **Use the `HW_Crypto` build configuration** to maximize performance and ensure all supported hardware accelerators are enabled.
- **Enable Secure Boot** on the STM32N6570 to verify firmware authenticity before execution.
- **Review and customize `mbedtls_config.h`** based on your threat model, enabling only the required algorithms and disabling unused features.
- **Adopt secure firmware update mechanisms**, such as OEMuRoT‑based authenticated updates with anti‑rollback protection.
- **Protect device certificates and private keys** stored in external flash; for higher‑security deployments, consider integrating a secure element such as **STSAFE**.
- **Enable the Memory Cipher Engine (MCE)** to encrypt sensitive data stored in external flash and protect against physical extraction attacks.
