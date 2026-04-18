# Copilot Instructions for Spider-Bot

When generating code for this repository, keep the architecture explicit and do not bypass it.

## Core rules

- ESP32 handles locomotion only.
- Raspberry Pi handles audio-visual features only.
- Do not move audio/video control into ESP32 code.
- Do not drive servos from Raspberry Pi code in the current version.
- The global FSM supervises the system mode and does not perform IK math.
- `leg_kinematics` is the module responsible for inverse kinematics.
- Keep the hierarchical FSM structure: global FSM, locomotion FSM, leg FSM.
- Preserve the tripod gait unless the user explicitly asks for a different gait.
- Keep safety mandatory: centering, safe stop, error handling, signal loss handling, servo fault handling, and impossible IK target handling.
- Keep responsibilities separated: input reading, state management, locomotion, IK, and servo command output.
- Favor embedded-friendly code: readable, deterministic, explicit, and with minimal unnecessary abstraction.

## Architecture intent

- The robot is an hexapod with 6 legs, 18 servomotors, and 3 DOF per leg.
- Locomotion uses inverse kinematics as the mathematical core.
- Servo commands are the final step after IK and angle conversion.
- The code should reinforce the split between motion on ESP32 and audio-visual features on Raspberry Pi.