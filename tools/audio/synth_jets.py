"""Engine sound for ACA's blockout jets (Prefabs/Vehicles/Aircraft), synthesised: no recordings, so nothing to license.

The jets are only ever heard from the ground, 300 m to 6 km away, so the loops are made to sound like that, not
like an engine at arm's length (Jacob, 4 Nov: the first try was a dinky rocket motor with a dreadful crackle, too
loud, as if right by his head, with no sense of distance):
 - a deep roar: -3 dB/octave (-6 in the far layer) with a hump at 110-180 Hz, nothing under ~35 Hz (that would
   be a sub hum on most speakers), and the highs taken off by the air (a steep low-pass);
 - the rolling 'phasing' of a jet passing overhead: its sound bounces off the ground and the echo's delay keeps
   changing, which sweeps notches through the roar (a comb filter with a slowly swept delay);
 - slow swells from turbulence; for the A-10 a faint TF34 whistle through the roar; no crackle.
Two layers per jet, crossfaded by distance in the sound project: NEAR (heard within ~2.5 km, low-passed at around
1-2 kHz) and FAR (a dull rumble, low-passed near 400 Hz, carrying to 6 km). Every layer is periodic over the loop
(noise built in the frequency domain, modulation and sweep on whole cycles, delays wrapped round), so the loop
repeats without a click.

Run with any Python that has numpy (Blender's has):
  "<Blender>/5.2/python/bin/python.exe" tools/audio/synth_jets.py <addon root>
Writes Sounds/Vehicles/Aircraft/Samples/ACR_<jet>_Engine_{Near,Far}_LP.wav (48 kHz mono 16-bit), the sound project
templates and the audio job for tools/Build-AircraftAssets.ps1, which registers the samples and writes
Sounds/Vehicles/Aircraft/ACR_<jet>_Engine.acp (events SOUND_ACR_JET_LP and SOUND_ACR_JET_FAR_LP) once the samples
have resource names. Previews: .local-aircraft/audio/ACR_<jet>_flyby.wav, a pass 400 m overhead with both layers
at their distance levels (not used by the game: for listening before Workbench).
"""
import os, sys, wave
import numpy as np

ROOT = os.path.abspath(sys.argv[1])
RATE = 48000
SECONDS = 12.0
N = int(RATE * SECONDS)
SAMPLES = os.path.join(ROOT, "Sounds", "Vehicles", "Aircraft", "Samples")
ACPS = os.path.join(ROOT, "Sounds", "Vehicles", "Aircraft")
PROFILE = os.path.join(ROOT, ".local-workbench", "profile")
TMPL = os.path.join(PROFILE, "acp")
PREVIEW = os.path.join(ROOT, ".local-aircraft", "audio")
for d in (SAMPLES, TMPL, PREVIEW):
    os.makedirs(d, exist_ok=True)
posix = lambda q: q.replace("\\", "/")

# Distance curves (gain 0-1 by metres) and bank volumes (dB). The flyover starts the sound at 5.8 km (silent here)
# and keeps the jet until the listener is 6 km away (ACR_LocalFlyover.SOUND_START_M / SOUND_KEEP_M).
NEAR = dict(curve=[(0, 1, False), (300, 1, True), (2500, 0, False)], volume=-22)
FAR = dict(curve=[(300, 0, False), (1200, 1, True), (6000, 0, False)], volume=-20)

JETS = {
    # hump: the roar's low hump (Hz); cut: the NEAR / FAR low-pass (Hz); whine: [(Hz, level)] in the NEAR layer
    "A10A": dict(hump=180, cut=(1800, 420), whine=[(2240, 0.10), (1150, 0.04)], seed=1),
    "Su25": dict(hump=140, cut=(1300, 380), whine=[(2950, 0.02)], seed=2),
    "RF4C": dict(hump=110, cut=(1100, 340), whine=[], seed=3),
    "Su17M4R": dict(hump=125, cut=(1200, 360), whine=[], seed=4),
}

