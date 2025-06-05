# PowerShell Script to Clean and Fix the CipherGUI Repository
#
# This script will:
# 1. Restructure the lib/glfw directory correctly.
# 2. Rename Makefiles to the .win/.mac convention.
# 3. Update .gitignore and clean the Git index.
#
# To run:
# 1. Open PowerShell in the project root.
# 2. Run: Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
# 3. Run: .\fix_repo.ps1

Write-Host "--- Starting Repository Fix Script ---" -ForegroundColor Yellow

# --- Step 1: Fix the GLFW Directory Structure ---
$glfw_root = ".\lib\glfw"
$bad_glfw_dir = Join-Path $glfw_root "glfw-3.4.bin.WIN64" # Adjust version if needed

if (Test-Path $bad_glfw_dir) {
    Write-Host "[1/4] Incorrect GLFW directory found. Restructuring..."

    # Define source paths for the two folders we need
    $source_include = Join-Path $bad_glfw_dir "include"
    $source_lib = Join-Path $bad_glfw_dir "lib-mingw-w64"

    # Check if the needed folders exist before moving
    if ((Test-Path $source_include) -and (Test-Path $source_lib)) {
        # Move the two essential folders up to the lib/glfw root
        Move-Item -Path $source_include -Destination $glfw_root
        Move-Item -Path $source_lib -Destination $glfw_root
        
        # Delete the rest of the junk from the original extraction
        Write-Host "    - Deleting leftover GLFW files and folders..."
        Remove-Item -Path $bad_glfw_dir -Recurse -Force
        Remove-Item -Path (Join-Path $glfw_root "glfw3.h") -ErrorAction SilentlyContinue
        Remove-Item -Path (Join-Path $glfw_root "glfw3native.h") -ErrorAction SilentlyContinue

        Write-Host "    - GLFW directory fixed successfully." -ForegroundColor Green
    } else {
        Write-Host "    - ERROR: Could not find 'include' or 'lib-mingw-w64' in the extracted folder." -ForegroundColor Red
        Write-Host "    - Please re-download GLFW and extract it into lib/glfw." -ForegroundColor Red
        exit
    }
} else {
    Write-Host "[1/4] GLFW directory structure seems ok or is missing. Skipping restructure."
}

# --- Step 2: Correct Makefile Naming ---
Write-Host "[2/4] Standardizing Makefile names..."
if (Test-Path ".\Makefile") {
    Rename-Item -Path ".\Makefile" -NewName "Makefile.win" -Force
    Write-Host "    - Renamed 'Makefile' to 'Makefile.win'." -ForegroundColor Green
}
if (Test-Path ".\Makefile.mac") {
     Write-Host "    - 'Makefile.mac' already exists."
}

# --- Step 3: Update .gitignore ---
Write-Host "[3/4] Updating .gitignore file..."
$gitignore_content = @"
# Build artifacts
*.o
*.exe
*.tmp
CipherGUI.exe
cipher_gui

# OS generated files
.DS_Store
Thumbs.db

# Editor / IDE specific
.vscode/
.idea/

# Private Vault - Should not be in version control
private_vault/
.private_vault/

# Other project specific ignores
history.md
"@
Set-Content -Path ".\.gitignore" -Value $gitignore_content
Write-Host "    - .gitignore has been updated." -ForegroundColor Green


# --- Step 4: Clean Git Index ---
Write-Host "[4/4] Cleaning Git index from unwanted files..."
git rm -r --cached . -q
git add . -A
Write-Host "    - Git index cleaned. Please review and commit the changes."
Write-Host "    - Run 'git status' to see the result." -ForegroundColor Green

Write-Host "--- Script Finished ---" -ForegroundColor Yellow
Write-Host "Your repository is now clean. Please commit the changes with:"
Write-Host 'git commit -m "Admin: Clean repository and fix project structure"'