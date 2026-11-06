# Experimental macOS arm64 backend

Local port of BioRhythmicStudios `port` b71d137, with POSIX file/settings/test
implementations imported from `linux-port` 8e49d1f. The original project and its
credits/license notices remain authoritative. This is an experimental community
backend, not an upstream release or a full-game compatibility certification.

## Build on this Mac

Run `port/macos/build.sh` from the checkout. Clang, CMake, Ninja and SDL3 are
already installed. The script keeps its build, module cache and temporary files
on the external SSD and packages `../Wario Land 4.app` with SDL3. The ROM is not
included in the app. Its launcher expects `../.work/rom/baserom_us.gba` and uses
`../saves/wl4.sav`, `../settings.cfg`, and `../wl4-last-run.log`.

The app has an ad-hoc local signature, not notarization. Packaging preserves
AppleDouble metadata outside the app under `build/macos-arm64/package-metadata`
because the signer otherwise mistakes `._*` files for nested code. Existing
SDL3 is copied by data only and its license is included in Resources.

## Memory and native execution

Darwin cannot allocate the original low GBA addresses in a normal arm64 process.
RAM/ROM symbols and the coroutine stack use a high bank at `0x8000000000` plus
their GBA offset. `mach_vm_allocate` reserves each range without replacing any
existing map. `GP` converts stored 32-bit addresses to host aliases; `GP_SET`
converts back. Stack addresses retain the same low 32 bits for DMA registers.

Build-only derivatives under `build/macos-arm64/darwin-generated` change the
Mach-O symbol names, section names, inline-asm aliases and selected platform
pointer uses. Shared `src/`, `include/` and `port/generated/` remain unchanged.
`prepare.py` writes only changed derivatives, and CMake tracks their inputs.
The original GBA compiler and data extraction path are not required for this build.

Raw low pointers and I/O accesses raise SIGSEGV/SIGBUS. The Darwin handler decodes
and completes the faulting native arm64 load/store using the backing storage.
I/O still executes the existing before/after hardware callbacks, including DMA,
interrupt flags and audio writes. There is no GBA instruction interpreter:
compiled C game functions and native ARM64 arithmetic/control flow execute directly.
Supported accesses include scalar, sign-extending, pair, SIMD, single-lane SIMD,
acquire/release, register-offset, pre-index and post-index forms. Unknown
instructions stop with a diagnostic instead of silently inventing a result.

Actual accessible subregions are recorded separately from reservations so that
IWRAM/EWRAM mirrors reach the existing redirect callback rather than recursively
faulting on an inaccessible alias. Host page protection uses the 16 KiB page size.
Standard memory/file output wrappers translate valid RAM/ROM addresses before
calling libc or a kernel-backed file operation. I/O bypass is deliberately excluded.

## Verified locally, 27 October 2026

- Complete arm64 build, including layout assertions and checks.
- Coroutine stack: four yields and 2 MiB recursion.
- BIOS/open bus/mirrors and all 50 LZ77 streams, 630048 decompressed bytes.
- I/O, immediate/timed/stack DMA, IRQ regressions and sound-register tests.
- Additional real arm64 pair, indexed, signed and SIMD access tests.
- Settings/keyboard/virtual-gamepad checks; no physical gamepad attached.
- Disposable save roundtrip, recovery, truncated/oversized input, partial write and
  obstructed rename. exFAT ignores chmod; permission-denial cases alone use explicit
  in-process failure injection enabled by `--test-save`. Missing paths, file-size
  limit failures and rename failures remain real filesystem tests.
- 300 intro frames and 1400-frame file-select route.
- 8500-frame Hall of Hieroglyphs route, exit 0, screenshot and nonzero audio captured.
- New process loads a copy of the Hall save and shows `Hall of Hieroglyphs` in slot A.
  Original and reloaded copies retain identical SHA-256 hashes.
- Local `.app` signature passes `codesign --verify --deep --strict`.
- Actual app launched through LaunchServices: Metal renderer, VSync reported on,
  title/menu visible, Enter advances to file select, F11 enters fullscreen.
- SDL audio initializes without a `No sound` error. Audible quality still requires
  the user to listen; a nonzero WAV is not proof of speaker output quality.

Artifacts and logs: `build/macos-audit/`. The GUI was left open for the user.

## Limits

Full-game traversal, every compiler-emitted memory instruction, physical controller,
ASan and frame/memory equivalence against mGBA are not verified. Performance is a
local bounded measurement, not a claim about every stage or machine. This Mac runs
macOS 27.0.1; other macOS versions were not tested. `--fault` intentionally stops
its process. No existing saves should be used for development tests; always specify
a new `--save` path. The tests preserve their evidence under `build/`.

The original GBA byte-identical rebuild was not run locally: its reference object
set and compiler are absent. `git diff --exit-code -- src include port/generated`
passes, and `tools/agbcc` was not touched. No commit, push or release was performed.

## Bluetooth follow-up

See `docs/MACOS_INPUT_FIX.md`. The same local app now recovers stale disconnected
SDL handles by polling as well as events, and logs input/focus/player state.
A dropped-event virtual-controller room-transition test passes after the fix.
Physical Machenike G5 Pro reproduction remains pending. User confirms light
audio crackles, so audible quality is not considered verified; audio counters
are now logged for diagnosis. Player saves are preserved.

## Audio experiment rolled back

The player rejected the separate 100 ms audio trial for latency. The normal
Wario Land 4.app remains the exact responsive controller-fixed binary. Audio
experiments are OFF by default and build.sh forces OFF. A short-ramp, old-buffer
policy has only synthetic validation and is not deployed. See
`docs/MACOS_AUDIO_INVESTIGATION.md`; occasional crackles remain unresolved.
