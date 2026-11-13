# Doot MIDI

Sing "doots" and turn them into notes in FL Studio's piano roll.

Status: **Step 2, record mode.** Record a take, see the notes, drag them into FL's piano roll.

## Install on Windows

1. Open the [Latest build](../../releases/tag/latest-build) page and download `Doot-MIDI-Windows.zip`.
2. Unzip it. Copy the **`Doot MIDI.vst3`** folder into `C:\Program Files\Common Files\VST3` (Windows will ask for admin permission, click Continue).
3. In FL Studio: **Options > Manage plugins**, then click **Find installed plugins**. "Doot MIDI" should appear under Effects. Click its star to add it to your favorites.

## Set up in FL Studio

1. **Options > Audio settings**: choose **FL Studio ASIO** as the device, and make sure your microphone is selected as the input.
2. Open the **Mixer** (F9) and click an empty insert track. In the box at the top right of the mixer (it says "(none)"), pick your microphone input.
3. In one of that track's empty effect slots, choose **Doot MIDI**.

## Record some doots

1. Click **Record**. The plugin waits for FL.
2. Press **Play** in FL (Space) and sing along to your beat.
3. Press **Stop** in FL. Your notes appear in the plugin's preview.
4. Drag the **Drag into the piano roll** tile onto your piano roll (or the playlist). You can also click **Save .mid**.

The notes line up with the bar you started recording in. Turn off **Start/stop with FL play** to record without FL playing; the first note then starts the clip.

**Settings** (all of these update the notes instantly, even after recording):

- **Snap to grid**: quantizes note starts and ends. Off keeps your exact timing.
- **Velocity from voice**: louder doots become louder notes. Off gives every note velocity 100.
- **Sensitivity**: turn up if quiet doots are missed, down if noise or breaths become notes.
- **Timing nudge**: if every note lands a little late (common because of audio latency), move this negative.

The last take is saved with your FL project.

The zip also contains `Doot MIDI Standalone.exe`, which runs the plugin on its own outside FL Studio. Windows may warn that it's from an unknown publisher; click **More info > Run anyway**.

## How it's built

JUCE 8 C++ plugin (VST3 + Standalone). Every push to `main` builds on GitHub Actions (`.github/workflows/build.yml`) and replaces the **Latest build** release.
