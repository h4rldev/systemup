if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator"))
{
    # Get the current script path and directory
    $scriptPath = $MyInvocation.MyCommand.Path
    $currentDir = Split-Path -Parent $scriptPath

    # Restart as administrator in the same directory
    Start-Process powershell -Verb RunAs -ArgumentList "-NoExit", "-Command", "Set-Location '$currentDir'; & '$scriptPath'"
    exit
}

if (-not (Test-Path ".\systemup.exe")) {
    Write-Host "systemup.exe not found in the current directory. Please ensure it exists."
    Write-Host "Pwd: $PWD"
    Start-Sleep -Seconds 5
    exit 1
}

if (-not (Test-Path "$env:USERPROFILE\systemup")) {
    Write-Host "Creating directory for SystemUP!"
    mkdir $env:USERPROFILE\systemup
}

if (-not (Test-Path "$env:USERPROFILE\systemup\systemup.exe")) {
    Move-Item .\systemup.exe $env:USERPROFILE\systemup\systemup.exe
}

if (-not (Test-Path "$env:USERPROFILE\systemup\systemup.exe")) {
    Write-Host "Failed to move systemup.exe to the user directory."
    Start-Sleep -Seconds 5
    exit 1
}

# Create a scheduled task to run the script at startup
$action = New-ScheduledTaskAction -Execute "$env:USERPROFILE\systemup\systemup.exe"
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Highest

if (-not (Get-ScheduledTask -TaskName "Run SystemUP!" -ErrorAction SilentlyContinue)) {
    Write-Host "Creating scheduled task to run SystemUP! at startup."
} else {
    Write-Host "Scheduled task 'Run SystemUP!' already exists. Updating it."
    Unregister-ScheduledTask -TaskName "Run SystemUP!" -Confirm:$false
}
Register-ScheduledTask -Action $action -Trigger $trigger -TaskName "Run SystemUP!" -Description "Runs SystemUP! at startup" -Principal $principal -Settings $settings

# Get the Start Menu Programs folder path
$startMenuPath = [Environment]::GetFolderPath('Programs')

# Create a shortcut
$WshShell = New-Object -comObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("$startMenuPath\SystemUP!.lnk")
$Shortcut.TargetPath = "C:\Users\$env:USERNAME\systemup\systemup.exe"
$Shortcut.WorkingDirectory = "C:\Users\$env:USERNAME\systemup"
$Shortcut.Description = "SystemUP! - See your uptime in your tray bar!"
$Shortcut.IconLocation = "$env:USERPROFILE\systemup\systemup.exe,0"
$Shortcut.Save()

Write-Host "SystemUP! has been installed successfully."
Write-Host "You can run it from the Start menu or by searching for 'SystemUP!', or wait for the next system startup."