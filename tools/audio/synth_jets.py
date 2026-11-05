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
import os, shutil, sys, wave
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
# spatial: 1 = a point source; the near roar a little less (a loud jet fills the sky).
# rate: what each loop is saved at, enough for what it holds (Jacob, 5 Nov: trim the mod's size): the far rumble
# is low-passed under 420 Hz, the near roar under 1.8 kHz (its whine to ~3.2 kHz), the scream under ~7 kHz.
NEAR = dict(curve=[(0, 1, False), (300, 1, True), (2500, 0, False)], volume=-17, spatial=0.75, rate=16000)
FAR = dict(curve=[(300, 0, False), (1200, 1, True), (6000, 0, False)], volume=-15, spatial=1, rate=11025)
# The turbojets' scream, close in only: the hot, tearing high roar of an afterburnerless turbojet going over (Jacob,
# 4 Nov: the non-turbofan jets need a better scream overhead). High frequencies die off in air fast, so it is gone by
# 1.2 km (the flyover starts it inside that: ACR_LocalFlyover.SCREAM_START_M).
SCREAM = dict(curve=[(0, 1, False), (250, 1, True), (1200, 0, False)], volume=-19, spatial=0.85, rate=24000)
SCREAM_VARIANTS = 2  # within 900 m the pass-by takes over, so this layer only plays for passes 0.9-1.2 km off
# The pass-by carries its own distance in the render: flat out to 3 km. Its loudest moment sits at -9 dBFS, about
# 7 dB over the old near loop at full, with the approach far quieter than that: the dynamics are the point.
FLYBY = dict(curve=[(0, 1, False), (3000, 1, False), (3500, 0, False)], volume=-17, spatial=0.6, oneshot=True)

