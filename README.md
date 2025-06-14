# SystemUP!

A simple utility to show system uptime, and easily fix the main cause of your system uptime staying high on Windows systems.

## Building

### Dependencies

- MSYS2
- GCC (MinGW-w64)
- Windows SDK

To build the project, run the following command in the terminal:
(Note: Ensure you have MSYS2 installed, this will not work in cmd.exe or PowerShell, nor in WSL.)

```bash
./build.sh -b
```

## Running

After building, you can run the application like you would any other executable in Windows.
The executable will be located in the `build/bin` directory usually.

It will prompt **UAC** for permission to run, as it needs to access registry for the fast startup toggle.

## Uninstalling

To uninstall, simply delete the executable and any associated files in the `build/bin` directory. There is no formal uninstallation process as this is a simple utility without an installer.

## Features

- Displays system uptime in the system tray.
- Allows toggling of fast startup.
- Provides a context menu for quick access to power options
- Uses only Windows API for system tray integration.
- Lightweight and minimalistic design.

## License

This project is licensed under the BSD-3-Clause License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request for any changes or improvements.