freqs = np.fft.rfftfreq(N, 1 / RATE)
t = np.arange(N) / RATE


def periodic(hz):
    """The nearest frequency that fits a whole number of cycles in the loop."""
    return max(1, round(hz * SECONDS)) / SECONDS


def lowpass_mag(f, cut, order=4):
    return 1 / np.sqrt(1 + (f / cut) ** (2 * order))


def roar(rng, hump, cut, slope):
    """Noise falling `slope` (0.5: -3 dB/oct, 1: -6) with a +8 dB hump at `hump` Hz, high-passed at 35 Hz,
    low-passed at `cut`."""
    f = np.maximum(freqs, 1.0)
    highpass = 1 / np.sqrt(1 + (35 / f) ** 4)
    mag = f ** -slope * (1 + 1.5 * np.exp(-0.5 * (np.log2(f / hump) / 0.9) ** 2)) * highpass * lowpass_mag(f, cut)
    x = np.fft.irfft(mag * np.exp(2j * np.pi * rng.random(len(freqs))), N)
    return x / np.sqrt(np.mean(x ** 2))


def whine(rng, tones, cut):
    x = np.zeros(N)
    drift = np.sin(2 * np.pi * periodic(0.17) * t) + 0.5 * np.sin(2 * np.pi * periodic(0.41) * t + 1.3)
    for hz, level in tones:
        f0 = periodic(hz)
        phase = 2 * np.pi * f0 * (t + 0.003 * np.cumsum(drift) / RATE)  # the drift has zero mean: phase closes
        x += level * np.sqrt(2) * np.sin(phase + rng.uniform(0, 2 * np.pi))
    if not tones:
        return x
    spec = np.fft.rfft(x) * lowpass_mag(np.maximum(freqs, 1.0), cut * 1.8, 2)  # a whistle carries a bit further
    return np.fft.irfft(spec, N)


def ground_phasing(x, depth=0.65):
    """Direct sound plus its ground echo, the echo's delay swept 1.5-9 ms and back once a loop: moving notches."""
    tau = 0.00525 + 0.00375 * np.sin(2 * np.pi * t / SECONDS + 0.7)
    pos = (np.arange(N) - tau * RATE) % N
    i0 = np.floor(pos).astype(int)
    frac = pos - i0
    echo = x[i0] * (1 - frac) + x[(i0 + 1) % N] * frac
    return x + depth * echo


def layer(spec, cut, with_whine):
    rng = np.random.default_rng(spec["seed"] * 10 + (1 if with_whine else 2))
    x = roar(rng, spec["hump"], cut, 0.5 if with_whine else 1.0)
    if with_whine:
        x += whine(rng, spec["whine"], cut)
    x = ground_phasing(x)
    swell = 1 + 0.18 * np.sin(2 * np.pi * periodic(0.13) * t + 0.4) + 0.08 * np.sin(2 * np.pi * periodic(0.31) * t + 2.0)
    x *= swell
    for _ in range(4):  # -16 dBFS RMS; peaks rounded off softly
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


def curve_gain(curve, d):
    """The sound project's curve as the game plays it: linear segments, Reciprocal ones as 1/distance."""
    for (x0, y0, recip), (x1, y1, _) in zip(curve, curve[1:]):
        if x0 <= d <= x1:
            if recip and y1 == 0:  # 1/d from x0, reaching 0 at x1
                return y0 * max(0.0, (x0 / d - x0 / x1) / (1 - x0 / x1))
            return y0 + (y1 - y0) * (d - x0) / (x1 - x0)
    return curve[-1][1] if d > curve[-1][0] else curve[0][1]


