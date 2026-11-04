"""Engine loops for ACA's blockout jets (Prefabs/Vehicles/Aircraft), synthesised: no recordings, so nothing to license.

Each jet is heard from the ground 300-500 m away, so what matters is the character at a distance: a turbojet's
broadband roar and crackle (RF-4C's J79s, Su-17's AL-21F, Su-25's R-95Sh) or a turbofan's whine over a softer
roar (the A-10's TF34 'hog whistle'). Every layer is periodic over the loop (noise built in the frequency domain,
tones and modulation on whole cycles, crackle wrapped round), so the loop repeats without a click.

Run with any Python that has numpy (Blender's has):
  "<Blender>/5.2/python/bin/python.exe" tools/audio/synth_jets.py <addon root>
Writes Sounds/Vehicles/Aircraft/Samples/ACR_<jet>_Engine_LP.wav (48 kHz mono 16-bit), the sound project templates
and the audio job for tools/Build-AircraftAssets.ps1, which registers the samples and writes the .acp files
(Sounds/Vehicles/Aircraft/ACR_<jet>_Engine.acp, event SOUND_ACR_JET_LP) once their resource names exist.
Previews: .local-aircraft/audio/ACR_<jet>_flyby.wav, a pass at 400 m with distance and air absorption.
"""
import os, sys, wave
import numpy as np

ROOT = os.path.abspath(sys.argv[1])
RATE = 48000
SECONDS = 10.0
N = int(RATE * SECONDS)
SAMPLES = os.path.join(ROOT, "Sounds", "Vehicles", "Aircraft", "Samples")
ACPS = os.path.join(ROOT, "Sounds", "Vehicles", "Aircraft")
PROFILE = os.path.join(ROOT, ".local-workbench", "profile")
TMPL = os.path.join(PROFILE, "acp")
PREVIEW = os.path.join(ROOT, ".local-aircraft", "audio")
for d in (SAMPLES, TMPL, PREVIEW):
    os.makedirs(d, exist_ok=True)
posix = lambda q: q.replace("\\", "/")

# Loudness: the RHS FA-18 is too loud, cuts in abruptly and stops too soon (Jacob, 4 Nov). These sit under gunfire, carry far
# and fade out slowly: full level within FULL_M, falling as 1/distance to nothing at SILENT_M (the flyover keeps
# the model until the listener is that far away). The bank volume sets the overall level; tune it here.
FULL_M = 400
SILENT_M = 5000
BANK_DB = -9

JETS = {
    # roar: spectral peak (Hz), its width (octaves), level; crackle: pops per second, level;
    # whine: [(Hz, level)], wobble: slow pitch drift of the tones (fraction); beat: two engines' slow beating (Hz)
    "A10A": dict(roar=(260, 1.6, 0.55), crackle=(0, 0), whine=[(1150, 0.10), (2240, 0.30), (4480, 0.07), (6720, 0.03)],
                 wobble=0.004, beat=0.7, seed=1),
    "Su25": dict(roar=(190, 1.9, 0.9), crackle=(28, 0.18), whine=[(2950, 0.06), (5900, 0.02)],
                 wobble=0.003, beat=0.9, seed=2),
    "RF4C": dict(roar=(150, 2.1, 1.0), crackle=(55, 0.35), whine=[(2600, 0.035)], wobble=0.002, beat=1.3, seed=3),
    "Su17M4R": dict(roar=(170, 2.0, 1.0), crackle=(40, 0.28), whine=[(3100, 0.04)], wobble=0.002, beat=0.0, seed=4),
}

freqs = np.fft.rfftfreq(N, 1 / RATE)
t = np.arange(N) / RATE


def periodic(hz):
    """The nearest frequency that fits a whole number of cycles in the loop."""
    return max(1, round(hz * SECONDS)) / SECONDS


def shaped_noise(rng, peak, octaves):
    """Noise with a log-normal spectral hump at `peak` Hz and a gentle -3 dB/oct tail: periodic by construction."""
    f = np.maximum(freqs, 1.0)
    hump = np.exp(-0.5 * (np.log2(f / peak) / (octaves / 2)) ** 2)
    tail = 0.25 * (peak / f) ** 0.5 * (f > peak)
    mag = (hump + tail) * (f > 25)
    spec = mag * np.exp(2j * np.pi * rng.random(len(freqs)))
    x = np.fft.irfft(spec, N)
    return x / np.sqrt(np.mean(x ** 2))


def bandpass(x, lo, hi):
    spec = np.fft.rfft(x)
    spec *= (freqs > lo) & (freqs < hi)
    return np.fft.irfft(spec, N)


