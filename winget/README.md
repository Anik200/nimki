# Winget Package Manifests for Nimki

This directory contains the package manifests for submitting Nimki to the [Windows Package Manager Community Repository](https://github.com/microsoft/winget-pkgs).

## Submission Steps

### Option 1: GitHub Pull Request (Recommended)
1. Fork [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).
2. Copy `manifests/a/Anik200/nimki/0.1.4` into the fork at `manifests/a/Anik200/nimki/0.1.4`.
3. Push to a branch on your fork and open a Pull Request titled `New package: Anik200.nimki version 0.1.4`.
4. The automated verification bots will test the package and merge the PR.

### Option 2: WingetCreate CLI
```powershell
winget install Microsoft.WingetCreate
wingetcreate submit .\manifests\a\Anik200\nimki\0.1.4 --token <GITHUB_PERSONAL_ACCESS_TOKEN>
```

## Local Validation
To validate the manifests before submitting:
```powershell
winget validate --manifest .\manifests\a\Anik200\nimki\0.1.4
```