def flyby_preview(near, far, path, closest=400, speed=200):
    seconds = 50
    n = RATE * seconds
    tt = np.arange(n) / RATE - seconds / 2
    dist = np.sqrt(closest ** 2 + (speed * tt) ** 2)
    out = np.zeros(n)
    for loop, cfg in ((near, NEAR), (far, FAR)):
        x = np.tile(loop, int(np.ceil(n / N)))[:n]
        gain = np.array([curve_gain(cfg["curve"], d) for d in dist[::480]])
        out += x * np.repeat(gain, 480)[:n] * 10 ** (cfg["volume"] / 20)
    out *= 10 ** (14 / 20)  # roughly where the game's mix puts it; listen at a normal volume
    write_wav(path, out)


SOUND = '''  SoundClass {{
   id {sound}
   name "{event}"
   version 5
   tl -256 {y}
   ins {{
    ConnectionsClass connections {{
     id 64
     links {{
      ConnectionClass connection {{
       id {shader}
       port 65
      }}
     }}
    }}
   }}
   outState 4107
   outStatePort 181255
   noInAudible 0
  }}
'''
SHADER = '''  ShaderClass {{
   id {shader}
   name "Shader {label}"
   version 5
   tl -512 {y}
   ins {{
    ConnectionsClass connections {{
     id 0
     links {{
      ConnectionClass connection {{
       id {spatial}
       port 65
      }}
     }}
    }}
    ConnectionsClass connections {{
     id 1
     links {{
      ConnectionClass connection {{
       id {amp}
       port 65
      }}
     }}
    }}
    ConnectionsClass connections {{
     id 64
     links {{
      ConnectionClass connection {{
       id {bank}
       port 65
      }}
     }}
    }}
   }}
   pi {{
    3 0
   }}
  }}
'''
AMP = '''  AmplitudeClass {{
   id {amp}
   name "Amplitude {label}"
   version 5
   tl -512 {y80}
   curve Custom
   AttenuationCurve {{
{points}   }}
  }}
'''
BANK = '''  BankLocalClass {{
   id {bank}
   name "Engine {label}"
   version 7
   tl -768 {y}
   Volume {volume}
   "Loop count" 255
   "Infinite loop" 1
   "Termination Fade Out" 4000
   "Fade in time" 6000
   Samples {{
    AudioBankSampleClass Sample {{
     Filename "@WAV:{wav}@"
    }}
   }}
  }}
'''
SPATIAL = '''  SpatialityClass {{
   id {spatial}
   name "Spatiality {label}"
   version 5
   tl -512 {y160}
   spatialFactor 1
  }}
'''
AUX = '''  AuxOutClass {{
   id {aux}
   name "AuxOut EXT Reverb {label}"
   version 5
   tl 0 {y80}
   ins {{
    ConnectionsClass connections {{
     id 0
     links {{
      ConnectionClass connection {{
       id {sound}
       port 5
      }}
     }}
    }}
   }}
   pi {{
    1 0
   }}
   OSPort 197639
   Volume -12
  }}
'''
LINK = '''      ConnectionClass connection {{
       id {id}
       port 65
      }}
'''
MIXER = '''  MixerClass {{
   id 4107
   name "OutputState 4"
   version 4
   res "{{B764D803219C775E}}Sounds/FinalMix.afm"
   ins {{
    ConnectionsClass connections {{
     id 181255
     links {{
{sounds}     }}
    }}
    ConnectionsClass connections {{
     id 197639
     links {{
{auxes}     }}
    }}
   }}
   pi {{
    8912896 0
   }}
   path "{{B764D803219C775E}}Sounds/FinalMix.afm"
   inputs {{
    "WPN_Handling" "WPN_Shots" "WPN_Explosions" "WNP_BulletHits" "CHAR" "ENV_AMB_2D" "VEH_Animations" "Impacts" "Dialog" "Music" "ENV_Doors" "VEH_Engine" "VEH_Tires" "VON" "SFX" "SFX_Reverb" "VON_Reverb" "Dialog_Reverb" "Impacts_EXT" "ENV_AMB_3D" "WPN_SonicCracks" "CHAR_Gear" "PA" "SFX_Reverb_Exterior" "UI" "ENV_AMB_3D_Reverb_Exterior" "SFX_Direct" "SFX_Reverb_Small" "SFX_Reverb_Medium" "SFX_Reverb_Large" "WPN_Shots_Player" "Dialog_Reverb_Small" "Dialog_Reverb_Medium" "Dialog_Reverb_Large" "WPN_TravelingProjectile"
   }}
  }}
'''


