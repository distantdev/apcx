# APCX

APCX is a **VST3 MIDI effect** (step sequencer) made for the **Akai APC Mini MK2**. It pairs that exact 8×8 grid with your DAW: same layout on screen and on the hardware, with **pads and LEDs driven in lockstep** so the MK2 is your real control surface—not an afterthought.

![APCX plug-in window](docs/screenshot.png)

---

## Download and install

APCX is **Windows-only** for now (Windows 10 or later). It is built for the **Akai APC Mini MK2** on USB—you need that hardware to use it as intended. You also need a DAW that supports **VST3** and can host a **MIDI effect** (or otherwise route MIDI from the plug-in; exact wording depends on your DAW).

### What you need

- **Akai APC Mini MK2** connected over USB  
- Windows 10 or later  
- A VST3-capable DAW that can host a **MIDI effect** and route its MIDI onward  

### Get the latest build

Open the repository **[Releases](releases)** page on GitHub and download the latest **MSI** or **ZIP** for Windows.

### Option A — MSI installer (recommended)

1. Download **`APCX-Setup.msi`** from the release.  
2. Run the installer and complete the steps.  
3. Restart your DAW or run its **plug-in rescan** so it picks up APCX.  

The installer places the VST3 bundle where your system expects VST3 plug-ins (you should not need to copy files by hand).

### Option B — ZIP (manual install)

1. Download **`APCX-VST3-Windows.zip`** from the release and extract it.  
2. Copy the **`APCX.vst3`** bundle (the whole folder-like bundle, not a single `.dll` inside it) into a VST3 folder your DAW scans, for example:  
   - **All users:** `C:\Program Files\Common Files\VST3`  
   - **Your account:** `%APPDATA%\VST3`  
   Your DAW may use an extra custom folder—check its plug-in preferences.  
3. Rescan plug-ins in your DAW and load **APCX**.

If the DAW does not list APCX, confirm the **full** `APCX.vst3` bundle was copied and that you chose a folder that appears in the DAW’s VST3 scan list.

---

## Using APCX

1. Connect the **APC Mini MK2** over USB before or right after you open the plug-in.  
2. Insert APCX on a track or slot where your DAW allows **MIDI-generating / MIDI-effect** VST3s, and route its MIDI output to a software instrument (or the next MIDI effect in the chain), following your DAW’s usual rules for MIDI FX.  
3. Watch the status line at the bottom: it should move from **searching** to **connected** when the APC is found.  
4. **Top four pad rows (32 pads):** steps in the sequence—use the screen, the MK2 pads, or both; they stay aligned.  
5. **Bottom left 4×4:** note / pitch selection for painting steps.  
6. **Bottom right 4×4:** velocity selection.  

**Key / scale / octave** controls are under the grid. **Pad colors on the APC** follow the same logic as the UI (current step, notes, velocity, and so on).

**Shift on the hardware:** hold **Shift** on the APC for the on-device scale/key/octave overlay.

---

## Development — environment and build

These steps match **CMake + Visual Studio 2022**, which is what this repo and **GitHub Actions** use. Building on macOS/Linux is not set up in this repository yet.

### Prerequisites

| Tool | Notes |
|------|--------|
| **Git** | For clone and submodules. |
| **CMake** | **3.22** or newer (`cmake --version`). |
| **Visual Studio 2022** | Workload **Desktop development with C++**, including the **MSVC** and **Windows** SDK toolsets. |

### Clone the repository

JUCE is included as a **git submodule**, so clone with submodules:

```bash
git clone --recurse-submodules <repository-url>
cd <repository-folder>
```

If you already cloned without submodules:

```bash
cd <repository-folder>
git submodule update --init --recursive
```

### Verify JUCE is a submodule (not a bad copy)

Your repo should record **one** gitlink for `JUCE`, not thousands of tracked files under `JUCE/`.

- After init, `git submodule status` should list `JUCE` with a commit SHA (leading `-` or `+` means out of date vs index—see Git docs).
- **Sanity check:** `git ls-files -s JUCE` must show **exactly one** line starting with **`160000`** (submodule mode). If you see many paths like `JUCE/CMakeLists.txt` with mode `100644`, JUCE was committed as normal files instead of a submodule—fix that before contributing.

On Windows you can run:

```powershell
pwsh -File scripts/verify-juce-submodule.ps1
```

### Pinned JUCE version

APCX is built and tested against **JUCE 8.0.12** (Git tag **`8.0.12`** on [juce-framework/JUCE](https://github.com/juce-framework/JUCE)). The submodule pointer in this repo should stay on that tag (or a patch commit you have explicitly validated) so CI and local builds stay reproducible.

To move the submodule to that tag after cloning:

```bash
cd JUCE
git fetch --tags
git checkout 8.0.12
cd ..
git add JUCE
git commit -m "Pin JUCE submodule to 8.0.12"
```

### Build (CMake, same as CI)

From the repository root in **PowerShell** or **cmd**:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target APCX_VST3
```

**Output:**

- Release VST3 bundle: `build\APCX_artefacts\Release\VST3\APCX.vst3`  
- With the current CMake settings, a copy may also be written under `build\VST3\` after a successful build (useful for quick testing).

To install manually for testing, copy the **`APCX.vst3`** bundle into a VST3 scan folder (see [Option B — ZIP](#option-b--zip-manual-install) above) and rescan in your DAW.

### After `git pull`

If submodule pointers change:

```bash
git pull
git submodule update --init --recursive
```

### Optional: Projucer / `.jucer` workflow

You can open **`APCX.jucer`** in Projucer, point global paths at this repo’s **`JUCE/modules`**, export a Visual Studio solution, and build from there. The **supported** path for contributions and CI is **CMake** as above.

### CI and releases (maintainers)

The workflow **`.github/workflows/build.yml`** runs on:

- **Pull requests** targeting `main`  
- **Pushes** of tags matching `v*` (e.g. `v1.0.0`)  
- **Manual** runs (**Actions** → workflow → **Run workflow**)

It produces **build artifacts**: a **VST3 ZIP** and an **MSI**. For `v*` tags it also creates a **GitHub Release** and attaches those files.

---

## Contributing

Pull requests are welcome. Please use the **CMake** build, test in a DAW with an **APC Mini MK2** when you can, and describe behavior changes clearly—especially anything that touches MIDI timing or hardware mapping.

---

## License

Copyright © 2023-2026 Distant Nebula. All rights reserved.

Contact: Levi Sluder — [levi@distantnebula.com](mailto:levi@distantnebula.com) — [https://distantnebula.com](https://distantnebula.com)
