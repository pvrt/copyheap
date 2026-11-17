# CopyHeap Project Rules

## Build & Deployment Post-Processing
After every build of the application:
1. Always terminate any running instances of `CopyHeap.exe` (`Stop-Process -Name CopyHeap -Force -ErrorAction SilentlyContinue`).
2. Always copy and strip the built executable to:
   - `d:\dev\copyheap\bin\CopyHeap.exe`
   - `C:\Users\ratnikovpv\Documents\apps\CopyHeap.exe`
