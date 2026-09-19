# Target Replay

Play an existing Geometry Dash macro up to a chosen percentage.

Open a Classic level, pause, then choose **Target Replay** in the upper right.
Choose a `.gdr`, `.gdr.json`, or `.gdr2` file, set **Target %**, and press **Start**.
Start restarts the level. **Stop** releases inputs and leaves the game paused.

Supports Normal mode and 240 TPS input replays. At 1–99%, the mod ends the
attempt and pauses when the game's progress crosses the target. At 100%, it
waits for actual level completion. Existing higher best records are not lowered.

Only input events are replayed; bot-specific position/velocity corrections are
not applied. Matching level ID does not guarantee matching level physics or
version. Platformer, Practice, and Start Positions are not supported.

This is a BOT/TAS tool. Game statistics may change, and leaderboard submission
is not blocked. Keep automated runs separate from legitimate records.

Built for Geode 5.8.2 / Geometry Dash 2.2081. The macOS package and parser tests
have been built and checked; live in-game playback has not yet been tested.
