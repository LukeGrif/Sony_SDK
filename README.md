# Sony Camera Control Centre (OpenCV GUI)

A lightweight **Control Centre** GUI built with **OpenCV** for managing Sony cameras via your C++ wrapper around the **Sony Camera Remote SDK**. It provides compact dropdowns for **Focus / Exposure / Shutter / Aperture / ISO**, buttons for **Focus**, **Capture**, **Auto Capture**, an editable **auto-capture interval**, and an **Open Latest** helper to open the newest `DSCxxxxx.JPG` from a folder.

The design uses a single **"Control Centre"** window rendered with OpenCV primitives—no external UI frameworks. It expects an existing `cli::CameraDevice` interface that exposes getters and setters for Sony camera parameters.

![alt text](image-1.png)

---

## ✨ Features

- **Camera state readback** (polls when not auto-capturing): focus mode, exposure program mode, shutter, aperture, ISO
- **Dropdown selectors** for:
  - Focus: `AF_S`, `AF_A`, `AF_C`, `DMF`, `MF`
  - Exposure Program Mode: `Auto`, `P_Auto`, `A_AperturePriority`, `S_ShutterSpeedPriority`, `M_Manual`, `Portrait`, `Sports_Action`, `Macro`, `Landscape`, `Sunset`, `Night`
  - Shutter speeds: `Bulb` through `1/4,000`
  - Aperture values: `F2.8` to `F22` (list from code)
  - ISO values: `AUTO` to `102,400` (list from code)
- **Buttons**: *Focus*, *Capture*, *Auto Capture* (toggle), *Open Latest*, *Exit*
- **Editable auto-capture interval** (ms) with clamping
- **Open Latest**: finds and opens newest `DSCxxxxx.JPG` in a folder
- **Custom background image** support via `cris_logo.png` (optional; falls back to solid color)

---

## 🧱 Architecture at a Glance

Core entrypoint (from `ViewImage.h/.cpp`):

```cpp
void ViewImage::runUI(std::shared_ptr<cli::CameraDevice> camera,
                      std::atomic<bool>& exitFlag,
                      std::atomic<bool>& autoCaptureFlag);
```

- Starts a **Control Centre** window (`1080 × 600`)
- Spawns a background thread that periodically calls `camera->capture_image()` while **Auto Capture** is enabled (sleep = interval ms)
- In the UI loop (when not auto-capturing) it polls camera for:
  - `get_live_view()` (refresh/keep-alive)
  - `get_focus_mode_output()`
  - `get_exposure_program_mode_output()`
  - `get_shutter_speed_output()`
  - `get_aperture_output()`
  - `get_iso_output()`
- Mouse handler wires dropdowns & buttons to setters:
  - `set_focus_mode_new(std::wstring, int)`
  - `set_exposure_program_mode_new(std::wstring, int)`
  - `set_shutter_speed_new(std::wstring, int)`
  - `set_aperture_new(std::wstring, int)`
  - `set_iso_new(std::wstring, int)`
  - `s1_shooting()` (Focus), `capture_image()` (Capture)

> You will need a concrete `cli::CameraDevice` adapter that wraps Sony Camera Remote SDK calls and returns human-readable strings for the current settings.

---

## ⚙️ Configuration

Update constants near the top of the file to suit your environment:

```cpp
// Focus modes
static std::vector<std::wstring> focusModes = {L"AF_S", L"AF_A", L"AF_C", L"DMF", L"MF"};

// Shutter / Aperture / ISO lists
// (See arrays in source for full options)

// Open Latest search root (change for your machine)
std::string folder = "C:\\Users\\<YOU>\\OneDrive\\Desktop\\Sony_SDK\\build\\Release";

// UI dimensions
static const int CC_WIDTH = 1080;
static const int CC_HEIGHT = 600;
```

Optional runtime asset:
- `cris_logo.png` — if present in CWD, it is resized and used as the background for the Control Centre. Otherwise a light gray background is drawn.

---

## 🖱️ UI & Controls

**Mouse**  
- **Click** a dropdown field to toggle it (Focus / Exposure / Shutter / Aperture / ISO).  
- **Scroll** while the dropdown is open to move through longer lists.  
- **Click** an item to apply setting (and it calls the corresponding `set_*_new()` on your camera).  
- **Buttons**:
  - **Focus** → `s1_shooting()`
  - **Capture** → `capture_image()`
  - **Auto Capture** → toggles a background capture loop that sleeps for `interval ms`
  - **Open Latest** → finds newest `DSCxxxxx.JPG` under `folder` and opens it via `ShellExecuteA`
  - **Exit** → sets `exitFlag = true`