def curve_points(curve):
    out = ""
    for i, (x, y, recip) in enumerate(curve):
        out += '    CurvePoint "%d" {\n' % (i + 1)
        if x:
            out += "     X %g\n" % x
        if y:
            out += "     Y %g\n" % y
        if recip:
            out += "     Shape Reciprocal\n     Modifier 0.5\n"
        out += "    }\n"
    return out


def acp(layers):
    """One sound project, one event per layer. The graph is vanilla's distant-helicopter loop
    (Sounds/Environment/Misc/Helicopters): looping bank, 3D spatiality, distance curve, to the 3D ambience bus
    with an exterior reverb send; node ids keep vanilla's pattern (a counter in the high byte, the node type in
    the low byte)."""
    parts = {k: "" for k in ("sounds", "shaders", "amps", "banks", "spatials", "auxes")}
    sound_links = aux_links = ""
    for k, (event, label, wav, cfg) in enumerate(layers):
        base = k * 0x1C00  # the second layer's counters start past the first's
        ids = dict(bank=0x0404 + base, shader=0x0812 + base, sound=0x0C09 + base, amp=0x1413 + base,
                   spatial=0x181C + base, aux=0x1C1D + base)
        y = k * 400
        f = dict(ids, event=event, label=label, wav=wav, volume=cfg["volume"], y=y, y80=y - 80, y160=y - 160,
                 points=curve_points(cfg["curve"]))
        parts["sounds"] += SOUND.format(**f)
        parts["shaders"] += SHADER.format(**f)
        parts["amps"] += AMP.format(**f)
        parts["banks"] += BANK.format(**f)
        parts["spatials"] += SPATIAL.format(**f)
        parts["auxes"] += AUX.format(**f)
        sound_links += LINK.format(id=ids["sound"])
        aux_links += LINK.format(id=ids["aux"])
    return ("AudioClass {\n sounds {\n" + parts["sounds"] + " }\n mixers {\n"
            + MIXER.format(sounds=sound_links, auxes=aux_links) + " }\n shaders {\n" + parts["shaders"]
            + " }\n amplitudes {\n" + parts["amps"] + " }\n banks_local {\n" + parts["banks"]
            + " }\n spatialities {\n" + parts["spatials"] + " }\n auxOuts {\n" + parts["auxes"] + " }\n version 1\n}\n")


job = []
for name, spec in JETS.items():
    near = layer(spec, spec["cut"][0], True)
    far = layer(spec, spec["cut"][1], False)
    wavs = []
    for tag, loop in (("Near", near), ("Far", far)):
        wav = os.path.join(SAMPLES, "ACR_%s_Engine_%s_LP.wav" % (name, tag))
        write_wav(wav, loop)
        wavs.append(posix(wav))
        job.append("WAV %s" % posix(wav))
    flyby_preview(near, far, os.path.join(PREVIEW, "ACR_%s_flyby.wav" % name))
    text = acp([("SOUND_ACR_JET_LP", "Near", wavs[0], NEAR), ("SOUND_ACR_JET_FAR_LP", "Far", wavs[1], FAR)])
    tmpl = os.path.join(TMPL, "ACR_%s_Engine.acp" % name)
    with open(tmpl, "w", newline="\r\n") as f:
        f.write(text)
    job.append("ACP %s|%s" % (posix(os.path.join(ACPS, "ACR_%s_Engine.acp" % name)), posix(tmpl)))
    print("[audio] %s: near + far loops" % name)
with open(os.path.join(PROFILE, "audio_job.txt"), "w") as f:
    f.write("\n".join(job) + "\n")
print("[audio] previews in " + PREVIEW)
