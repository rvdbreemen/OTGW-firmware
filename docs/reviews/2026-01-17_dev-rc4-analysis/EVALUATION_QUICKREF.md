# Evaluation Framework Quick Reference

## Quick Commands

```bash
# Quick check (30 seconds)
python evaluate.py --quick

# Full evaluation
python evaluate.py

# Generate report for CI/CD
python evaluate.py --report --no-color

# Verbose output
python evaluate.py --verbose

# Help
python evaluate.py --help
```

## Status Indicators

- ✓ **PASS** (Green): Check passed
- ⚠ **WARN** (Yellow): Warning - review recommended
- ✗ **FAIL** (Red): Failed - action required
- ℹ **INFO** (Cyan): Informational

## Health Score Guide

- **80-100%**: Excellent ✓
- **60-79%**: Good, minor improvements needed
- **40-59%**: Needs attention
- **<40%**: Critical issues

## Common Warnings & Fixes

### Serial.print() Usage
```cpp
❌ Serial.println("message");
✓ DebugTln("message");
```

### Missing Header Guards
```cpp
❌ // No guards

✓ #ifndef MY_HEADER_H
✓ #define MY_HEADER_H
   // content
✓ #endif
```

### Unsafe String Operations
```cpp
❌ strcpy(dest, src);
✓ strlcpy(dest, src, sizeof(dest));
```

### String Class Usage
```cpp
❌ String msg = "Hello";
✓ char msg[32];
✓ strlcpy(msg, "Hello", sizeof(msg));
```

## Exit Codes

- `0` - All good
- `1` - Failures detected
- `2` - Too many warnings (>5)

## Categories

1. **Structure** - File organization
2. **Coding** - Code standards
3. **Memory** - Memory usage
4. **Build** - Build system
5. **Dependencies** - Library versions
6. **Documentation** - Docs coverage
7. **Security** - Security issues
8. **Git** - Repository health
9. **Filesystem** - Data files
10. **Version** - Version info

## Integration Examples

### Pre-commit Hook
```bash
#!/bin/bash
python evaluate.py --quick --no-color || exit 1
```

### CI/CD (GitHub Actions)
```yaml
- run: python evaluate.py --report --no-color
- uses: actions/upload-artifact@v3
  with:
    name: eval-report
    path: evaluation-report.json
```

### Make Target
```makefile
.PHONY: evaluate
evaluate:
	python evaluate.py --report
```

## Tips

- Run `--quick` during development
- Run full evaluation before commits
- Generate reports for tracking trends
- Customize thresholds in `evaluate.py`
- Add to CI/CD for automated checks