**Interval editing**  
- Click the **interval box**, then type digits to edit (max length & bounds enforced).  
- **Enter** to apply, **Esc** to cancel editing.  
- Validated to `0 … 600000` ms, stored in `autoIntervalMs`.

**Keyboard (global)**  
- Press **Esc** any time (when not editing interval) to exit the app.

---

## 🔍 How “Open Latest” Works

The helper scans a folder for files whose names match `DSC*.JPG` (case-insensitive) and selects the **most recently modified**. It then launches the default viewer via `ShellExecuteA` on Windows.

```cpp
std::string latest = findLatestDSCImage(folder);
if (!latest.empty()) ShellExecuteA(NULL, "open", latest.c_str(), NULL, NULL, SW_SHOWNORMAL);
```

> Change `folder` to your actual Sony SDK output directory.

---

## 🧰 Requirements

- **Windows** (code includes `<windows.h>` and uses `ShellExecuteA`)
- **C++17** compiler (MSVC recommended)
- **CMake** 3.16+ (recommended)
- **OpenCV** 4.x (built for your compiler toolset)
- **Sony Camera Remote SDK** (headers/libs; used by your `cli::CameraDevice` wrapper)

Optional:
- An icon/background PNG named `cris_logo.png` in the working directory

---

## 🏗️ Build (CMake + MSVC)

Example out-of-source build on Windows (x64):

```powershell
# From the project root
windows:
    mkdir build
    cd build
    cmake -A "x64" ..
    cmake --build . --config Release
    cd Release
    ./RemoteCli
```



> Make sure your `cli::CameraDevice` sources are compiled into the same target or linked as a library and that they can find the Sony SDK headers and libs.

---

## ▶️ Run

```powershell
.\build\Release\RemoteCli.exe
```

- The **Control Centre** window opens.  
- If `cris_logo.png` is present, it will be used as the background.  
- Click the dropdowns to change settings.  
- Use **Auto Capture** to start/stop an automated capture loop (interval in ms).  
- **Open Latest** tries to open the newest `DSCxxxxx.JPG` from your configured folder.

---

## 🧪 Integration Notes

- The UI does **not** own camera transport; it just **calls** your `cli::CameraDevice` API.  
- To keep state fresh, it calls `get_live_view()` in the main loop while **not auto-capturing** (acts as a keep‑alive / refresh).  
- Make sure your adapter returns stable, human-readable strings for the getters so they render well in the dropdown boxes.

---

## 🩺 Troubleshooting

**Nothing updates / blank strings**  
- Ensure your `cli::CameraDevice` implementation successfully queries the Sony SDK, and that the function names in the UI (`get_*_output`) match.

**Dropdown selection does nothing**  
- Verify the corresponding `set_*_new` method is implemented and connected to the Sony SDK. Check console output for the `[... Set to: ...]` lines.

**Open Latest says “No DSCxxxxx.JPG files found.”**  
- Point `folder` to the correct output directory for your camera captures.

**Window closes when I press Esc**  
- That’s expected. Esc closes the app when not editing the interval.

**Background not visible**  
- Ensure `cris_logo.png` is in the current working directory. Otherwise the UI uses a neutral gray background.

## copyright notice and disclaimer for OSS
### libssh2

Copyright (c) 2004-2007 Sara Golemon <sarag@libssh2.org>
Copyright (c) 2005,2006 Mikhail Gusarov <dottedmag@dottedmag.net>
Copyright (c) 2006-2007 The Written Word, Inc.
Copyright (c) 2007 Eli Fant <elifantu@mail.ru>
Copyright (c) 2009-2023 Daniel Stenberg
Copyright (C) 2008, 2009 Simon Josefsson
Copyright (c) 2000 Markus Friedl
Copyright (c) 2015 Microsoft Corp.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

  Neither the name of the copyright holder nor the names of any other contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

---------------

### OpenSSL

#### Apache License
Version 2.0, January 2004  
https://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS


### OpenCV

#### Apache License
Version 2.0, January 2004
http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

