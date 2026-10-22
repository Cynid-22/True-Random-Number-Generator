# Stochastic Models of the Entropy Sources

For each of TRNG's five entropy sources, this document describes:
- what exactly is unpredictable;
- what the collector reads;
- how a sample is built from predictable and unpredictable parts;
- why its credit cap is safe;
- what an attacker could observe or influence;
- how failures are caught.

BSI AIS 31 asks for a model like this for every noise source. These are written models backed by measurement, not a certification.

The measured numbers come from [Measured Source Entropy](../README.md#measured-source-entropy-nist-sp-800-90b) in the README: NIST SP 800-90B `ea_non_iid` and `ea_restart`, run on the raw values on 2026-10-21, on one Windows 11 laptop, on bare metal.

---

## Scope and assumptions

**The attacker may:**
- know the hardware, the Windows version and TRNG's code;
- watch or listen to the user;
- play sounds near the microphone;
- run programs on the same computer that have no access to TRNG's process (a web page, a sandboxed app, a program under another Windows account), and so load the CPU or time their own threads.

**Out of scope:**
- code running as the same Windows user or as administrator, which can read TRNG's memory directly;
- a compromised Windows or hypervisor;
- tampering with the hardware.

**Clock:** every value is stamped with a steady clock that only moves forward and is not changed by time sync, usually in 100 ns steps. Timestamps are hashed into the seed but never credited.

## How credit works for every source

1. **Per-sample estimate.** Each sample is credited `log2` of the smallest of a chosen set of values, capped at the source's maximum:
   - the sample itself;
   - its change from the previous sample;
   - the change of that change.

   The Microphone instead uses a bit-balance rule, described in its section. The estimate is a heuristic, not a min-entropy measurement. The cap carries the guarantee.
2. **Repeats earn nothing.** A value equal to one of the source's last 8 values gets no credit.
3. **Caps sit below the measurements.** Each cap is below the min-entropy measured for that source, with the margins listed below. No cap is ever raised above its measurement.
4. **Health tests.** Every value runs through the SP 800-90B repetition count test and adaptive proportion test, with a false alarm rate of 2^-20 and a 512-sample window. The cutoffs are computed from the cap. A failing source earns no more credit and shows **Failed**.

   | Cap per sample | Repetition count cutoff | Adaptive proportion cutoff (of 512) | Sources |
   |---|---|---|---|
   | 8 bits | 4 | 13 | Microphone (per 64-LSB block) |
   | 2 bits | 11 | 177 | Keystroke |
   | 1 bit | 21 | 311 | Mouse, Clock Drift, CPU Jitter |

5. **Start-up test.** Each time collection starts, a source's first 1,024 samples must pass both health tests before any of their credit counts.
6. **Credit adds up across sources.** Credits from different sources are added as if the sources were independent. Where they are not, see [Dependencies between sources](#dependencies-between-sources).

All values and their timestamps are condensed with SHA-512 into the seed of the generation pipeline. The credit counts the min-entropy that went in. How the pipeline uses it is described in the README's [Security Considerations](../README.md#security-considerations).

### Where the credit comes from

| Source | Cap | Highest rate |
|---|---|---|
| Microphone | 8 bits per 64 samples | Up to 6,000 bits/s at 48 kHz |
| Clock Drift | 1 bit per sample | Up to about 1,000 bits/s (one sample per 1 ms wait) |
| CPU Jitter | 1 bit per sample | Up to about 1,000 bits/s |
| Keystroke | 2 bits per timing (two timings per key) | A few bits per second while typing |
| Mouse | 1 bit per sample (at most one per 7 ms) | Up to about 140 bits/s while moving |

The microphone and the two timing sources supply most of the credit. That is why the virtual machine rule and the microphone's raw stream matter.

---

## Microphone

**What is unpredictable.** Every microphone input carries electronic noise:
- thermal noise in the resistors and the amplifier;
- shot noise in the transistors;
- the converter's own noise.

This noise is random at the physical level. When its size is at least about one step of the analog-to-digital converter, the lowest bit of each sample comes out close to a fair coin flip, whatever sound is being recorded.

**What is read:**
- The least significant bit (LSB) of each sample of the first channel, at the device's own bit depth (up to 24 bits).
- Windows' raw stream, which skips sound enhancements, when the driver supports it.
- 64 consecutive LSBs are packed into one value.
- The first 500 ms after each start are discarded, and buffers that Windows flags as silent are skipped.

**Model.**
- A sample is the sound plus the noise, rounded to converter steps.
- In the worst case the attacker knows the sound, or even produces it. The noise is what they cannot know.
- For Gaussian noise with standard deviation σ and converter step q, the LSB's bias falls off as exp(-π²σ²/2q²). It is about 0.5% at σ = q and about 2 × 10^-9 at σ = 2q.
- The microphone, its amplifier and the converter's filter shape the noise, so neighbouring samples are correlated. When the noise spans several steps, the LSB keeps little of that correlation.
- Measured on a built-in 16-bit microphone in a quiet room: 0.875 bits per LSB (non-IID), and the restart test passed with 0.844.
- Before the raw stream was used, Windows' noise suppression brought this down to 0.0446. Processing can remove the noise, which is why the raw stream is requested and every block is checked.

**Credit.**
- At most 8 bits per 64 LSBs, which is 0.125 bits per LSB: 6.8 times below the measurement.
- A block's credit is 64 times the smaller of two per-bit min-entropies: one from how balanced its ones and zeros are, one from how balanced its bit changes are. A block of all zeros, all ones or strictly alternating bits earns nothing.

**What an attacker can observe or influence:**
- **Playing a known sound, including ultrasound,** shifts the signal but does not take away the noise. To set the LSB, an attacker would have to predict the noise itself.
- **A sound loud enough to clip** pins the samples at full scale, so the LSB stops changing. Those blocks earn no credit, and 4 identical blocks in a row fail the repetition count test.
- **A program allowed to use the microphone** can record the same input, and with it the same or similar LSBs. This is the main exposure of this source. Windows' privacy settings show which apps use the microphone.

**Failures and how they are caught:**

| Failure | How it is caught |
|---|---|
| Hardware mute that sends exact zeros | No credit; the repetition count test fails after 4 blocks |
| Mute that Windows flags as silence | No data at all |
| Disconnected device | The collector stops, and the source shows Unavailable |
| A driver noise gate or other processing | Long runs of equal bits, which lower the credit and trip the health tests |
| Settling after a start (the restart test showed about 175 ms) | The 500 ms discard |

---

## Keystroke

**What is unpredictable.** It comes in two parts:
- **A coarse human part.** Nobody types with the same timing to the millisecond. Gaps between keys vary by tens of milliseconds even in practised words, and hold times by several milliseconds.
- **A fine system part.** Each key takes a varying path from the keyboard to TRNG:
  - the keyboard's own scan or USB report interval, often 1 to 8 ms;
  - the interrupt and the Windows input stack;
  - the wake-up of TRNG's input thread.

  Each step adds a delay that changes by microseconds from key to key.

**What is read:**
- The steady-clock time, in nanoseconds, at which TRNG's Raw Input thread receives each key.
- **Flight time** is a key-down minus the previous key-up. **Dwell time** is a key-up minus the latest key-down.
- Ignored:
  - auto-repeat from a held key;
  - keys without a physical device handle (sent by software);
  - keys while TRNG's window is not in front;
  - keyboard overrun codes.

**Model.**
- A timing is the human interval, rounded to the keyboard's report steps, plus the delivery delay.
- The measured symbol is the lowest 8 bits of the timing in nanoseconds: its position within 256 ns. At 100 ns clock steps, that allows 64 different values (6 bits at most).
- That position is set almost entirely by the fine system part, not by the person.
- The measured 3.278 bits per timing therefore comes from the delivery delay alone. The human part adds more on top, for an attacker who cannot observe the typing.

**Credit.**
- At most 2 bits per timing. Timings in nanoseconds are large, so almost every timing earns the full 2 bits unless it repeats one of the last 8 values.
- That is 1.6 times below the 3.278 bits measured on the lowest 8 bits alone.
- The recording held 3,883 timings, far below the 1,000,000 samples the standard expects, so the estimate is less certain than for the other sources. The restart test does not apply to human input.

**What an attacker can observe or influence:**
- **Watching or recording the typing,** for example its sound, reveals the coarse timings to about a millisecond. It does not reveal the sub-microsecond delivery delay the credit rests on.
- **Keys sent by software** are rejected. A virtual keyboard driver would have a device handle and is not rejected, but installing one needs administrator rights, which is out of scope.
- **Holding a key** earns nothing, since auto-repeat is ignored.
- **Loading the CPU from another program** changes the spread of the delivery delay. No known way lets an unprivileged program make that delay constant to within 100 ns.

**Failures and how they are caught:**
- A hardware device that types at fixed intervals takes away the human part but not the delivery delay.
- Exact repeats earn no credit and trip the health tests.

---

## Mouse

**What is unpredictable.**
- **The hand.** Involuntary micro-movements of the hand (tremor and motor noise) sit on top of the intended motion.
- **The sensor.** It counts at hundreds to thousands of counts per inch, so it resolves movements of tens of micrometres, and its own tracking of the surface adds noise.

**What is read:**
- The sensor's raw relative counts (dx, dy) through Raw Input, before pointer speed and acceleration.
- Counts are summed into one sample at most every 7 ms, only while the pointer is over the Mouse Path box and TRNG is in front.
- Movement without a physical device handle is rejected, unless the touchpad itself has reported within the last 50 ms. A precision touchpad's movement has no device handle, just like movement sent by software.
- The value holds the running position (16 bits per axis, wrapping) and the sample's dx and dy.

**Model.**
- Movement per sample is the intended motion, which is smooth and partly predictable, plus the involuntary part.
- Each sample covers at least 7 ms but ends when the next report arrives, so the length of the window varies too.
- Measured, as 4 low bits of dx plus 4 of dy:
  - 1.750 bits per sample on a 1,000 Hz mouse (8,902 samples);
  - 1.657 bits per sample on a precision touchpad (14,291 samples).

**Credit.**
- At most 1 bit per sample: 1.66 to 1.75 times below the measurements.
- A sample is credited `log2` of the smallest of three values: its movement, the change in movement and the change of that change. The position itself is not credited.
- A straight line at constant speed has no change, so it earns nothing. Only irregular motion counts.
- The recordings are far below 1,000,000 samples, so the margin is kept for the same reason as for the keyboard.

**What an attacker can observe or influence:**
- **Video of the hand** cannot resolve movement at the level of single counts.
- **Software movement** is rejected. A programmable USB device that poses as a mouse would be accepted, but that needs physical access, which is out of scope.
- **Software movement during a touch:** while a finger is on a precision touchpad, software movement cannot be told apart from touchpad movement.
  - Added movement does not remove the hand's noise from the sum.
  - It could, however, produce credited samples while the finger rests still.
  - Sending input to TRNG needs a program running on the same desktop at the same or higher integrity level.

**Failures and how they are caught:**
- A mouse that is not moved produces no samples.
- Firmware smoothing or "angle snapping" makes the motion more regular, which lowers the credit through the change-of-change rule.
- Exact repeats earn no credit and trip the health tests.

---

## Clock Drift

**What is unpredictable.** It is the delay between the moment a timer is due and the moment the waiting thread actually runs again. That delay comes from:
- the timing of the timer interrupt;
- other interrupts and deferred work on that core;
- the scheduler;
- how long the core takes to leave a power-saving state.

Each depends on everything else the computer is doing at that microsecond.

**What is read:**
- The CPU's time-stamp counter, read before and after a 1 ms wait on a high-resolution Windows timer. The lowest 16 bits of the difference are kept.
- Differences of 0 or over 10^9 ticks are skipped.
- Without the high-resolution timer (Windows 10 before version 1803), TRNG falls back to the normal sleep and logs a warning.

**Model.**
- The difference is the counter rate times (1 ms + L), where L is the wake-up delay.
- Most x86 processors since about 2008 have an invariant time-stamp counter: it ticks at a fixed rate whatever the core's clock speed, temperature or voltage. All of the variation is therefore in L.
- The name comes from comparing two clocks, Windows' timer and the CPU's counter; there is no drift between oscillators involved.
- At about 3 GHz, one tick is a third of a nanosecond, while L varies by microseconds, thousands of ticks. Even so, the lowest bits are not equally random on every CPU, which is why they are measured rather than assumed.
- Measured: 4.733 bits per lowest 8 bits. The restart test passed with 4.733.

**Credit.**
- At most 1 bit per sample, 4.7 times below the measurement.
- A sample earns its bit when both the change and the change of that change, in the lowest 16 bits, are at least 2 ticks.

**What an attacker can observe or influence:**
- **Loading the CPU or changing its power state** from another program changes how L is spread.
- **Timing its own wake-ups:** a program on the same computer can do this. They partly share the same interrupts, so it may learn part of L, but it cannot read the low bits of TRNG's own count. The 4.7 times margin is meant to cover such partial knowledge.
- **In a virtual machine** the host controls both the time-stamp counter and the delivery of timer interrupts. Clock Drift therefore earns no credit when TRNG detects a hypervisor (see the README).

**Failures and how they are caught:**

| Failure | How it is caught |
|---|---|
| No high-resolution timer | A coarser wait, with a warning in the log |
| Virtual machine | No credit |
| A counter that stops changing in its low bits | The repetition count test |

---

## CPU Jitter

**What is unpredictable.** It comes in two parts:
- **The sampling thread's wake-up delay,** the same kind of delay as Clock Drift's.
- **How fast the counting thread runs in between.** That speed varies with:
  - interrupts and other threads taking its core;
  - clock speed changes from boost, power limits and thermal throttling (here temperature does matter, through the clock speed);
  - a second thread sharing the same physical core;
  - the memory holding the counter moving to the sampling thread's core each time it is read.

**What is read.**
- One thread adds 1 to a shared counter in a tight loop.
- A second thread waits 1 ms on a high-resolution timer and records how far the counter moved since its last look.
- Neither thread pauses the other.

**Model.**
- The count difference is the counting speed, integrated over (1 ms + L), minus whatever time the counting thread lost to interrupts and other threads.
- Both factors vary from sample to sample.
- Measured: 7.188 bits per lowest 8 bits. The restart test passed with 7.188.

**Credit.**
- At most 1 bit per sample, 7.2 times below the measurement.
- A sample is credited `log2` of the smallest of three values: the count difference, its change and the change of that change.

**What an attacker can observe or influence:**
- **Competing for the core** changes the counting speed. That adds variation; making the speed exactly constant would need control over every interrupt on that core.
- **In a virtual machine** the host controls scheduling and timer delivery, so CPU Jitter earns no credit there either.

**Failures and how they are caught:**
- If the counting thread gets no CPU time at all, the difference is 0. It earns no credit, and 21 zeros in a row fail the repetition count test.
- The counting thread keeps one core busy while collecting. That is the cost of the source, not a failure.

---

## Dependencies between sources

**Clock Drift and CPU Jitter**
- Both wake from 1 ms high-resolution timers. When one interrupt wakes both threads, their wake-up delays are related.
- CPU Jitter's second part, the counting speed, is its own.
- Together they earn at most 2 bits per millisecond. That is below the 7.188 bits measured for CPU Jitter alone, so even if Clock Drift added nothing beyond what CPU Jitter already holds, their combined credit stays under one measurement.

**Keystroke and the timing sources**
- The fine part of a keystroke timing comes from system delays, partly the same activity the timing sources measure. They are not the same path: the keystroke's comes through the input interrupt, while the timing sources' comes from the timer interrupt.
- Keystroke credit is small next to theirs, a few bits per second against up to about 2,000.

**Microphone and mouse**
- The microphone's electronic noise and the hand's motion are physical processes separate from the CPU's timing.

TRNG adds the credits as if the sources were independent. That is an assumption, and the margins are meant to cover the overlaps above.

## Limits

- **One machine.** All measurements come from one laptop. Other CPUs, keyboards, mice and microphones can measure differently. The caps are set low to leave room for that, but they are not measured on every machine.
- **Small human recordings.** The keystroke and mouse recordings are far below the 1,000,000 samples SP 800-90B expects.
- **What the health tests catch.** They look at whole values, so they catch a source that sticks or repeats, not a gradual loss of randomness in the lowest bits. Against that, there are the per-sample rule (no credit for changes of 0 or 1, or for repeats) and the caps.
- **Hidden hypervisors.** A hypervisor built to hide its hypervisor flag is not detected, and Clock Drift and CPU Jitter would then earn credit inside it.
- **Same-user programs.** A program running as the same Windows user can read TRNG's memory, and no model of the sources protects against that.
