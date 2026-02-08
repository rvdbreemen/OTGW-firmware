---
# METADATA
Document Title: Configuration Strategy Evaluation: .env vs config.py
Review Date: 2026-02-06
Target Version: 1.0.0
Reviewer: GitHub Copilot
Document Type: Architecture Analysis
Status: COMPLETE
---

# Configuration Strategy Evaluation

## Executive Summary

This document evaluates the optimal configuration strategy for the OTGW-firmware project's Python tooling (`build.py`, `evaluate.py`, `flash_esp.py`). Specifically, it compares the usage of Dotenv (`.env`) files versus a native Python configuration module (`config.py`).

**Recommendation:** Adopt a **Hybrid `config.py`** approach.
Use a dedicated Python module (`config.py`) that defines structural defaults (file paths, project names) while optionally allowing overrides via environment variables. This combines the reliability of code-based configuration for CI/CD with the flexibility of environment variables for local development.

## Context

The project currently uses Python scripts for:

1. **Build Automation** (`build.py`): Compiling firmware and filesystem.
2. **Quality Assurance** (`evaluate.py`): Analyzing code quality and structure.
3. **Flashing** (`flash_esp.py`): interacting with hardware.

These scripts need to know:

- Where source files are located (`src/OTGW-firmware`).
- Where to output artifacts (`build/`).
- Project metadata (`PROJECT_NAME`).

## Analysis: Dotenv (`.env`) Approach

This approach involves storing variables in a `.env` file, which is ignored by git, and parsing it at runtime.

### Advantages

- **Standard Implementation**: The `.env` pattern is widely understood across many languages.
- **Secrets Management**: Excellent for keeping secrets (API keys, passwords) out of source control.
- **Local Customization**: Developers can easily change their local `BUILD_DIR` without affecting others.

### Drawbacks

- **Parsing Overhead**: Scripts must implement or import parsing logic (as seen in the custom `load_env` function recently added).
- **Type Safety**: All values are strings. "True" vs "true" vs "1" requires careful parsing.
- **CI/CD Friction**: In GitHub Actions, the `.env` file does not exist. The scripts rely on the *defaults* hardcoded in the script's dictionary anyway, effectively duplicating the "source of truth" (once in `.env.example` and once in the script's fallback defaults).
- **Structural Coupling**: Variables like `FIRMWARE_ROOT` are structural constants. They *must* match the git repository layout. Extracting them to an ignored file creates a risk where the config drifts from the actual file structure.

## Analysis: Python Module (`config.py`) Approach

This approach involves a dedicated `config.py` file checked into the repository.

```python
# config.py
from pathlib import Path
import os

PROJECT_ROOT = Path(__file__).parent.resolve()
FIRMWARE_ROOT = PROJECT_ROOT / "src" / "OTGW-firmware"
BUILD_DIR = PROJECT_ROOT / os.getenv("OTGW_BUILD_DIR", "build")
```

### Benefits

- **Single Source of Truth**: The definitions exist in one place, checked into git.
- **Native Types**: Supports `Path` objects, lists, and booleans natively. No string parsing needed.
- **CI/CD Readiness**: Works out-of-the-box. GitHub Actions check out the code, and `config.py` is there with the correct structural paths.
- **Autocomplete**: IDEs can suggest available configuration variables.
- **Robustness**: Logic (like making paths relative to `__file__`) ensures paths work regardless of where the script is executed from.

### Limitations

- **Secrets Risk**: Care must be taken not to commit secrets to `config.py`. (However, this project's build variables are structural, not secrets).

## CI/CD Pipeline Implications

### With `.env`

1. **Local**: `load_env()` parses local `.env`.
2. **CI**: `load_env()` finds no `.env`. It falls back to the default dict defined in the script.
   - *Risk:* If the default dict in `build.py` differs from `.env.example`, the CI build behaves differently than local dev.

### With `config.py`

1. **Local**: Script imports `config`. Paths are calculated relative to the file.
2. **CI**: Script imports `config`. Paths are calculated exactly the same way.
   - *Benefit:* Guaranteed consistency between local and CI environments.

## Detailed Comparison Matrix

| Feature | .env | config.py (Recommended) |
| :--- | :--- | :--- |
| **Structural Constants** | Weak (Defined in ignored file) | **Strong** (Defined in code) |
| **Secrets Management** | **Strong** | Weak (Unless split) |
| **CI/CD Integration** | Requires Env Vars / Defaults | **Native** |
| **Type Safety** | String parsing required | **Native Python Types** |
| **Developer Experience** | Must copy `.env.example` | Works immediately |
| **Maintenance** | Update Python dict + `.env.example` | Update `config.py` only |

## Final Recommendation

For **OTGW-firmware**, the variables in question (`FIRMWARE_ROOT`, `DATA_DIR`) are **structural constants**, not secrets or deployment parameters.

Therefore, **`config.py` is superior**.

### Implementation Steps

1. Create `config.py` in the root:

   ```python
   import os
   from pathlib import Path

   # Base Paths
   PROJECT_DIR = Path(__file__).parent.resolve()
   
   # Structural Config (Fixed)
   PROJECT_NAME = "OTGW-firmware"
   FIRMWARE_ROOT = PROJECT_DIR / "src" / "OTGW-firmware"
   DATA_DIR = FIRMWARE_ROOT / "data"
   
   # Environment Config (Overridable)
   BUILD_DIR = PROJECT_DIR / os.getenv("BUILD_DIR", "build")
   ```

2. Update scripts to import it:

   ```python
   import config
   # Use config.FIRMWARE_ROOT
   ```

This removes the need for `load_env` boilerplate in every script and ensures that `evaluate.py`, `build.py`, and `flash_esp.py` always agree on where the files are.
