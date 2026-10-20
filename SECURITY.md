# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest  | Yes       |

## Reporting a Vulnerability

If you discover a security vulnerability in this project, **please do not open a public issue**.

Instead, report it privately:

1. **Email**: [your-email@example.com] *(replace with your actual contact)*
2. **GitHub**: Use [Security Advisories](https://github.com/yourusername/TRNG/security/advisories/new) to report privately

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

### Cryptographic Pipeline
- **SHA-512** (FIPS 180-4) for hashing
- **HKDF** (RFC 5869) for key derivation
- **ChaCha20** (RFC 8439) for stream generation
- **AES-256-CTR** (FIPS 197) for block transformation
- Quad-layer pipeline tested with **NIST SP 800-22** (8000 streams): 13 of 15 tests passed cleanly; FFT and Universal passed only marginally (their P-values passed, but their pass counts, 7886 and 7892 of 8000, were just under the strict bound of 7893). See the README for the full results

### Memory Security
- Collected entropy data and the seed material use an allocator that locks their pages in RAM (`VirtualLock`, best effort: the process asks for a 128 MB minimum working set, and larger pools can still be paged) and wipes every block when it is freed, including the old blocks a growing buffer leaves behind
- Dear ImGui's memory is wiped when freed, and Clear, leaving One-Time Pad, logoff and exit wipe ImGui's copies of typed text (edit buffer, undo history, deactivated copy)
- Output buffers, keys and the pool are wiped with `SecureZeroMemory`; the generation thread works on a snapshot of the settings that is wiped when it ends
- Logoff and shutdown (`WM_ENDSESSION`) wipe the pool, outputs, One-Time Pad message and clipboard
- System DLLs load from System32 only (`SetDefaultDllDirectories`; d3d11, d3dcompiler and dwmapi are loaded at run time from System32; gamepad support is compiled out), so a DLL placed next to the exe is not loaded

### Thread Safety
- `std::atomic` for all shared collector state
- `std::mutex` for entropy pool access
- Atomic `exchange()` pattern for start/stop operations

### Logging
- `DEBUG` log level removed entirely
- No entropy values, timing data, or sample counts in log output
- Generated output is never logged

## Known Limitations

- **Clipboard**: Copied output is excluded from Windows clipboard history and cloud sync and cleared after 60 seconds or at exit, but the copy Windows keeps until then cannot be wiped by the application (OS limitation)
- **Memory locking**: Outputs, One-Time Pad keys and messages are wiped but not locked in RAM, so they can reach the pagefile
- **Entropy Estimation**: Each source is credited conservatively (at most 1 or 2 bits per timing or movement sample, 8 bits per 64 microphone LSBs) and runs the SP 800-90B repetition count and adaptive proportion health tests, but the per-sample caps have not been validated with an SP 800-90B assessment of raw samples
- **AES MixColumns**: Column ordering should be verified against FIPS 197 Appendix B/C test vectors
