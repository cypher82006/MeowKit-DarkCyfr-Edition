# 🛡️ Security Policy & Disclosure Guidelines

## 🎯 Scope & Supported Versions

The **MeowKit-DarkCyfr-Edition** project takes firmware integrity, memory isolation, and operational security seriously.

We actively provide security patches and stability updates for the following releases:

| Version / Branch | Supported | Notes |
|---|---|---|
| `main` (Latest) | ✅ Yes | Active development branch |
| `v1.x` Stable Releases | ✅ Yes | Production-ready firmware builds |
| Legacy / Unofficial Forks | ❌ No | Please upgrade to the latest `main` branch |

---

## ⚠️ Dual-Use & Ethical Hacking Disclaimer

**MeowKit-DarkCyfr-Edition** is an advanced cyber security research tool, hardware diagnostics device, and penetration testing platform. It contains dual-use capabilities (Wi-Fi frame injection, BadUSB keystroke automation, Sub-GHz RF transmission, and dynamic payload execution).

* **Authorized Use Only:** This software and associated hardware are intended strictly for authorized educational purposes, defensive penetration testing, security auditing, and laboratory research on networks and systems you own or have explicit, written authorization to evaluate.
* **Compliance:** The developers and maintainers assume no liability and are not responsible for any misuse, unauthorized access, or damage caused by this utility. Ensure strict compliance with all local, state, federal, and international cyber laws and telecommunications regulations (e.g. FCC Part 15 / ITU ISM standards).

---

## 🔍 Reporting a Vulnerability

If you discover a security vulnerability, buffer overflow, sandboxing escape, or hardware brick risk within this firmware, please do **NOT** open a public GitHub issue.

### Preferred Method: GitHub Private Vulnerability Reporting
1. Navigate to the **Security** tab of this repository.
2. Select **Advisories** -> **Report a vulnerability**.
3. Detail the technical vulnerability, affected subsystem, and proof-of-concept steps.

### Alternative Method: Direct Encrypted Email
If you prefer email or do not have a GitHub account:
* Send full vulnerability details to **security@darkcyfr.xyz**.
* If disclosing sensitive proof-of-concept code, encrypt the message using the maintainer's public PGP key (available upon request or via keyservers).

---

## ⏱️ Vulnerability Handling Timeline

When a report is received through authorized channels:
1. **Initial Acknowledgment:** Within **48 hours** of receipt.
2. **Triage & Reproduction:** Within **7 business days**, confirming severity and impact.
3. **Patch Development & Testing:** A patch is authored and tested on physical ESP32-S3 hardware.
4. **Coordinated Disclosure:** We adhere to a standard **90-day responsible disclosure window**. A public security advisory and CVE (if applicable) will be issued alongside the patched release.

---

## 🔒 Safe Sandbox & Hardware Isolation Policy

Vulnerabilities of high priority include:
* Arbitrary memory writes escaping the **Wasm3** or **Lua 5.4** interpreters in `App 11`.
* Buffer overflows in network parsing (Wi-Fi management frames, UDP packet handlers).
* Permanent hardware lockups (PMIC overvoltage / undervoltage states on AXP2101).
* Exploitable vulnerabilities in the BadUSB HID stack that compromise host systems outside test bounds.
