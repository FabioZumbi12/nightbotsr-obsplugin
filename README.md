[Read this in Portuguese (Português)](README.pt-BR.md)

# Nightbot Plugin for OBS Studio

## Introduction

This is a plugin for OBS Studio that integrates Nightbot's *Song Request* system directly into the OBS interface. It adds a new dockable panel (Dock) that allows you to view and manage the queue of songs requested by your Twitch viewers, without needing to switch to a web browser.

## Features

*   **Queue Visualization:** See the list of songs currently in the Nightbot queue.
*   **Playback Control:** Control the current song (play, pause, skip).
*   **Queue Management:** Promote songs to the top of the list or remove them.
*   **Full Integration:** Everything is done within a dockable panel in OBS Studio.

## Installation

1.  Download the installer (`.exe`) from the Releases page.
2.  Run the installer. It will try to automatically locate your OBS Studio installation folder.
3.  After installation, start OBS Studio.

## How to Use

1.  With OBS Studio open, go to the **Docks** menu.
2.  Click on the **"Nightbot"** option to activate the plugin's dock.
3.  A new panel will appear. You can position it wherever you prefer in the OBS interface.
4.  Log in with your Nightbot account to load and manage your song queue.

## Building from Source

If you want to build the plugin manually, follow the steps below.

### Requirements
*   Git
*   CMake (version 3.28 or higher)
*   Visual Studio 2022 (with the "Desktop development with C++" workload)
*   OBS Studio dependencies

### Steps
1.  Clone the repository:
    `git clone --recursive https://github.com/FabioZumbi12/Nightbot-ObsPlugin.git`
2.  Create a build directory: `mkdir build && cd build`
3.  Run CMake to generate the project files: `cmake ..`
4.  Compile the project using Visual Studio or directly from the command line: `cmake --build . --config RelWithDebInfo`

## Contributions

Contributions are welcome! Feel free to open an *issue* to report problems or suggest new features, or submit a *pull request* with improvements.

