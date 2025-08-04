#!/usr/bin/env python3
"""
Check Python dependencies for neuromorphic event streaming
"""

import sys

def check_dependency(name, package=None):
    """Check if a dependency is available"""
    if package is None:
        package = name
    
    try:
        __import__(package)
        print(f"[OK] {name} is available")
        return True
    except ImportError:
        print(f"[MISSING] {name} is NOT available (install with: pip install {package})")
        return False

def main():
    print("Checking Python dependencies for neuromorphic event streaming...")
    print("=" * 60)
    
    # Required dependencies
    required = [
        ("NumPy", "numpy"),
        ("Event Stream", "event_stream"),
    ]
    
    # Optional dependencies (for future BindsNET integration)
    optional = [
        ("PyTorch", "torch"),
        ("BindsNET", "bindsnet"),
    ]
    
    all_required_available = True
    
    print("Required dependencies:")
    for name, package in required:
        if not check_dependency(name, package):
            all_required_available = False
    
    print("\nOptional dependencies (for SNN integration):")
    for name, package in optional:
        check_dependency(name, package)
    
    print("\n" + "=" * 60)
    
    if all_required_available:
        print("[SUCCESS] All required dependencies are available!")
        print("You can now test the event streaming pipeline.")
    else:
        print("[ERROR] Some required dependencies are missing.")
        print("Install missing packages with:")
        print("  pip install numpy event_stream")
        print("  # Optional: pip install torch bindsnet")
    
    return 0 if all_required_available else 1

if __name__ == "__main__":
    sys.exit(main())