# OTGW-firmware Evaluation Framework

## Overview

The evaluation framework provides comprehensive automated analysis of the OTGW-firmware workspace to ensure code quality, build system health, security, and documentation standards.

## Installation

The evaluation framework is a standalone Python script with no external dependencies beyond Python 3.x.

```bash
# Ensure Python 3 is installed
python --version  # Should be 3.x

# Run evaluation
python evaluate.py
```

## Usage

### Quick Evaluation (Essential Checks Only)

```bash
python evaluate.py --quick
```

Runs essential checks:
- Code structure validation
- Build system verification
- Version information check

### Full Evaluation

```bash
python evaluate.py
```

Runs all evaluation checks:
- Code structure analysis
- Coding standards compliance
- Memory usage patterns
- Build system validation
- Dependency health
- Documentation coverage
- Security analysis
- Git repository health
- Filesystem data verification

### Generate Detailed Report

```bash
python evaluate.py --report
```

Generates a JSON report (`evaluation-report.json`) with detailed results for CI/CD integration or tracking over time.

### Verbose Output

```bash
python evaluate.py --verbose
```

Shows all checks as they run, including passing checks.

### Custom Report File

```bash
python evaluate.py --report --output my-report.json
```

### Disable Colors

```bash
python evaluate.py --no-color
```

Useful for CI/CD environments or when piping output.

## Evaluation Categories

### 1. Code Structure Analysis

**Checks:**
- Required files present (OTGW-firmware.ino, README.md, LICENSE, etc.)
- INO module organization
- Header guards in .h files

**Example Output:**
```
✓ [Structure] Required file: OTGW-firmware.ino: Found (13935 bytes)
✓ [Structure] Header guard: Debug.h: Has header guards
⚠ [Structure] Header guard: version.h: Missing or incomplete header guards
```

### 2. Coding Standards

**Checks:**
- Improper `Serial.print()` usage (should use Debug macros)
- Excessive `String` class usage (heap fragmentation risk)
- Global variable usage patterns
- Magic numbers

**Best Practices:**
- Use `DebugTln()`, `DebugTf()` instead of `Serial.print()`
- Prefer `char` buffers over `String` objects
- Define constants for magic numbers

### 3. Memory Analysis

**Checks:**
- Large buffer allocations (>1KB)
- Total static memory usage
- Heap fragmentation risks

**Example Output:**
```
ℹ [Memory] Large Buffers: Found 3 buffers > 1KB (total: 4096 bytes)
```

### 4. Build System

**Checks:**
- Makefile presence and targets
- build.py script availability
- Essential targets (binaries, clean, upload, filesystem)

**Example Output:**
```
✓ [Build] Makefile: Found Makefile
✓ [Build] Make target: binaries: Target defined
```

### 5. Dependencies

**Checks:**
- Library count and versions
- Version pinning (all libraries should specify @version)
- Dependency conflicts

**Example Output:**
```
ℹ [Dependencies] Library Count: Found 11 library dependencies
⚠ [Dependencies] Version Pinning: Only 10/11 dependencies are version-pinned
```

### 6. Documentation

**Checks:**
- README.md completeness
- Build documentation (BUILD.md, FLASH_GUIDE.md)
- Inline code comment ratio
- Key sections present (Installation, Features, License)

**Target:**
- Comment ratio > 10%
- All key README sections present

### 7. Security Analysis

**Checks:**
- Hardcoded credentials detection
- Unsafe string operations (`strcpy`, `strcat`, `sprintf`, `gets`)
- Buffer overflow risks
- Input validation

**Example Output:**
```
✓ [Security] Hardcoded Credentials: No obvious hardcoded credentials found
⚠ [Security] Unsafe String Ops: Found 14 unsafe string operations
```

### 8. Git Repository

**Checks:**
- Repository initialization
- Current branch
- Uncommitted changes
- .gitignore presence and rules

**Example Output:**
```
ℹ [Git] Current Branch: On branch: main
⚠ [Git] Working Tree: 1 uncommitted changes
```

### 9. Filesystem Data

**Checks:**
- data/ directory presence
- File count and total size
- Web UI files (.html, .css, .js, .json)

**Example Output:**
```
ℹ [Filesystem] data/ content: 33 files, 366114 bytes total
✓ [Filesystem] Web UI files: Found 8 web interface files
```

### 10. Version Information

**Checks:**
- version.h presence
- Semantic version format
- Build number tracking

**Example Output:**
```
ℹ [Version] Version Info: Version: 1.0.0-rc3+f5f651f, Build: 123
```

## Exit Codes

The evaluation script returns different exit codes for automation:

- **0**: All checks passed, warnings ≤ 5
- **1**: One or more checks failed
- **2**: More than 5 warnings (review recommended)
- **130**: Interrupted by user (Ctrl+C)

## Health Score

