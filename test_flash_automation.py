#!/usr/bin/env python3
"""
Test script to verify flash_esp.py automation works without user intervention.
This script simulates the build and flash workflow to identify any interactive prompts.
"""

import subprocess
import sys
from pathlib import Path

def run_command(cmd, description, timeout=300):
    """Run a command and check for completion without hanging."""
    print(f"\n{'='*60}")
    print(f"Testing: {description}")
    print(f"Command: {' '.join(cmd)}")
    print('='*60)
    
    try:
        # Use timeout to detect if process hangs waiting for input
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False
        )
        
        print(f"\nExit Code: {result.returncode}")
        
        if result.returncode == 0:
            print("✓ PASSED - Command completed without hanging")
        else:
            print("⚠ Command failed (but didn't hang)")
            if "No serial port detected" in result.stderr or "No serial port detected" in result.stdout:
                print("  (Expected: No device connected)")
        
        # Check for interactive prompts in output
        interactive_indicators = [
            "(y/N)",
            "Enter",
            "Select",
            "choose",
            "input"
        ]
        
        output = result.stdout + result.stderr
        found_prompts = []
        
        for indicator in interactive_indicators:
            if indicator in output and "no-interactive" not in output.lower():
                found_prompts.append(indicator)
        
        if found_prompts:
            print(f"\n⚠ WARNING: Possible interactive prompts detected:")
            for prompt in found_prompts:
                print(f"  - '{prompt}'")
        else:
            print("\n✓ No interactive prompts detected")
        
        # Show last 20 lines of output
        print("\nLast output lines:")
        print("-" * 60)
        lines = output.strip().split('\n')
        for line in lines[-20:]:
            print(f"  {line}")
        
        return result.returncode
        
    except subprocess.TimeoutExpired:
        print("\n✗ FAILED - Process timed out (likely waiting for user input)")
        return -1
    except Exception as e:
        print(f"\n✗ FAILED - Exception: {e}")
        return -1


def main():
    """Run automation tests."""
    script_dir = Path(__file__).parent
    flash_script = script_dir / "flash_esp.py"
    
    if not flash_script.exists():
        print(f"Error: flash_esp.py not found at {flash_script}")
        sys.exit(1)
    
    print("""
╔════════════════════════════════════════════════════════════════╗
║     flash_esp.py Automation Verification Test Suite           ║
╚════════════════════════════════════════════════════════════════╝

This test will verify that flash_esp.py can run without user interaction
when using automation flags (-y or --no-interactive).

Note: Tests will fail at the actual flashing stage if no device is connected,
but should NOT hang waiting for user input.
""")
    
    tests = [
        {
            'cmd': [sys.executable, str(flash_script), "--help"],
            'desc': "Help text (should show -y and --no-interactive options)",
            'timeout': 10
        },
        {
            'cmd': [sys.executable, str(flash_script), "--build", "-y"],
            'desc': "Build mode with -y flag (full automation)",
            'timeout': 600  # Build can take several minutes
        },
        {
            'cmd': [sys.executable, str(flash_script), "--build", "--no-interactive"],
            'desc': "Build mode with --no-interactive flag",
            'timeout': 600
        },
    ]
    
    results = []
    
    for i, test in enumerate(tests, 1):
        print(f"\n\n{'#'*60}")
        print(f"# TEST {i}/{len(tests)}")
        print(f"{'#'*60}")
        
        exit_code = run_command(
            test['cmd'],
            test['desc'],
            test.get('timeout', 300)
        )
        
        results.append({
            'test': test['desc'],
            'exit_code': exit_code,
            'passed': exit_code != -1  # -1 means timeout (hung waiting for input)
        })
    
    # Print summary
    print(f"\n\n{'='*60}")
    print("TEST SUMMARY")
    print('='*60)
    
    for i, result in enumerate(results, 1):
        status = "✓ PASS" if result['passed'] else "✗ FAIL"
        print(f"{i}. {status} - {result['test']}")
    
    passed = sum(1 for r in results if r['passed'])
    total = len(results)
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    if passed == total:
        print("\n✓ All automation tests passed!")
        print("  flash_esp.py can run without user intervention when using -y or --no-interactive")
        return 0
    else:
        print("\n✗ Some tests failed - manual review needed")
        return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(130)
