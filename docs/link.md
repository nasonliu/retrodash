# LINK: two players on two FM-1s

Two players each have their own FM-1 and play the same song at the same time. Everything in the game is built for this
from the start: the note chart is deterministic, and each unit judges its own presses against its own clock. So only the
start, the clock and the score have to go over the cable.

## The layers: from browser to USB-C

| Layer | Connection | Status |
|---|---|---|
| **L0** | The protocol: MIDI channel messages on channel 16, plus clock, start and stop (below) | Version 1, in the reference |
| **L1** | Two windows in the same browser (BroadcastChannel), and a bot that speaks the same protocol | Done in reference 0.1 |
| **L2** | Two FM-1s via a USB host: a computer or a tablet with a relay page (WebMIDI), or a USB-MIDI host box | Possible with Dinghy as it is. Needs the firmware port. |
| **L3** | Two FM-1s with one USB-C cable directly between them | Requires USB host mode in the firmware. To be settled in a feasibility study. |

The messages are the same on all layers. What works in L1 works in L2 and L3 without changes.

LINK is off in MIDI MODE ([D5](DECISIONS.md)). There the MIDI clock and START/STOP belong to the DAW.

### Why not just one USB-C cable yet

USB always has one host and one device. The FM-1 is a USB device (class compliant USB-MIDI), and Dinghy's `usb.c` runs
in device mode. If two FM-1s are connected with a C-to-C cable, both wait for a host, and nothing happens. The SLOOP
guide says the same about a USB keyboard straight into the FM-1. The TRS jack is MIDI in only, so it cannot connect two
FM-1s on its own either.

The feasibility study for L3 must answer this:

1. **Host mode.** Can the USB controller in the JieLi AC791N run as a host (OTG)? I have not found the datasheet for
   the AC791N specifically, so this is not settled.
2. **Power.** A host must supply 5 V on VBUS. Can the FM-1 do that from the battery, or is an OTG adapter with its own
   power input needed?
3. **Host driver.** The firmware needs a small USB-MIDI host driver (enumeration and bulk endpoints). It is a
   substantial job, but a bounded one: it only needs to know one device, another FM-1.
4. **Roles.** Who is the host? The most natural is that P1 is the host and P2 is the device, chosen in the menu before
   the match.

If the feasibility study answers no, L2 becomes the standard connection. It works regardless. Some FM-1 versions also
have Bluetooth MIDI, which may become a later layer (L4), but Dinghy has no Bluetooth.

## Protocol, version 1

A Dinghy app receives only channel messages, and of the system messages only clock, start, continue and stop (`0xF8`,
`0xFA`, `0xFB`, `0xFC`). SysEx belongs to the platform's updater, and Song Position Pointer (`0xF2`) is discarded. So
the protocol uses control change messages (CC) on channel 16, status `0xBF`, and the position is START plus the number
of clock pulses.

| Message | Bytes | From | Meaning |
|---|---|---|---|
| HELLO | `BF 64 v` (CC 100) | Both, every second | Protocol version, currently 1 |
| ID | `BF 65 id` (CC 101) | Both, every second | Random 1–126. The lowest becomes P1 (the bot is 127). On a tie, the receiver draws again. |
| SONG | `BF 66 n` (CC 102) | P1 | The song's place in P1's list, for display only |
| SONG-ID | `BF 69 hi` `BF 6A lo` (CC 105–106) | P1 | The song's identity: a 14-bit hash of the play track and the tempo. P2 finds the song from it, not from the place in the list. |
| SPEED | `BF 68 %` (CC 104) | P1 | Tempo in percent, 50–100 |
| START | `FA` | P1 | Both start the song now. P2 uses the last SONG-ID and SPEED; the level is the song's own ([D12](DECISIONS.md)), so CC 103 (LEVEL in earlier versions) is no longer sent, and is ignored. If P2 does not have the song, `SONG NOT HERE` is shown and P2 does not start. |
| CLOCK | `F8` | P1 | 24 pulses per beat, from START |
| STOP | `FC` | P1 | The match is aborted, both go to SONGS |
| SCORE | `BF 6E hi` `BF 6F mid` `BF 70 lo` (CC 110–112) | Both | Score in three 7-bit parts, up to 2,097,151. Applied when `lo` arrives. |
| COMBO | `BF 71 hi` `BF 72 lo` (CC 113–114) | Both | Combo. Applied when `lo` arrives. |
| JUDGE | `BF 73 j` (CC 115) | Both | 1 PERFECT, 2 GREAT, 3 GOOD, 4 MISS |
| BOOST | `BF 74 0/1` (CC 116) | Both | OP BOOST off or on |
| OPS | `BF 75 n` (CC 117) | Both | Operators lit, 0–6 |
| DONE | `BF 76 %` (CC 118) | Both | Finished, with hit percentage |
| BYE | `BF 77 0` (CC 119) | Both | Disconnects |

Notes on channel 16 (`9F` and `8F`) are reserved for later, when each player can hear the other's voice in band mode.

**Rules**

- SCORE and COMBO are sent only when they change, and at most 15 times a second.
- P2 starts on START and counts the clock. In the firmware, P2 is to adjust the tempo slightly when its own position
  differs from the pulse count by more than 10 ms. For now the reference only counts the pulses. Each player hears
  their own unit, so only the difference at the start is noticeable.
- Each unit judges its own presses locally. Latency in the cable never affects PERFECT or GREAT, only how quickly the
  other player's score is shown.
- A line that has been silent for 3.5 seconds is treated as disconnected.

## Modes

| Mode | What | Status |
|---|---|---|
| **VS** | The same song at the same time. The opponent's score is in the top bar, and the winner is shown on the results screen. | Done in reference 0.1 (two windows and bot) |
| **Alternate** | The players take a phrase each. The longest combo wins the round. | Planned |
| **Band** | Co-op: P1 plays the melody, P2 bass or chords from its own note chart. The notes on channel 16 let both hear the whole. | Planned |
| **Battle** | OP BOOST sends a handicap to the opponent, for example that the notes only become visible near the hit line for two bars. | Idea |

A simpler variant with two players on one FM-1, each with their own end of the keyboard as in RoboSeq's battle, may
come as well. The main focus is two FM-1s.