def crackle(rng, rate, level):
    """Turbojet crackle: short decaying bursts at random times (wrapped round the loop), high-passed."""
    if rate <= 0:
        return np.zeros(N)
    x = np.zeros(N)
    count = int(rate * SECONDS)
    for _ in range(count):
        at = rng.integers(N)
        length = int(RATE * rng.uniform(0.002, 0.012))
        burst = rng.standard_normal(length) * np.exp(-np.linspace(0, 6, length)) * rng.lognormal(0, 0.6)
        idx = (at + np.arange(length)) % N
        x[idx] += burst
    x = bandpass(x, 400, 9000)
    return level * x / (np.sqrt(np.mean(x ** 2)) + 1e-9)


def tones(rng, whine, wobble):
    x = np.zeros(N)
    drift = np.sin(2 * np.pi * periodic(0.2) * t) + 0.5 * np.sin(2 * np.pi * periodic(0.53) * t + 1.3)
    for hz, level in whine:
        f0 = periodic(hz)
        # phase of a tone whose pitch drifts by `wobble`: integrate f0 * (1 + wobble * drift); the drift is
        # periodic with zero mean, so the phase still closes the loop exactly
        phase = 2 * np.pi * f0 * (t + wobble * np.cumsum(drift) / RATE)
        x += level * np.sqrt(2) * np.sin(phase + rng.uniform(0, 2 * np.pi))
        # a little noise round each tone, so it sounds like blades, not a sine generator
        x += 0.4 * level * bandpass(rng.standard_normal(N), hz * 0.97, hz * 1.03) * 3
    return x


def engine(spec):
    rng = np.random.default_rng(spec["seed"])
    peak, octaves, roar_level = spec["roar"]
    x = roar_level * shaped_noise(rng, peak, octaves)
    x += crackle(rng, *spec["crackle"])
    x += tones(rng, spec["whine"], spec["wobble"])
    # turbulence on the way to the listener, and two engines beating
    am = 1 + 0.12 * np.sin(2 * np.pi * periodic(0.37) * t + 0.4) + 0.07 * np.sin(2 * np.pi * periodic(1.1) * t + 2.0)
    if spec["beat"]:
        am *= 1 + 0.08 * np.sin(2 * np.pi * periodic(spec["beat"]) * t)
    x *= am
    x /= np.sqrt(np.mean(x ** 2))
    # -16 dBFS RMS for all four (the bank volume sets the level); crackle peaks are rounded off by a soft
    # limiter rather than turning the whole loop down
    for _ in range(4):
        x *= 10 ** (-16 / 20) / np.sqrt(np.mean(x ** 2))
        x = 0.97 * np.tanh(x / 0.97)
    return x


def write_wav(path, x):
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


