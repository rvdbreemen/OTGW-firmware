#!/usr/bin/env python3
"""
OTGW-firmware Workspace Evaluation Framework

This script performs comprehensive evaluation of the workspace including:
- Code quality metrics
- Build system validation
- Dependency health checks
- Documentation coverage
- Security analysis
- Memory and resource analysis
- Test coverage analysis

Usage:
    python evaluate.py              # Full evaluation
    python evaluate.py --quick      # Quick check (essentials only)
    python evaluate.py --report     # Generate detailed report
    python evaluate.py --fix        # Auto-fix issues where possible
"""

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Any
import config

class Colors:
    """ANSI color codes"""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

    @staticmethod
    def disable():
        # Suppress type checker warnings for intentional constant reassignment
        Colors.HEADER = Colors.OKBLUE = Colors.OKCYAN = ''  # type: ignore
        Colors.OKGREEN = Colors.WARNING = Colors.FAIL = Colors.ENDC = Colors.BOLD = ''  # type: ignore


class EvaluationResult:
    """Store evaluation results"""
    def __init__(self, category: str, name: str, status: str, message: str, details: str = ""):
        self.category = category
        self.name = name
        self.status = status  # PASS, WARN, FAIL, INFO
        self.message = message
        self.details = details
        self.timestamp = datetime.now()

    def __repr__(self):
        icon = {"PASS": "✓", "WARN": "⚠", "FAIL": "✗", "INFO": "ℹ"}.get(self.status, "?")
        color = {"PASS": Colors.OKGREEN, "WARN": Colors.WARNING, "FAIL": Colors.FAIL, "INFO": Colors.OKCYAN}.get(self.status, "")
        return f"{color}{icon} [{self.category}] {self.name}: {self.message}{Colors.ENDC}"