JETS = {
    # hump: the roar's low hump (Hz); cut: the NEAR / FAR low-pass (Hz); whine: [(Hz, level)] in the NEAR layer;
    # scream: the turbojets' close-in roar, body centre (Hz) and tearing bursts a second; slap: its thumping
    # undertone, pitch (Hz) and thumps a second; the A-10's turbofans have neither
    "A10A": dict(speed=180, hump=180, cut=(1800, 420), whine=[(2240, 0.10), (1150, 0.04)],
                 howl=dict(f0=138, formants=[(560, 3.5), (1150, 5.0)], level=0.55), seed=1),
    "Su25": dict(speed=180, hump=140, cut=(1300, 380), whine=[(2950, 0.02)], scream=(480, 28), slap=(85, 12), seed=2),
    "RF4C": dict(speed=220, hump=110, cut=(1100, 340), whine=[], scream=(300, 34), slap=(58, 9), seed=3),  # J79: the lowest, harshest
    "Su17M4R": dict(speed=220, hump=125, cut=(1200, 360), whine=[], scream=(380, 25), slap=(70, 11), seed=4),
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


def howl(rng, cfg, cut):
    """The TF34's throaty "waaaaahhh" under its whine (Jacob, 4 Nov): a buzzy fan tone at `f0` with all its
    harmonics, voiced through two vowel resonances (`formants`: Hz, Q) like an open "ah", its pitch wavering
    slowly (+-1.5 %, zero-mean so the loop closes) with a little jitter, and some breath: noise through the same
    resonances. RMS `level` against the roar's 1."""
    f = np.maximum(freqs, 1.0)

    def vowel(fr):
        g = np.zeros_like(fr)
        for fc, q in cfg["formants"]:
            g += 1 / np.sqrt(1 + (q * (fr / fc - fc / fr)) ** 2)
        return g
    f0 = periodic(cfg["f0"])
    drift = np.sin(2 * np.pi * periodic(0.11) * t + 0.3) + 0.6 * np.sin(2 * np.pi * periodic(0.29) * t + 2.1)
    jitter = np.fft.irfft(lowpass_mag(f, 18, 2) * (f > 2) * np.exp(2j * np.pi * rng.random(len(freqs))), N)
    jitter /= np.max(np.abs(jitter))
    wobble = 0.015 * drift / np.max(np.abs(drift)) + 0.004 * jitter
    phase = 2 * np.pi * f0 * (t + np.cumsum(wobble) / RATE)
    voiced = np.zeros(N)
    for k in range(1, int(3000 / f0)):
        voiced += vowel(np.array([k * f0]))[0] / k ** 0.3 * np.sin(k * phase + rng.uniform(0, 2 * np.pi))
    voiced /= np.sqrt(np.mean(voiced ** 2))
    breath = np.fft.irfft(vowel(f) * np.exp(2j * np.pi * rng.random(len(freqs))), N)
    breath /= np.sqrt(np.mean(breath ** 2))
    x = voiced + 0.5 * breath
    x = np.fft.irfft(np.fft.rfft(x) * lowpass_mag(f, cut * 1.5, 2), N)
    return cfg["level"] * x / np.sqrt(np.mean(x ** 2))


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
        if spec.get("howl"):
            x += howl(rng, spec["howl"], cut)
    x = ground_phasing(x)
    swell = 1 + 0.18 * np.sin(2 * np.pi * periodic(0.13) * t + 0.4) + 0.08 * np.sin(2 * np.pi * periodic(0.31) * t + 2.0)
    x *= swell
    for _ in range(4):  # -16 dBFS RMS; peaks rounded off softly
        x *= 10 ** (-16 / 20) / np.sqrt(np.mean(x ** 2))
        x = 0.97 * np.tanh(x / 0.97)
    return x


def scream(spec, variant):
    """Turbojet roar torn out of the nozzle, close in. Jet mixing noise peaks near Strouhal 0.2 (0.2 U/D: low
    hundreds of Hz for a J79, AL-21 or R-95 nozzle), so the energy is a broad body round `centre` (rising below it,
    -3 dB/oct above, high-passed at 120 Hz) with only a little hiss over 2 kHz: the first try sat at 2-2.6 kHz and
    sounded like "a compressed air nozzle instead of air violently tearing out the back of a jet nozzle" (Jacob,
    4 Nov). The tearing is irregular: random bursts (Poisson, `rate` a second, 20-80 ms, random depth) that swell
    the body and carry a brighter ripping band of their own; no periodic flutter (it rattled: "rickety") and no
    impulses (the crackle was disliked). An asymmetric soft clip skews the wave so it rips rather than hisses.
    Each variant has its own noise, centre and burst rate, and the bank picks one at random each pass ("a
    predictable cadence and tone each time"). Periodic like the other loops."""
    centre, rate = spec["scream"]
    centre *= (1.0, 0.88, 1.12, 0.95)[variant]
    rate *= (1.0, 1.35, 0.75, 1.15)[variant]
    rng = np.random.default_rng(spec["seed"] * 100 + 50 + variant)
    f = np.maximum(freqs, 1.0)

    def noise(mag):
        x = np.fft.irfft(mag * np.exp(2j * np.pi * rng.random(len(freqs))), N)
        return x / np.sqrt(np.mean(x ** 2))
    highpass = 1 / np.sqrt(1 + (120 / f) ** 8)
    body_mag = (np.minimum(f / centre, 1.0) ** 1.0) * (np.maximum(f / centre, 1.0) ** -0.5) * highpass
    body_mag *= lowpass_mag(f, 5500, 3)
    body_mag += 0.08 * highpass / np.sqrt(1 + (2500 / f) ** 4) * lowpass_mag(f, 7000, 3)  # a little hiss
    body = noise(body_mag)
    rip = noise(np.exp(-0.5 * (np.log2(f / (centre * 2.5)) / 1.2) ** 2) * highpass)  # brighter, broad
    # bursts: Hann windows at Poisson times, wrapped round the loop
    env = np.zeros(N)
    count = rng.poisson(rate * SECONDS)
    for start in rng.random(count) * N:
        length = int(rng.uniform(0.02, 0.08) * RATE)
        w = np.hanning(length) * rng.uniform(0.3, 1.0)
        idx = (int(start) + np.arange(length)) % N
        env[idx] += w
    env = np.minimum(env, 1.6)
    x = body * (0.55 + 0.45 * env) + 0.7 * rip * env
    x /= np.sqrt(np.mean(x ** 2))
    x += 1.2 * slap(spec, variant, rng)
    x = ground_phasing(x, 0.3)
    x *= 1 + 0.15 * np.sin(2 * np.pi * periodic(0.19 + 0.04 * variant) * t + variant)
    # asymmetric soft clip: the positive half driven harder than the negative
    x *= 10 ** (-12 / 20) / np.sqrt(np.mean(x ** 2))
    x = np.where(x > 0, np.tanh(1.8 * x) / 1.8, np.tanh(0.9 * x) / 0.9)
    x -= np.mean(x)
    x *= 10 ** (-16 / 20) / np.sqrt(np.mean(x ** 2))
    return 0.97 * np.tanh(x / 0.97)


def slap(spec, variant, rng):
    """The thrust's slap under the tear (Jacob, 4 Nov: "the SLAP of the thrust... the deeper, almost percussive
    rumble"): irregular low pressure thumps (Poisson, `rate` a second; each a 1.5-cycle swing at `hz`, a 6-12 ms
    raised-cosine attack, so no click, and a 30-90 ms decay, random depth) over a rumble bed of band-limited noise
    that swells with them. High-passed at 30 Hz so the one-sided thumps leave no offset. RMS 1."""
    hz, rate = spec["slap"]
    hz *= (1.0, 0.92, 1.08, 0.96)[variant]
    rate *= (1.0, 1.2, 0.85, 1.1)[variant]
    f = np.maximum(freqs, 1.0)
    thumps = np.zeros(N)
    env = np.zeros(N)
    for start in rng.random(rng.poisson(rate * SECONDS)) * N:
        f0 = hz * rng.uniform(0.85, 1.15)
        attack, decay = rng.uniform(0.006, 0.012), rng.uniform(0.03, 0.09)
        n = int((attack + 5 * decay) * RATE)
        tt = np.arange(n) / RATE
        shape = np.where(tt < attack, 0.5 - 0.5 * np.cos(np.pi * tt / attack), np.exp(-(tt - attack) / decay))
        depth = rng.uniform(0.35, 1.0)
        idx = (int(start) + np.arange(n)) % N
        thumps[idx] += depth * shape * np.sin(2 * np.pi * f0 * tt)
        env[idx] += depth * shape
    bed_mag = np.exp(-0.5 * (np.log2(f / hz) / 0.9) ** 2)
    bed = np.fft.irfft(bed_mag * np.exp(2j * np.pi * rng.random(len(freqs))), N)
    bed /= np.sqrt(np.mean(bed ** 2))
    thumps /= np.sqrt(np.mean(thumps ** 2))
    x = thumps + 0.45 * bed * (0.5 + np.minimum(env, 1.5))
    x = np.fft.irfft(np.fft.rfft(x) / np.sqrt(1 + (30 / f) ** 8) * lowpass_mag(f, 400, 3), N)
    return x / np.sqrt(np.mean(x ** 2))


# ---------- the pass-by: one rendered fly-past per jet and distance ----------
# A loop at a distance-set volume can't give a jet's two signatures: the pitch falling as it passes (Doppler: the
# A-10's whine sliding down, Jacob, 4 Nov, "missing its signature noise") and the roar slamming in from behind
# just after it has gone over ("that Michael Bay oomph"). So each jet gets a 24 s pass-by rendered through the
# real geometry: the sound at the listener is the source as emitted at the retarded time (Doppler), spread 1/r,
# dulled by air (high frequencies lost with distance), plus its ground echo (a comb sweeping as the angle
# changes), with each part radiating its own way: the fan tones forward and aft, the jet's tear and slap in a
# lobe behind the nozzle. ACR_LocalFlyover starts it so its closest moment (FLYBY_PEAK_S in) lands as the jet
# passes, and lets the close-range loops go under it.
FLYBY_RATE = 24000
FLYBY_SECONDS = 24.0
FLYBY_PEAK_S = 12.0
FLYBY_DISTANCES = {"Near": 250.0, "Mid": 600.0}  # ACR_LocalFlyover picks Near under 420 m
SOUND_SPEED = 343.0


def lobe(theta, centre, width):
    return np.exp(-0.5 * ((theta - centre) / width) ** 2)


def flyby(spec, D, seed):
    v = spec["speed"]
    rng = np.random.default_rng(seed)
    loops = 6  # 72 s of source (the loops are periodic, so they tile): the approach heard in 12 s left the jet
    # over ~35 s (sound from a closing source arrives compressed)
    tile = lambda x: np.tile(x, loops)
    f = np.maximum(freqs, 1.0)
    roar_src = roar(rng, spec["hump"], 6000, 0.5)
    parts = [(tile(roar_src), "roar")]
    if spec["whine"] or spec.get("howl"):
        tone = whine(rng, spec["whine"], 6000) * 4.5  # the A-10's signature: the fan whine, up front
        if spec.get("howl"):
            tone = tone + howl(rng, spec["howl"], 4000)
        parts.append((tile(tone), "fan"))
    if spec.get("scream"):
        parts.append((tile(scream(spec, int(rng.integers(4)))), "jet"))
    src_len = N * loops
    n_out = int(FLYBY_SECONDS * FLYBY_RATE)
    tau = np.arange(n_out) / FLYBY_RATE
    hp, hl = D, 1.7  # the jet D m over the listener's ground, the listener's ear 1.7 m up
    out = np.zeros(n_out)
    for path, refl in (("direct", 1.0), ("ground", 0.55)):
        dz = hp - hl if path == "direct" else hp + hl
        # emission times on a fine grid -> reception times; invert for each output sample
        te = np.linspace(-40, 40, 400001)
        r = np.sqrt((v * te) ** 2 + dz ** 2)
        tr = te + r / SOUND_SPEED
        tr0 = (hp - hl) / SOUND_SPEED  # the direct sound's closest moment
        want = tau - FLYBY_PEAK_S + tr0
        te_out = np.interp(want, tr, te)
        r_out = np.sqrt((v * te_out) ** 2 + dz ** 2)
        cos_t = np.clip(-v * te_out / r_out, -1, 1)  # angle off the nose to the listener
        theta = np.degrees(np.arccos(cos_t))
        idx = (te_out + 50) * RATE  # source time 0 (overhead) sits 50 s into the source
        i0 = np.clip(np.floor(idx).astype(int), 0, src_len - 2)
        frac = idx - i0
        gain = refl * D / r_out / (1 - 0.3 * v / SOUND_SPEED * cos_t)
        for src, kind in parts:
            sig = src[i0] * (1 - frac) + src[i0 + 1] * frac
            if kind == "roar":
                d = 0.55 + 0.45 * lobe(theta, 130, 40)
            elif kind == "fan":
                d = 0.3 + 0.9 * lobe(theta, 45, 30) + 0.6 * lobe(theta, 125, 30)
            else:
                d = 0.12 + 1.3 * lobe(theta, 140, 28)
            out += sig * d * gain
    # air absorption by the distance of each moment (dB/m rising steeply with frequency), frame by frame
    frame, hop = 2048, 512
    win = np.hanning(frame)
    ff = np.fft.rfftfreq(frame, 1 / FLYBY_RATE)
    alpha = 0.0045 * (np.maximum(ff, 1) / 1000) ** 1.25  # dB per metre: ~0.025 at 4 kHz, ~0.06 at 8 kHz (ISO 9613-ish)
    padded = np.concatenate([np.zeros(frame), out, np.zeros(frame)])
    dist = np.concatenate([np.full(frame, 3000.0), np.sqrt((spec["speed"] * (tau - FLYBY_PEAK_S)) ** 2 + D ** 2),
                           np.full(frame, 3000.0)])
    res = np.zeros_like(padded)
    norm = np.zeros_like(padded)
    for start in range(0, len(padded) - frame, hop):
        seg = padded[start:start + frame] * win
        rr = dist[start + frame // 2]
        spec_ = np.fft.rfft(seg) * 10 ** (-alpha * rr / 20)
        res[start:start + frame] += np.fft.irfft(spec_, frame) * win
        norm[start:start + frame] += win ** 2
    out = res[frame:frame + n_out] / np.maximum(norm[frame:frame + n_out], 1e-6)
    # loudest 100 ms at -9 dBFS RMS, peaks rounded off
    k = FLYBY_RATE // 10
    short = np.sqrt(np.convolve(out ** 2, np.ones(k) / k, "same"))
    out *= 10 ** (-9 / 20) / short.max()
    out = 0.97 * np.tanh(out / 0.97)
    # fade the ends so starting and finishing make no click
    ramp = int(0.5 * FLYBY_RATE)
    out[:ramp] *= np.linspace(0, 1, ramp)
    out[-ramp:] *= np.linspace(1, 0, ramp)
    return out


def resample_loop(x, rate):
    """A periodic loop at RATE to `rate`, by its spectrum: band-limited, and it still closes seamlessly."""
    n = int(round(len(x) * rate / RATE))
    spec = np.fft.rfft(x)[:n // 2 + 1]
    return np.fft.irfft(spec, n) * n / len(x)


def write_wav(path, x, rate=RATE):
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


def curve_gain(curve, d):
    """The sound project's curve as the game plays it: linear segments, Reciprocal ones as 1/distance."""
    for (x0, y0, recip), (x1, y1, _) in zip(curve, curve[1:]):
        if x0 <= d <= x1:
            if recip and y1 == 0:  # 1/d from x0, reaching 0 at x1
                return y0 * max(0.0, (x0 / d - x0 / x1) / (1 - x0 / x1))
            return y0 + (y1 - y0) * (d - x0) / (x1 - x0)
    return curve[-1][1] if d > curve[-1][0] else curve[0][1]


def flyby_preview(layers, path, closest=400, speed=200):
    seconds = 50
    n = RATE * seconds
    tt = np.arange(n) / RATE - seconds / 2
    dist = np.sqrt(closest ** 2 + (speed * tt) ** 2)
    out = np.zeros(n)
    for loop, cfg in layers:
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
{looping}   "Termination Fade Out" 4000
   Samples {{
{samples}   }}
  }}
'''
SPATIAL = '''  SpatialityClass {{
   id {spatial}
   name "Spatiality {label}"
   version 5
   tl -512 {y160}
   spatialFactor {spatial_factor}
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
        samples = "".join('    AudioBankSampleClass Sample {\n     Filename "@WAV:%s@"\n    }\n' % w for w in wav)
        looping = '   "Loop count" 255\n   "Infinite loop" 1\n   "Fade in time" 6000\n'
        if cfg.get("oneshot"):
            looping = ""
        f = dict(ids, event=event, label=label, samples=samples, looping=looping, volume=cfg["volume"], y=y, y80=y - 80, y160=y - 160,
                 spatial_factor=cfg["spatial"],
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
    loops = [("Near", near, NEAR), ("Far", far, FAR)]
    if spec.get("scream"):
        loops += [("Scream" + ("_%d" % v if v else ""), scream(spec, v), SCREAM) for v in range(SCREAM_VARIANTS)]
    wavs = []
    for tag, loop, cfg in loops:
        wav = os.path.join(SAMPLES, "ACR_%s_Engine_%s_LP.wav" % (name, tag))
        write_wav(wav, resample_loop(loop, cfg["rate"]), cfg["rate"])
        wavs.append(posix(wav))
        job.append("WAV %s" % posix(wav))
    preview = loops[:3]
    flyby_preview([(x, cfg) for _, x, cfg in preview], os.path.join(PREVIEW, "ACR_%s_flyby.wav" % name))
    for v in range(SCREAM_VARIANTS if spec.get("scream") else 1):
        pick = loops[:2] + loops[2 + v:3 + v]
        flyby_preview([(x, cfg) for _, x, cfg in pick],
                      os.path.join(PREVIEW, "ACR_%s_overhead%s.wav" % (name, "_%d" % v if v else "")), closest=300)
    events = [("SOUND_ACR_JET_LP", "Near", [wavs[0]], NEAR), ("SOUND_ACR_JET_FAR_LP", "Far", [wavs[1]], FAR)]
    if spec.get("scream"):
        events.append(("SOUND_ACR_JET_SCREAM_LP", "Scream", wavs[2:], SCREAM))
    for k, (tag, D) in enumerate(FLYBY_DISTANCES.items()):
        wav = os.path.join(SAMPLES, "ACR_%s_Flyby_%s.wav" % (name, tag))
        write_wav(wav, flyby(spec, D, spec["seed"] * 1000 + k), FLYBY_RATE)
        job.append("WAV %s" % posix(wav))
        events.append(("SOUND_ACR_JET_FLYBY_%s" % tag.upper(), "Flyby " + tag, [posix(wav)], FLYBY))
        shutil.copy(wav, os.path.join(PREVIEW, "ACR_%s_passby_%dm.wav" % (name, D)))
    text = acp(events)
    tmpl = os.path.join(TMPL, "ACR_%s_Engine.acp" % name)
    with open(tmpl, "w", newline="\r\n") as f:
        f.write(text)
    job.append("ACP %s|%s" % (posix(os.path.join(ACPS, "ACR_%s_Engine.acp" % name)), posix(tmpl)))
    print("[audio] %s: %s loops" % (name, " + ".join(tag.lower() for tag, _, _ in loops)))
with open(os.path.join(PROFILE, "audio_job.txt"), "w") as f:
    f.write("\n".join(job) + "\n")
print("[audio] previews in " + PREVIEW)