def flyby_preview(loop, path, closest=400, speed=200):
    """What a listener hears as the jet passes `closest` m away: the acp's distance curve plus air absorption
    (highs fade with distance). Not used by the game: for listening to the loops before Workbench."""
    seconds = 30
    n = RATE * seconds
    x = np.tile(loop, int(np.ceil(n / N)))[:n]
    tt = np.arange(n) / RATE - seconds / 2
    dist = np.sqrt(closest ** 2 + (speed * tt) ** 2)
    gain = np.where(dist <= FULL_M, 1.0, FULL_M / dist) * np.clip((SILENT_M - dist) / (SILENT_M - FULL_M) * 4, 0, 1)
    out = np.zeros(n)
    blocks = 64
    edges = np.linspace(0, n, blocks + 1).astype(int)
    for a, b in zip(edges, edges[1:]):  # absorption by blocks: low-pass falling from 12 kHz towards 1.5 kHz
        d = dist[(a + b) // 2]
        cutoff = 12000 * (closest / d) ** 0.9 + 1500
        seg = np.fft.rfft(x[a:b])
        f = np.fft.rfftfreq(b - a, 1 / RATE)
        seg /= np.sqrt(1 + (f / cutoff) ** 4)
        out[a:b] = np.fft.irfft(seg, b - a) * gain[a:b]
    out *= 10 ** (BANK_DB / 20) * 2.5
    write_wav(path, out)


ACP = r'''AudioClass {
 sounds {
  SoundClass {
   id 3081
   name "SOUND_ACR_JET_LP"
   version 5
   tl -256 0
   ins {
    ConnectionsClass connections {
     id 64
     links {
      ConnectionClass connection {
       id 2066
       port 65
      }
     }
    }
   }
   outState 4107
   outStatePort 181255
   noInAudible 0
  }
 }
 mixers {
  MixerClass {
   id 4107
   name "OutputState 4"
   version 4
   res "{B764D803219C775E}Sounds/FinalMix.afm"
   ins {
    ConnectionsClass connections {
     id 181255
     links {
      ConnectionClass connection {
       id 3081
       port 65
      }
     }
    }
    ConnectionsClass connections {
     id 197639
     links {
      ConnectionClass connection {
       id 7197
       port 65
      }
     }
    }
   }
   pi {
    8912896 0
   }
   path "{B764D803219C775E}Sounds/FinalMix.afm"
   inputs {
    "WPN_Handling" "WPN_Shots" "WPN_Explosions" "WNP_BulletHits" "CHAR" "ENV_AMB_2D" "VEH_Animations" "Impacts" "Dialog" "Music" "ENV_Doors" "VEH_Engine" "VEH_Tires" "VON" "SFX" "SFX_Reverb" "VON_Reverb" "Dialog_Reverb" "Impacts_EXT" "ENV_AMB_3D" "WPN_SonicCracks" "CHAR_Gear" "PA" "SFX_Reverb_Exterior" "UI" "ENV_AMB_3D_Reverb_Exterior" "SFX_Direct" "SFX_Reverb_Small" "SFX_Reverb_Medium" "SFX_Reverb_Large" "WPN_Shots_Player" "Dialog_Reverb_Small" "Dialog_Reverb_Medium" "Dialog_Reverb_Large" "WPN_TravelingProjectile"
   }
  }
 }
 shaders {
  ShaderClass {
   id 2066
   name "Shader Jet"
   version 5
   tl -512 0
   ins {
    ConnectionsClass connections {
     id 0
     links {
      ConnectionClass connection {
       id 6172
       port 65
      }
     }
    }
    ConnectionsClass connections {
     id 1
     links {
      ConnectionClass connection {
       id 5139
       port 65
      }
     }
    }
    ConnectionsClass connections {
     id 64
     links {
      ConnectionClass connection {
       id 1028
       port 65
      }
     }
    }
   }
   pi {
    3 0
   }
  }
 }
 amplitudes {
  AmplitudeClass {
   id 5139
   name "Amplitude"
   version 5
   tl -512 -80
   curve Custom
   AttenuationCurve {
    CurvePoint "1" {
     Y 1
    }
    CurvePoint "2" {
     X @FULL@
     Y 1
     Shape Reciprocal
     Modifier 0.5
    }
    CurvePoint "3" {
     X @SILENT@
    }
   }
  }
 }
 banks_local {
  BankLocalClass {
   id 1028
   name "Engine"
   version 7
   tl -768 0
   Volume @VOLUME@
   "Loop count" 255
   "Infinite loop" 1
   "Termination Fade Out" 4000
   "Fade in time" 6000
   Samples {
    AudioBankSampleClass Sample {
     Filename "@WAV:@SAMPLE@@"
    }
   }
  }
 }
 spatialities {
  SpatialityClass {
   id 6172
   name "Spatiality"
   version 5
   tl -512 -160
   spatialFactor 1
  }
 }
 auxOuts {
  AuxOutClass {
   id 7197
   name "AuxOut EXT Reverb"
   version 5
   tl 0 -80
   ins {
    ConnectionsClass connections {
     id 0
     links {
      ConnectionClass connection {
       id 3081
       port 5
      }
     }
    }
   }
   pi {
    1 0
   }
   OSPort 197639
   Volume -12
  }
 }
 version 1
}
'''
# (Graph copied from vanilla's distant-helicopter loop, Sounds/Environment/Misc/Helicopters: one looping bank,
# 3D spatiality, a distance curve, to the 3D ambience bus with an exterior reverb send.)

job = []
for name, spec in JETS.items():
    loop = engine(spec)
    wav = os.path.join(SAMPLES, "ACR_%s_Engine_LP.wav" % name)
    write_wav(wav, loop)
    flyby_preview(loop, os.path.join(PREVIEW, "ACR_%s_flyby.wav" % name))
    text = (ACP.replace("@FULL@", str(FULL_M)).replace("@SILENT@", str(SILENT_M)).replace("@VOLUME@", str(BANK_DB))
            .replace("@SAMPLE@", posix(wav)))
    tmpl = os.path.join(TMPL, "ACR_%s_Engine.acp" % name)
    with open(tmpl, "w", newline="\r\n") as f:
        f.write(text)
    job.append("WAV %s" % posix(wav))
    job.append("ACP %s|%s" % (posix(os.path.join(ACPS, "ACR_%s_Engine.acp" % name)), posix(tmpl)))
    print("[audio] %s: %s" % (name, wav))
with open(os.path.join(PROFILE, "audio_job.txt"), "w") as f:
    f.write("\n".join(job) + "\n")
print("[audio] previews in " + PREVIEW)
