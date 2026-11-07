# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest  | Yes       |

## Reporting a Vulnerability

If you discover a security vulnerability in this project, **please do not open a public issue**.

Instead, report it privately:

1. **Email**: [macaw-karma-crouch@duck.com](mailto:macaw-karma-crouch@duck.com)
2. **GitHub**: Use [Security Advisories](https://github.com/Cynid-22/True-Random-Number-Generator/security/advisories/new) to report privately

### What to Include

- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

### Response Timeline

- **Acknowledgment**: Within 48 hours
- **Assessment**: Within 1 week
- **Fix**: Depends on severity (critical issues prioritized)

## Security Architecture

This application implements the following security measures:

### Cryptographic Generator
- **HMAC_DRBG (SHA-512)** from NIST SP 800-90A Rev. 1, 256-bit security strength: created from the first 512 credited bits collected (or from 512 reserve bits if the ticked sources hold less), reseeded at every generation (with any new data meant for it) and every 64 MiB during long outputs, state rewritten after every request of at most 64 KiB
- **SHA-512** (FIPS 180-4) and **HMAC-SHA-512** (FIPS 198-1) underneath it; no table lookups, so no cache-timing leak
- **Self-tests at launch**: SHA-512, HMAC-SHA-512 (RFC 4231), HMAC_DRBG (NIST CAVP vectors) and the Toeplitz extractor (vectors from an independent reference computation) are checked against known answers; if one fails, generating is turned off
- Windows' random generator (BCryptGenRandom) adds 64 bytes every time the generator is created or reseeded. They are uncredited: never counted as entropy, so they can only add to the output's unpredictability and never prop up a claim. If Windows can't supply them, generation continues without them
- The generator's state is kept in locked memory and wiped on Clear, logoff and exit; a generation works on a copy that replaces it only when the generation succeeds
- The statistical results in the README are from the earlier quad-layer pipeline (ChaCha20, AES-256) and have not been rerun for HMAC_DRBG yet

### True Random Reserve
- After the generator's first 512 credited bits, each source's data is condensed into a reserve by a **Toeplitz extractor** (universal hashing, no hash function): every 640 credited bits of one source become 512 reserve bits, within 2^-65 of uniform (averaged over the extractor's seed) by the leftover hash lemma if the credit holds. The bound, the extractor's public seed (257 NIST Randomness Beacon pulses) and the rules are in [docs/stochastic_models.md](docs/stochastic_models.md#the-true-random-reserve)
- **TRUE RANDOM** outputs are reserve bits XOR HMAC_DRBG output (NIST SP 800-90C RBG3(XOR)), only when the reserve covers every byte the output draws; otherwise the output is HMAC_DRBG alone. There is never a partly covered output
- Data that seeds the generator never goes into the reserve, and reserve bits used for an XOR are never used to seed the generator (SP 800-90C 6.4.2). If the ticked sources hold less than 512 bits for the generator when it is created, it is seeded with 512 reserve bits that are then never used for an XOR. During TRUE RANDOM outputs the generator is reseeded with 256 reserve bits after every 16 KiB it produces
- Used reserve bits are wiped and never handed out again; a cancelled or failed generation uses none up
- If a source fails a health test, the chunk it was filling is wiped and dropped, so its data no longer reaches the reserve (AIS 31 total-failure reaction)
- The extractor processes every input bit the same way, without branches on the data, so its timing and memory accesses depend only on the input's length
- The reserve is kept in locked memory and wiped on Clear, logoff and exit

### Memory Security
- Collected entropy data, the seed material and the generator's state use an allocator that locks their pages in RAM (`VirtualLock`, best effort: the process asks for a 128 MB minimum working set, and larger pools can still be paged) and wipes every block when it is freed, including the old blocks a growing buffer leaves behind
- Dear ImGui's memory is wiped when freed, and Clear, leaving One-Time Pad, logoff and exit wipe ImGui's copies of typed text (edit buffer, undo history, deactivated copy)
- Output buffers, keys and the pool are wiped with `SecureZeroMemory`; the generation thread works on a snapshot of the settings that is wiped when it ends
- Logoff and shutdown (`WM_ENDSESSION`) wipe the pool, outputs, One-Time Pad message and clipboard
- System DLLs load from System32 only (`SetDefaultDllDirectories`; d3d11, d3dcompiler, dwmapi and bcrypt are loaded at run time from System32; gamepad support is compiled out), so a DLL placed next to the exe is not loaded

### Thread Safety
- `std::atomic` for all shared collector state
- `std::mutex` for entropy pool access
- Atomic `exchange()` pattern for start/stop operations

### Logging
- TRNG has no logging: no entropy values, timing data, seeds, keys or output reach a log file, the console or the debugger output

## Known Limitations

- **Clipboard**: Copied output is excluded from Windows clipboard history and cloud sync and cleared after 60 seconds or at exit, but the copy Windows keeps until then cannot be wiped by the application (OS limitation)
- **Memory locking**: Outputs, One-Time Pad keys and messages are wiped but not locked in RAM, so they can reach the pagefile
- **Entropy Estimation**: Each source is credited conservatively (at most 1 or 2 bits per timing or movement sample, 8 bits per 64 microphone LSBs) and runs the SP 800-90B repetition count and adaptive proportion health tests. The caps sit below the min-entropy an SP 800-90B assessment of raw samples measured, but on one laptop only ([Measured Source Entropy](README.md#measured-source-entropy-nist-sp-800-90b)); other hardware can measure differently