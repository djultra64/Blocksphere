# Read-only inventory. Run from PowerShell on the Windows test PC.
# Does not collect host/user names, product keys, disk paths or serial numbers.
$ErrorActionPreference = 'Stop'
$os = Get-CimInstance Win32_OperatingSystem
$system = Get-CimInstance Win32_ComputerSystem
$result = [ordered]@{
    schema_version = 1
    recorded_at_utc = (Get-Date).ToUniversalTime().ToString('o')
    os = @{ caption = $os.Caption; version = $os.Version; build = $os.BuildNumber; architecture = $os.OSArchitecture }
    hardware = @{ manufacturer = $system.Manufacturer; model = $system.Model; physical_ram_bytes = $system.TotalPhysicalMemory }
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors)
    gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, DriverDate, CurrentHorizontalResolution, CurrentVerticalResolution, CurrentRefreshRate)
    installed_ram_bytes = [uint64]((Get-CimInstance Win32_PhysicalMemory | Measure-Object Capacity -Sum).Sum)
    monitors = @(Get-CimInstance -Namespace root/wmi WmiMonitorID | ForEach-Object {
        @{active=$_.Active; model=(-join ($_.UserFriendlyName | Where-Object {$_ -gt 0} | ForEach-Object {[char]$_})); manufacturer=(-join ($_.ManufacturerName | Where-Object {$_ -gt 0} | ForEach-Object {[char]$_}))}
    })
    game_controllers = @(Get-CimInstance Win32_PnPEntity | Where-Object {$_.Name -match 'Gamepad|Game Controller|Xbox|DualSense|DualShock'} | Select-Object Name, Status)
    power_profile = (powercfg /getactivescheme | Out-String).Trim()
    notes = 'Display values can be null or stale; verify active resolution and refresh in Settings. PnP records do not establish physical controller count or P1/P2 independence.'
}
$result | ConvertTo-Json -Depth 6
