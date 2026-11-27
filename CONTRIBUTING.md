# Contributing to TRNG

Thank you for your interest in contributing! This project deals with cryptographic security, so all contributions must meet a high standard of correctness and safety.

## Getting Started

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Make your changes
4. Run `./build_tests.sh`, and test the changed feature in the app
5. Submit a pull request

## Development Setup

The requirements and build steps are in the README's [Building](README.md#building) section. In short, in the MSYS2 UCRT64 terminal:

```bash
./build.sh         # builds build/TRNG.exe
./build_tests.sh   # builds and runs the regression tests
```

## Guidelines

### Code Style
- Match the indentation of the file you edit (some files use 2 spaces, some 4)
- Follow existing naming conventions (`camelCase` for variables, `PascalCase` for classes/functions)
- Keep comments to a minimum: clear names and named constants should explain the code
- Comment only a non-obvious reason that no name can express, in one short line

### Security Requirements

Since this is a cryptographic application, all contributions **must**:

- **No logging**: TRNG has no logger. Never write entropy values, keys or generated output to a file, the console or the debugger output, except a file the user exports
- **Wipe all buffers**: Use `SecureZeroMemory` for any buffer that held sensitive data
- **Use atomic operations**: All shared state between threads must use `std::atomic` or `std::mutex`
- **Avoid `std::string` for secrets**: Use `std::vector<char>` or `std::vector<uint8_t>` with explicit secure wiping

### Pull Request Checklist

- [ ] Code compiles without new warnings (check once with `-Wall -Wextra` added to the `g++` line in `build.sh`)
- [ ] Regression tests pass (`./build_tests.sh`), and new behavior has a test in `tests/`
- [ ] No sensitive data written anywhere except user exports
- [ ] All temporary buffers are securely wiped
- [ ] Thread safety verified for shared state
- [ ] Existing functionality is not broken

## Reporting Issues

- For **bugs**, open a GitHub Issue with steps to reproduce
- For **security vulnerabilities**, see [SECURITY.md](SECURITY.md)

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
