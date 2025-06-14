if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Output "Script is NOT running as administrator."
    Write-Output "Please run this script as an administrator."
    exit 1
}


if (-not (Test-Path ".\systemup.exe")) {
    Write-Host "systemup.exe not found in the current directory. Please ensure it exists."
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
    exit 1
}

# Create a scheduled task to run the script at startup
$action = New-ScheduledTaskAction -Execute "$env:USERPROFILE\systemup\systemup.exe"
$trigger = New-ScheduledTaskTrigger -AtStartup
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Highest

if (-not (Get-ScheduledTask -TaskName "Run SystemUP!" -ErrorAction SilentlyContinue)) {
    Write-Host "Creating scheduled task to run SystemUP! at startup."
} else {
    Write-Host "Scheduled task 'Run SystemUP!' already exists. Updating it."
    Unregister-ScheduledTask -TaskName "Run SystemUP!" -Confirm:$false
}
Register-ScheduledTask -Action $action -Trigger $trigger -TaskName "Run SystemUP!" -Description "Runs SystemUP! at startup" -Principal $principal -Settings $settings