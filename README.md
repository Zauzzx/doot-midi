# Doot MIDI

Sing "doots" and turn them into notes in FL Studio's piano roll.

Status: **Step 1 test build.** The plugin loads in FL Studio and shows your mic level. Note detection comes next.

## Install on Windows

1. Open the [Latest build](../../releases/tag/latest-build) page and download `Doot-MIDI-Windows.zip`.
2. Unzip it. Copy the **`Doot MIDI.vst3`** folder into `C:\Program Files\Common Files\VST3` (Windows will ask for admin permission, click Continue).
3. In FL Studio: **Options > Manage plugins**, then click **Find installed plugins**. "Doot MIDI" should appear under Effects. Click its star to add it to your favorites.

## Test it

1. **Options > Audio settings**: choose **FL Studio ASIO** as the device, and make sure your microphone is selected as the input.
2. Open the **Mixer** (F9) and click an empty insert track. In the box at the top right of the mixer (it says "(none)"), pick your microphone input.
3. In one of that track's empty effect slots, choose **Doot MIDI**.
4. Sing a doot. The meter should move and the plugin should say **Hearing you!**

The zip also contains `Doot MIDI Standalone.exe`, which runs the plugin on its own outside FL Studio. Windows may warn that it's from an unknown publisher; click **More info > Run anyway**.

## How it's built

JUCE 8 C++ plugin (VST3 + Standalone). Every push to `main` builds on GitHub Actions (`.github/workflows/build.yml`) and replaces the **Latest build** release.