The health score is calculated as:

```
Health Score = ((PASS + INFO) / TOTAL) × 100%
```

**Interpretation:**
- **≥ 80%**: Good health (green)
- **60-79%**: Acceptable (yellow)
- **< 60%**: Needs attention (red)

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Code Quality

on: [push, pull_request]

jobs:
  evaluate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      - name: Run Evaluation
        run: |
          python evaluate.py --report --no-color
      - name: Upload Report
        uses: actions/upload-artifact@v3
        with:
          name: evaluation-report
          path: evaluation-report.json
```

### Pre-commit Hook

Create `.git/hooks/pre-commit`:

```bash
#!/bin/bash
python evaluate.py --quick --no-color
exit_code=$?
if [ $exit_code -eq 1 ]; then
    echo "❌ Evaluation failed. Fix issues before committing."
    exit 1
fi
exit 0
```

## Report Format

The JSON report contains:

```json
{
  "timestamp": "2026-01-10T12:52:10.123456",
  "project_dir": "/path/to/project",
  "summary": {
    "total": 37,
    "passed": 23,
    "warnings": 9,
    "failed": 0,
    "info": 5
  },
  "results": [
    {
      "category": "Structure",
      "name": "Required file: OTGW-firmware.ino",
      "status": "PASS",
      "message": "Found (13935 bytes)",
      "details": "",
      "timestamp": "2026-01-10T12:52:10.234567"
    }
  ]
}
```

## Customization

### Adding Custom Checks

Edit `evaluate.py` and add a new method to the `WorkspaceEvaluator` class:

```python
def check_my_custom_rule(self):
    """Check my custom rule"""
    print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== My Custom Check ==={Colors.ENDC}")
    
    # Your check logic here
    if condition:
        self.add_result(EvaluationResult(
            "CustomCategory", "Check Name", "PASS",
            "Check passed"
        ))
    else:
        self.add_result(EvaluationResult(
            "CustomCategory", "Check Name", "FAIL",
            "Check failed"
        ))
```

Then call it from `evaluate_all()`:

```python
def evaluate_all(self, quick: bool = False):
    # ... existing checks ...
    if not quick:
        self.check_my_custom_rule()
```

### Adjusting Thresholds

Modify the constants in the check methods:

```python
# Example: Change comment ratio threshold
if total_lines > 0:
    comment_ratio = (comment_lines / total_lines) * 100
    status = "PASS" if comment_ratio > 15 else "WARN"  # Changed from 10 to 15
```

## Common Issues and Solutions

### Issue: "Serial.print() usage" Warning

**Solution:** Replace `Serial.print()` with Debug macros:
```cpp
// Before
Serial.println("Debug message");

// After
DebugTln("Debug message");
```

### Issue: "Unsafe String Ops" Warning

**Solution:** Replace unsafe functions with safe alternatives:
```cpp
// Before
strcpy(buffer, source);

// After
strlcpy(buffer, source, sizeof(buffer));
```

### Issue: "Missing header guards" Warning

**Solution:** Add proper header guards to .h files:
```cpp
#ifndef MY_HEADER_H
#define MY_HEADER_H

// Header content

#endif // MY_HEADER_H
```

### Issue: "Version Pinning" Warning

**Solution:** Specify exact versions in Makefile:
```makefile
# Before
$(CLI) lib install SomeLibrary

# After
$(CLI) lib install SomeLibrary@1.2.3
```

## Troubleshooting

### Evaluation Fails to Run

1. Check Python version: `python --version` (must be 3.x)
2. Ensure you're in the project root directory
3. Check file permissions

### Incorrect Results

1. Run with `--verbose` to see all checks
2. Verify file paths are correct
3. Check if files have unusual encoding

### Performance Issues

1. Use `--quick` for faster evaluation
2. Exclude large directories if needed (modify script)
3. Run on faster hardware or reduce file count

## Best Practices

1. **Run regularly**: Include in your development workflow
2. **Review warnings**: Don't ignore warnings; they indicate potential issues
3. **Track over time**: Save reports and compare to track improvement
4. **Integrate with CI/CD**: Catch issues before they reach main branch
5. **Customize for your needs**: Adjust thresholds and add project-specific checks

## Future Enhancements

Potential additions to the framework:

- [ ] Static analysis integration (cppcheck, clang-tidy)
- [ ] Complexity metrics (cyclomatic complexity)
- [ ] Test coverage integration
- [ ] Performance benchmarking
- [ ] Automated fix suggestions
- [ ] Trend analysis over multiple evaluations
- [ ] Integration with code review tools

## Contributing

To contribute to the evaluation framework:

1. Test changes thoroughly
2. Update documentation
3. Add tests for new checks
4. Follow existing code style
5. Submit pull request

## License

This evaluation framework is part of the OTGW-firmware project and follows the same license terms.