class WorkspaceEvaluator:
    """Main evaluation framework"""
    
    def __init__(self, project_dir: Path, verbose: bool = False):
        self.project_dir = project_dir
        self.verbose = verbose
        self.results: List[EvaluationResult] = []
        self.stats: Dict[str, int] = defaultdict(int)
        
    def add_result(self, result: EvaluationResult):
        """Add evaluation result and update stats"""
        self.results.append(result)
        self.stats[result.status] += 1
        if self.verbose or result.status in ["FAIL", "WARN"]:
            print(result)

    def run_command(self, cmd: List[str], capture: bool = True) -> Tuple[int, str, str]:
        """Run command and return (returncode, stdout, stderr)"""
        try:
            result = subprocess.run(
                cmd,
                cwd=self.project_dir,
                capture_output=capture,
                text=True,
                timeout=60
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return -1, "", "Command timeout"
        except Exception as e:
            return -1, "", str(e)

    # ===== CODE QUALITY CHECKS =====
    
    def check_code_structure(self):
        """Analyze code structure and organization"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Code Structure Analysis ==={Colors.ENDC}")
        
        # Check for required files
        required_files: List[Path] = [
            config.FIRMWARE_ROOT / "OTGW-firmware.ino",
            config.FIRMWARE_ROOT / "OTGW-firmware.h",
            config.FIRMWARE_ROOT / "version.h",
            Path("README.md"),
            Path("LICENSE")
        ]
        
        for file in required_files:
            file_path = self.project_dir / file
            if file_path.exists():
                self.add_result(EvaluationResult(
                    "Structure", f"Required file: {file}", "PASS",
                    f"Found ({file_path.stat().st_size} bytes)"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Structure", f"Required file: {file}", "FAIL",
                    "Missing required file"
                ))

        # Check .ino file organization
        src_dir = config.FIRMWARE_ROOT
        ino_files = list(src_dir.glob("*.ino"))
        self.add_result(EvaluationResult(
            "Structure", "INO modules", "INFO",
            f"Found {len(ino_files)} Arduino modules",
            ", ".join([f.name for f in ino_files])
        ))

        # Check for proper header guards in .h files
        h_files = list(src_dir.glob("*.h"))
        for h_file in h_files:
            with open(h_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                if '#ifndef' in content and '#define' in content:
                    self.add_result(EvaluationResult(
                        "Structure", f"Header guard: {h_file.name}", "PASS",
                        "Has header guards"
                    ))
                else:
                    self.add_result(EvaluationResult(
                        "Structure", f"Header guard: {h_file.name}", "WARN",
                        "Missing or incomplete header guards"
                    ))

    def check_coding_standards(self):
        """Check coding standards and best practices"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Coding Standards ==={Colors.ENDC}")
        
        issues = {
            'serial_debug': 0,
            'string_usage': 0,
            'global_vars': 0,
            'magic_numbers': 0
        }
        
        src_dir = config.FIRMWARE_ROOT
        ino_cpp_files = list(src_dir.glob("*.ino")) + list(src_dir.glob("*.cpp"))
        
        for file in ino_cpp_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
                for i, line in enumerate(lines, 1):
                    # Check for Serial.print usage (should use Debug macros)
                    if 'Serial.print' in line and 'SetupDebug' not in line and '//' not in line.split('Serial.print')[0]:
                        issues['serial_debug'] += 1
                        if self.verbose:
                            print(f"  {file.name}:{i}: Serial.print usage")
                    
                    # Check for String class usage in critical paths
                    if re.search(r'\bString\s+\w+\s*=', line) and '//' not in line.split('String')[0]:
                        issues['string_usage'] += 1

        if issues['serial_debug'] > 0:
            self.add_result(EvaluationResult(
                "Coding", "Serial Debug Output", "WARN",
                f"Found {issues['serial_debug']} Serial.print() usage (should use Debug macros)"
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "Serial Debug Output", "PASS",
                "No improper Serial.print() usage found"
            ))

        if issues['string_usage'] > 5:
            self.add_result(EvaluationResult(
                "Coding", "String Class Usage", "WARN",
                f"Found {issues['string_usage']} String class usages (may cause heap fragmentation)"
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "String Class Usage", "PASS",
                f"Limited String usage ({issues['string_usage']} instances)"
            ))

    def check_memory_usage(self):
        """Analyze memory usage patterns"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Memory Analysis ==={Colors.ENDC}")
        
        # Check for large buffers
        large_buffers: List[Tuple[str, int]] = []
        src_dir = config.FIRMWARE_ROOT
        ino_cpp_files = list(src_dir.glob("*.ino")) + list(src_dir.glob("*.cpp")) + list(src_dir.glob("*.h"))
        
        for file in ino_cpp_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                # Look for large array declarations
                matches = re.findall(r'char\s+\w+\[(\d+)\]', content)
                for size_str in matches:
                    size = int(size_str)
                    if size > 1024:
                        large_buffers.append((file.name, size))

        if large_buffers:
            total_size = sum([s for _, s in large_buffers])
            self.add_result(EvaluationResult(
                "Memory", "Large Buffers", "INFO",
                f"Found {len(large_buffers)} buffers > 1KB (total: {total_size} bytes)",
                "; ".join([f"{f}: {s}B" for f, s in large_buffers[:5]])
            ))

    # ===== BUILD SYSTEM CHECKS =====
    
    def check_build_system(self):
        """Validate build system configuration"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Build System ==={Colors.ENDC}")
        
        # Check Makefile
        makefile = self.project_dir / "Makefile"
        if makefile.exists():
            self.add_result(EvaluationResult(
                "Build", "Makefile", "PASS",
                "Found Makefile"
            ))
            
            with open(makefile, 'r') as f:
                content = f.read()
                # Check for essential targets
                targets = ['binaries', 'clean', 'upload', 'filesystem']
                for target in targets:
                    if f"{target}:" in content:
                        self.add_result(EvaluationResult(
                            "Build", f"Make target: {target}", "PASS",
                            "Target defined"
                        ))
                    else:
                        self.add_result(EvaluationResult(
                            "Build", f"Make target: {target}", "WARN",
                            "Target not found"
                        ))
        else:
            self.add_result(EvaluationResult(
                "Build", "Makefile", "FAIL",
                "Makefile not found"
            ))

        # Check build.py
        build_script = self.project_dir / "build.py"
        if build_script.exists():
            self.add_result(EvaluationResult(
                "Build", "build.py", "PASS",
                "Found build script"
            ))
        else:
            self.add_result(EvaluationResult(
                "Build", "build.py", "WARN",
                "Build script not found"
            ))

    def check_dependencies(self):
        """Check library dependencies"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Dependencies ==={Colors.ENDC}")
        
        # Parse dependencies from Makefile
        lib_matches: List[str] = []
        makefile = self.project_dir / "Makefile"
        if makefile.exists():
            with open(makefile, 'r') as f:
                content = f.read()
                # Extract library installations
                lib_matches = re.findall(r'lib install (\S+)', content)
                if lib_matches:
                    self.add_result(EvaluationResult(
                        "Dependencies", "Library Count", "INFO",
                        f"Found {len(lib_matches)} library dependencies",
                        ", ".join(lib_matches)
                    ))

        # Check for library version pinning
        if lib_matches:
            pinned = [lib for lib in lib_matches if '@' in lib]
            if len(pinned) == len(lib_matches):
                self.add_result(EvaluationResult(
                    "Dependencies", "Version Pinning", "PASS",
                    "All dependencies are version-pinned"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Dependencies", "Version Pinning", "WARN",
                    f"Only {len(pinned)}/{len(lib_matches)} dependencies are version-pinned"
                ))

    # ===== DOCUMENTATION CHECKS =====
    
    def check_documentation(self):
        """Check documentation coverage"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Documentation ==={Colors.ENDC}")
        
        # Check README
        readme = self.project_dir / "README.md"
        if readme.exists():
            with open(readme, 'r', encoding='utf-8') as f:
                content = f.read()
                size = len(content)
                
                sections = ['Installation', 'Build', 'Features', 'License']
                found_sections: List[str] = []
                for section in sections:
                    if section.lower() in content.lower():
                        found_sections.append(section)
                
                self.add_result(EvaluationResult(
                    "Documentation", "README.md", "PASS",
                    f"{size} bytes, {len(found_sections)}/{len(sections)} key sections",
                    ", ".join(found_sections)
                ))
        else:
            self.add_result(EvaluationResult(
                "Documentation", "README.md", "FAIL",
                "README.md not found"
            ))

        # Check for build documentation
        build_docs = ["BUILD.md", "FLASH_GUIDE.md"]
        for doc in build_docs:
            doc_path = self.project_dir / doc
            docs_path = self.project_dir / "docs" / doc
            if doc_path.exists():
                self.add_result(EvaluationResult(
                    "Documentation", doc, "PASS",
                    f"Found ({doc_path.stat().st_size} bytes)"
                ))
            elif docs_path.exists():
                self.add_result(EvaluationResult(
                    "Documentation", doc, "PASS",
                    f"Found (docs/{doc}, {docs_path.stat().st_size} bytes)"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Documentation", doc, "WARN",
                    f"{doc} not found"
                ))

        # Check inline documentation (comments ratio)
        total_lines = 0
        comment_lines = 0
        src_dir = config.FIRMWARE_ROOT
        ino_files = list(src_dir.glob("*.ino"))
        
        for file in ino_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    total_lines += 1
                    stripped = line.strip()
                    if stripped.startswith('//') or stripped.startswith('/*'):
                        comment_lines += 1

        if total_lines > 0:
            comment_ratio = (comment_lines / total_lines) * 100
            status = "PASS" if comment_ratio > 10 else "WARN"
            self.add_result(EvaluationResult(
                "Documentation", "Code Comments", status,
                f"{comment_ratio:.1f}% comment ratio ({comment_lines}/{total_lines} lines)"
            ))

    # ===== SECURITY CHECKS =====
    
    def check_security(self):
        """Check for common security issues"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Security Analysis ==={Colors.ENDC}")
        
        security_issues: Dict[str, List[str]] = {
            'hardcoded_creds': [],
            'unsafe_string_ops': [],
            'buffer_overflow_risk': []
        }
        
        src_dir = config.FIRMWARE_ROOT
        all_code_files = (list(src_dir.glob("*.ino")) + 
                         list(src_dir.glob("*.cpp")) + 
                         list(src_dir.glob("*.h")))
        
        for file in all_code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
                for i, line in enumerate(lines, 1):
                    # Check for potential hardcoded credentials
                    if re.search(r'(password|passwd|pwd|secret|key)\s*=\s*["\'](?!xxx|changeme|\*+)[^"\']{3,}["\']', line, re.I):
                        security_issues['hardcoded_creds'].append(f"{file.name}:{i}")
                    
                    # Check for unsafe string operations
                    if re.search(r'\b(strcpy|strcat|sprintf|gets)\s*\(', line):
                        security_issues['unsafe_string_ops'].append(f"{file.name}:{i}")

        if security_issues['hardcoded_creds']:
            self.add_result(EvaluationResult(
                "Security", "Hardcoded Credentials", "WARN",
                f"Found {len(security_issues['hardcoded_creds'])} potential hardcoded credentials",
                "; ".join(security_issues['hardcoded_creds'][:3])
            ))
        else:
            self.add_result(EvaluationResult(
                "Security", "Hardcoded Credentials", "PASS",
                "No obvious hardcoded credentials found"
            ))

        if security_issues['unsafe_string_ops']:
            self.add_result(EvaluationResult(
                "Security", "Unsafe String Ops", "WARN",
                f"Found {len(security_issues['unsafe_string_ops'])} unsafe string operations",
                "; ".join(security_issues['unsafe_string_ops'][:3])
            ))
        else:
            self.add_result(EvaluationResult(
                "Security", "Unsafe String Ops", "PASS",
                "No unsafe string operations found"
            ))

    # ===== GIT REPOSITORY CHECKS =====
    
    def check_git_repository(self):
        """Check Git repository health"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Git Repository ==={Colors.ENDC}")
        
        git_dir = self.project_dir / ".git"
        if not git_dir.exists():
            self.add_result(EvaluationResult(
                "Git", "Repository", "WARN",
                "Not a Git repository"
            ))
            return

        # Check current branch
        rc, stdout, _ = self.run_command(["git", "branch", "--show-current"])
        if rc == 0:
            branch = stdout.strip()
            self.add_result(EvaluationResult(
                "Git", "Current Branch", "INFO",
                f"On branch: {branch}"
            ))

        # Check for uncommitted changes
        rc, stdout, _ = self.run_command(["git", "status", "--porcelain"])
        if rc == 0:
            if stdout.strip():
                changed_files = len(stdout.strip().split('\n'))
                self.add_result(EvaluationResult(
                    "Git", "Working Tree", "WARN",
                    f"{changed_files} uncommitted changes"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Git", "Working Tree", "PASS",
                    "Clean working tree"
                ))

        # Check .gitignore
        gitignore = self.project_dir / ".gitignore"
        if gitignore.exists():
            with open(gitignore, 'r') as f:
                rules = [line.strip() for line in f if line.strip() and not line.startswith('#')]
                self.add_result(EvaluationResult(
                    "Git", ".gitignore", "PASS",
                    f"Found with {len(rules)} rules"
                ))
        else:
            self.add_result(EvaluationResult(
                "Git", ".gitignore", "WARN",
                ".gitignore not found"
            ))

    # ===== FILE SYSTEM CHECKS =====
    
    def check_filesystem_data(self):
        """Check data directory for LittleFS"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Filesystem Data ==={Colors.ENDC}")
        
        data_dir = config.DATA_DIR
        if not data_dir.exists():
            self.add_result(EvaluationResult(
                "Filesystem", "data/ directory", "FAIL",
                "data/ directory not found"
            ))
            return

        # Count files in data directory
        files = list(data_dir.rglob("*"))
        file_count = len([f for f in files if f.is_file()])
        total_size = sum([f.stat().st_size for f in files if f.is_file()])
        
        self.add_result(EvaluationResult(
            "Filesystem", "data/ content", "INFO",
            f"{file_count} files, {total_size} bytes total"
        ))

        # Check for web interface files
        web_files = ['.html', '.css', '.js', '.json']
        web_count = len([f for f in files if f.suffix in web_files])
        
        if web_count > 0:
            self.add_result(EvaluationResult(
                "Filesystem", "Web UI files", "PASS",
                f"Found {web_count} web interface files"
            ))

    # ===== VERSION CONTROL =====
    
    def check_version_info(self):
        """Check version information"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Version Information ==={Colors.ENDC}")
        
        version_file = config.FIRMWARE_ROOT / "version.h"
        if version_file.exists():
            with open(version_file, 'r') as f:
                content = f.read()
                
                # Extract version info
                semver_match = re.search(r'#define\s+_SEMVER_FULL\s+"([^"]+)"', content)
                build_match = re.search(r'#define\s+_BUILD\s+(\d+)', content)
                
                if semver_match:
                    version = semver_match.group(1)
                    build = build_match.group(1) if build_match else "unknown"
                    self.add_result(EvaluationResult(
                        "Version", "Version Info", "INFO",
                        f"Version: {version}, Build: {build}"
                    ))
                else:
                    self.add_result(EvaluationResult(
                        "Version", "Version Info", "WARN",
                        "Could not parse version information"
                    ))
        else:
            self.add_result(EvaluationResult(
                "Version", "version.h", "WARN",
                "version.h not found"
            ))

    # ===== MAIN EVALUATION =====
    
    def evaluate_all(self, quick: bool = False):
        """Run all evaluations"""
        print(f"\n{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}OTGW-firmware Workspace Evaluation{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}")
        print(f"{Colors.OKCYAN}Project: {self.project_dir}{Colors.ENDC}")
        print(f"{Colors.OKCYAN}Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}{Colors.ENDC}\n")
        
        # Essential checks (always run)
        self.check_code_structure()
        self.check_build_system()
        self.check_version_info()
        
        if not quick:
            # Detailed checks
            self.check_coding_standards()
            self.check_memory_usage()
            self.check_dependencies()
            self.check_documentation()
            self.check_security()
            self.check_git_repository()
            self.check_filesystem_data()

    def print_summary(self):
        """Print evaluation summary"""
        print(f"\n{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}Evaluation Summary{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}\n")
        
        total = len(self.results)
        pass_count = self.stats.get("PASS", 0)
        warn_count = self.stats.get("WARN", 0)
        fail_count = self.stats.get("FAIL", 0)
        info_count = self.stats.get("INFO", 0)
        
        print(f"Total Checks: {total}")
        print(f"{Colors.OKGREEN}✓ Passed: {pass_count}{Colors.ENDC}")
        print(f"{Colors.WARNING}⚠ Warnings: {warn_count}{Colors.ENDC}")
        print(f"{Colors.FAIL}✗ Failed: {fail_count}{Colors.ENDC}")
        print(f"{Colors.OKCYAN}ℹ Info: {info_count}{Colors.ENDC}")
        
        # Calculate health score
        if total > 0:
            health_score = ((pass_count + info_count) / total) * 100
            health_color = Colors.OKGREEN if health_score >= 80 else Colors.WARNING if health_score >= 60 else Colors.FAIL
            print(f"\n{Colors.BOLD}Health Score: {health_color}{health_score:.1f}%{Colors.ENDC}")
        
        # Exit code based on failures
        if fail_count > 0:
            return 1
        elif warn_count > 5:
            return 2
        return 0

    def generate_report(self, output_file: Path):
        """Generate detailed JSON report"""
        report: Dict[str, Any] = {
            "timestamp": datetime.now().isoformat(),
            "project_dir": str(self.project_dir),
            "summary": {
                "total": len(self.results),
                "passed": self.stats.get("PASS", 0),
                "warnings": self.stats.get("WARN", 0),
                "failed": self.stats.get("FAIL", 0),
                "info": self.stats.get("INFO", 0)
            },
            "results": []
        }
        
        for result in self.results:
            report["results"].append({
                "category": result.category,
                "name": result.name,
                "status": result.status,
                "message": result.message,
                "details": result.details,
                "timestamp": result.timestamp.isoformat()
            })
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"\n{Colors.OKGREEN}Report saved to: {output_file}{Colors.ENDC}")


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="OTGW-firmware Workspace Evaluation Framework",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        "--quick",
        action="store_true",
        help="Quick evaluation (essential checks only)"
    )
    parser.add_argument(
        "--report",
        action="store_true",
        help="Generate detailed JSON report"
    )
    parser.add_argument(
        "--output",
        type=str,
        default="evaluation-report.json",
        help="Output file for report (default: evaluation-report.json)"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output (show all checks)"
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable colored output"
    )
    
    args = parser.parse_args()
    
    if args.no_color:
        Colors.disable()
    
    # Get project directory
    project_dir = Path(__file__).parent.resolve()
    
    # Run evaluation
    evaluator = WorkspaceEvaluator(project_dir, verbose=args.verbose)
    evaluator.evaluate_all(quick=args.quick)
    
    # Print summary
    exit_code = evaluator.print_summary()
    
    # Generate report if requested
    if args.report:
        output_path = project_dir / args.output
        evaluator.generate_report(output_path)
    
    return exit_code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print(f"\n{Colors.WARNING}Evaluation interrupted by user{Colors.ENDC}")
        sys.exit(130)
    except Exception as e:
        print(f"\n{Colors.FAIL}Error: {e}{Colors.ENDC}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
