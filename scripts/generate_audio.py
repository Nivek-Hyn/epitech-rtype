#!/usr/bin/env python3
"""
Generate retro 80s arcade sound effects and a background music loop in WAV format.
Uses only Python standard library (wave, struct, math, random).
"""

import math
import random
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100

def write_wav(path: Path, samples: list[float]):
    """Write normalized float samples (-1.0 to 1.0) to a 16-bit mono WAV file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(SAMPLE_RATE)
        
        # Clip and convert to 16-bit signed integer
        data = bytearray()
        for s in samples:
            val = max(-1.0, min(1.0, s))
            data.extend(struct.pack("<h", int(val * 32767)))
        wav_file.writeframes(data)
    print(f"Generated {path} ({len(samples)} samples, {len(samples)/SAMPLE_RATE:.2f}s)")

def generate_shoot(path: Path):
    """Laser zap: fast downward frequency sweep."""
    duration = 0.16
    total_samples = int(duration * SAMPLE_RATE)
    samples = []
    
    start_freq = 950.0
    end_freq = 180.0
    phase = 0.0
    
    for i in range(total_samples):
        t = i / total_samples
        # Exponential frequency decay
        freq = start_freq * ((end_freq / start_freq) ** t)
        phase += 2.0 * math.pi * freq / SAMPLE_RATE
        
        # Mix square wave and sine for crunchy 8-bit arcade tone
        sine = math.sin(phase)
        square = 1.0 if sine > 0 else -1.0
        wave_val = 0.6 * square + 0.4 * sine
        
        # Amplitude envelope: fast attack, exponential decay
        env = (1.0 - t) ** 1.8
        samples.append(wave_val * env * 0.7)
        
    write_wav(path, samples)

def generate_explosion(path: Path):
    """Explosion: noise burst with low rumble and long decay."""
    duration = 0.55
    total_samples = int(duration * SAMPLE_RATE)
    samples = []
    
    last_noise = 0.0
    rumble_phase = 0.0
    
    for i in range(total_samples):
        t = i / total_samples
        # Low-pass filtered noise
        raw_noise = random.uniform(-1.0, 1.0)
        # Smoothing (filter)
        last_noise = 0.85 * last_noise + 0.15 * raw_noise
        
        # Sub-rumble sine wave dropping in frequency
        rumble_freq = max(35.0, 120.0 * (1.0 - t))
        rumble_phase += 2.0 * math.pi * rumble_freq / SAMPLE_RATE
        rumble = math.sin(rumble_phase)
        
        # Envelope: punchy start, crunchy tail
        env = math.exp(-4.5 * t)
        mixed = (0.7 * last_noise + 0.3 * rumble) * env
        samples.append(mixed * 0.9)
        
    write_wav(path, samples)

def generate_hit(path: Path):
    """Hit/Impact: short click and pitch dive."""
    duration = 0.12
    total_samples = int(duration * SAMPLE_RATE)
    samples = []
    
    phase = 0.0
    for i in range(total_samples):
        t = i / total_samples
        freq = 300.0 * (1.0 - 0.7 * t)
        phase += 2.0 * math.pi * freq / SAMPLE_RATE
        env = (1.0 - t) ** 2.5
        noise = random.uniform(-0.3, 0.3) if t < 0.15 else 0.0
        samples.append((math.sin(phase) + noise) * env * 0.6)
        
    write_wav(path, samples)

def generate_music_loop(path: Path):
    """
    Seamless retro synthwave / chiptune loop (8 bars, 130 BPM).
    """
    bpm = 130.0
    beat_dur = 60.0 / bpm
    total_beats = 16  # 4 bars in 4/4
    duration = total_beats * beat_dur
    total_samples = int(duration * SAMPLE_RATE)
    
    # Bass notes (MIDI note / freq): D2, F2, C2, G2 progression
    chord_bass = [
        73.42, 73.42, 73.42, 73.42,  # D2 (bar 1)
        87.31, 87.31, 87.31, 87.31,  # F2 (bar 2)
        65.41, 65.41, 65.41, 65.41,  # C2 (bar 3)
        98.00, 98.00, 98.00, 98.00,  # G2 (bar 4)
    ]
    
    # 16th note arpeggios
    arp_notes_bar1 = [146.83, 220.0, 293.66, 349.23, 440.0, 349.23, 293.66, 220.0] * 2  # D minor
    arp_notes_bar2 = [174.61, 261.63, 349.23, 440.0, 523.25, 440.0, 349.23, 261.63] * 2  # F major
    arp_notes_bar3 = [130.81, 196.0, 261.63, 329.63, 392.0, 329.63, 261.63, 196.0] * 2  # C major
    arp_notes_bar4 = [196.0, 246.94, 293.66, 392.0, 493.88, 392.0, 293.66, 246.94] * 2  # G major
    all_arp = arp_notes_bar1 + arp_notes_bar2 + arp_notes_bar3 + arp_notes_bar4
    
    samples = [0.0] * total_samples
    
    # 1. Generate Bassline (8th notes pulse)
    bass_phase = 0.0
    for beat_idx in range(total_beats):
        base_freq = chord_bass[beat_idx]
        for sub in range(2):  # 2 eighth notes per beat
            t_start = (beat_idx + sub * 0.5) * beat_dur
            sample_start = int(t_start * SAMPLE_RATE)
            note_len = int(0.4 * beat_dur * SAMPLE_RATE)
            for s in range(note_len):
                idx = sample_start + s
                if idx >= total_samples:
                    break
                t_sub = s / (0.4 * beat_dur * SAMPLE_RATE)
                bass_phase += 2.0 * math.pi * base_freq / SAMPLE_RATE
                # Sawtoothish bass
                saw = 2.0 * (bass_phase / (2.0 * math.pi) - math.floor(bass_phase / (2.0 * math.pi) + 0.5))
                env = (1.0 - t_sub) ** 1.3
                samples[idx] += 0.28 * saw * env
                
    # 2. Generate Lead Arpeggio (16th notes)
    sixteenth_dur = beat_dur / 4.0
    total_sixteenths = total_beats * 4
    arp_phase = 0.0
    for step in range(total_sixteenths):
        freq = all_arp[step % len(all_arp)]
        t_start = step * sixteenth_dur
        sample_start = int(t_start * SAMPLE_RATE)
        note_len = int(0.85 * sixteenth_dur * SAMPLE_RATE)
        for s in range(note_len):
            idx = sample_start + s
            if idx >= total_samples:
                break
            t_note = s / note_len
            arp_phase += 2.0 * math.pi * freq / SAMPLE_RATE
            # Square wave with soft envelope
            sq = 1.0 if math.sin(arp_phase) > 0 else -1.0
            env = math.sin(math.pi * (1.0 - t_note) ** 0.6)
            samples[idx] += 0.15 * sq * env
            
    # 3. Retro Hi-Hat / Snare rhythm
    for beat_idx in range(total_beats):
        # Hi-hat on every 8th note
        for sub in range(2):
            t_start = (beat_idx + sub * 0.5) * beat_dur
            sample_start = int(t_start * SAMPLE_RATE)
            hat_len = int(0.04 * SAMPLE_RATE)
            for s in range(hat_len):
                idx = sample_start + s
                if idx < total_samples:
                    env = (1.0 - s / hat_len) ** 3.0
                    samples[idx] += 0.08 * random.uniform(-1.0, 1.0) * env
                    
        # Snare on beats 2 and 4 (beat_idx % 2 == 1)
        if beat_idx % 2 == 1:
            t_start = beat_idx * beat_dur
            sample_start = int(t_start * SAMPLE_RATE)
            snare_len = int(0.12 * SAMPLE_RATE)
            for s in range(snare_len):
                idx = sample_start + s
                if idx < total_samples:
                    env = (1.0 - s / snare_len) ** 2.0
                    samples[idx] += 0.18 * random.uniform(-1.0, 1.0) * env
                    
    write_wav(path, samples)

def main():
    assets_dir = Path("assets/audio")
    assets_dir.mkdir(parents=True, exist_ok=True)
    
    generate_shoot(assets_dir / "shoot.wav")
    generate_explosion(assets_dir / "explosion.wav")
    generate_hit(assets_dir / "hit.wav")
    generate_music_loop(assets_dir / "music.wav")
    print("All audio files generated successfully in assets/audio/")

if __name__ == "__main__":
    main()
