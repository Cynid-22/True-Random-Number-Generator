# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest  | ✅        |

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
- Quad-layer pipeline validated against **NIST SP 800-22** (all 15 tests passed)

### Memory Security
- `SecureZeroMemory` used for all sensitive buffers
- `std::vector` with explicit wiping preferred over `std::string`
- `shrink_to_fit()` called after buffer swaps to release heap allocations

### Thread Safety
- `std::atomic` for all shared collector state
- `std::mutex` for entropy pool access
- Atomic `exchange()` pattern for start/stop operations

### Logging
- `DEBUG` log level removed entirely
- No entropy values, timing data, or sample counts in log output
- Generated output is never logged

## Known Limitations

- **Clipboard**: Output copied to the clipboard cannot be securely wiped by the application (OS limitation)
- **Entropy Estimation**: Conservative estimates are used (e.g., 32 bits/sample for microphone) but have not been formally verified per NIST SP 800-90B
- **AES MixColumns**: Column ordering should be verified against FIPS 197 Appendix B/C test vectors
