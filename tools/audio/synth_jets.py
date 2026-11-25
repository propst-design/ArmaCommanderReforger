"""Engine sound for ACA's blockout jets (Prefabs/Vehicles/Aircraft), synthesised: no recordings, so nothing to license.

The jets are only ever heard from the ground, 300 m to 6 km away, so the loops and rendered pass-bys are built
around real outdoor jet acoustics:
 - Turbojets (RF-4C, Su-17M4R, Su-25): deep mixing roar, aerodynamic approach rush (plus subtle J79/R-95 intake
   character), and a continuous tearing aft exhaust cone (broadband turbulent shear + high-skewness shock-crackle
   + continuous low-end pressure billow, no discrete popping/flutter);
 - High-bypass turbofan (A-10A TF34-GE-100): leaner lows, unmistakable detuned twin-engine 28-blade fan whistle
   (~2.72 kHz BPF + partials), hollow upper fan-duct moan, and airy cold-stream bypass whoosh;
 - Realistic outdoor ground bounce: low-passed (~850-900 Hz) at modest depth so overhead phasing rolls naturally
   without metallic high-frequency flanging;
 - Rendered 24 s pass-bys (Near 250 m, Mid 600 m) with retarded-time Doppler (Catmull-Rom cubic interpolation)
   and true retarded-distance ISO-9613-style air absorption.
Every loop layer is periodic over 12 s (frequency-domain noise, whole-cycle modulation, wrapped delays), so loops
repeat without a click.

Run with any Python that has numpy (Blender's has):
  "<Blender>/5.2/python/bin/python.exe" tools/audio/synth_jets.py <addon root>
Writes Sounds/Vehicles/Aircraft/Samples/ACR_<jet>_Engine_{Near,Far,Scream}_LP.wav and ACR_<jet>_Flyby_{Near,Mid}.wav,
the sound project templates, and the audio job for tools/Build-AircraftAssets.ps1. Previews land in
.local-aircraft/audio/ (not used by the game: for listening before Workbench).
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

# Distance curves (gain 0-1 by metres), bank volumes (dB), and output sample rates.
# Matches commit ed0b5d4: Near loops at 48 kHz and pass-bys at 32 kHz for full close-in treble; Far stays trimmed.
NEAR = dict(curve=[(0, 1, False), (300, 1, True), (2500, 0, False)], volume=-17, spatial=0.75, rate=48000)
FAR = dict(curve=[(300, 0, False), (1200, 1, True), (6000, 0, False)], volume=-15, spatial=1, rate=11025)
SCREAM = dict(curve=[(0, 1, False), (250, 1, True), (1200, 0, False)], volume=-19, spatial=0.85, rate=24000)
SCREAM_VARIANTS = 2  # within 900 m the pass-by takes over, so this layer only plays for passes 0.9-1.2 km off
FLYBY = dict(curve=[(0, 1, False), (3000, 1, False), (3500, 0, False)], volume=-17, spatial=0.6, oneshot=True)

FLYBY_RATE = 32000
FLYBY_SECONDS = 24.0
FLYBY_PEAK_S = 12.0
FLYBY_DISTANCES = {"Near": 250.0, "Mid": 600.0}  # ACR_LocalFlyover picks Near under 420 m
SOUND_SPEED = 343.0

JETS = {
    "A10A": dict(
        turbofan=True,
        speed=175.0,
        hump=195.0,
        cut=(3200.0, 420.0),
        bpf=2720.0,
        whistle_gain=1.20,
        moan_gain=0.80,
        whoosh_gain=1.05,
        core_gain=0.60,
        seed=302,
    ),
    "Su25": dict(
        turbofan=False,
        speed=185.0,
        hump=135.0,
        cut=(1500.0, 380.0),
        whine=[(2620.0, 0.16, 20.0)],
        howl=dict(f0=155.0, formants=[(620.0, 2.0), (1250.0, 2.4)], level=0.18, cut=2600.0, detunes=(0.994, 1.006)),
        fan_gain=0.65,
        rush_gain=0.45,
        tear=dict(tear_centre=340.0, crackle=0.35, sub_hz=76.0, sub_level=0.92),
        tear_gain=1.15,
        seed=102,
    ),
    "RF4C": dict(
        turbofan=False,
        speed=220.0,
        hump=105.0,
        cut=(1400.0, 340.0),
        whine=[(1750.0, 0.14, 14.0)],
        howl=dict(f0=195.0, formants=[(780.0, 3.2), (1580.0, 3.8)], level=0.26, cut=3200.0, detunes=(0.990, 1.010)),
        fan_gain=0.85,
        rush_gain=0.50,
        tear=dict(tear_centre=380.0, crackle=0.72, sub_hz=58.0, sub_level=0.95),
        tear_gain=1.45,
        seed=103,
    ),
    "Su17M4R": dict(
        turbofan=False,
        speed=220.0,
        hump=120.0,
        cut=(1500.0, 360.0),
        whine=[],
        howl=None,
        fan_gain=0.0,
        rush_gain=0.58,
        tear=dict(tear_centre=460.0, crackle=0.82, sub_hz=68.0, sub_level=0.85),
        tear_gain=1.40,
        seed=104,
    ),
}

freqs = np.fft.rfftfreq(N, 1.0 / RATE)
t = np.arange(N) / RATE


def periodic(hz):
    """The nearest frequency that fits a whole number of cycles in the loop."""
    return max(1, round(hz * SECONDS)) / SECONDS


def lowpass_mag(f, cut, order=4):
    return 1.0 / np.sqrt(1.0 + (f / cut) ** (2 * order))


def highpass_mag(f, cut, order=4):
    return 1.0 / np.sqrt(1.0 + (cut / np.maximum(f, 1e-3)) ** (2 * order))


def lowpass_signal(x, sr, cut, order=2):
    ff = np.fft.rfftfreq(len(x), 1.0 / sr)
    spec = np.fft.rfft(x) * lowpass_mag(ff, cut, order)
    return np.fft.irfft(spec, len(x))


def colored_noise(rng, mag):
    x = np.fft.irfft(mag * np.exp(2j * np.pi * rng.random(len(freqs))), N)
    rms = np.sqrt(np.mean(x ** 2))
    return x / max(rms, 1e-12)


def smooth_turb(rng, f_lo, f_hi, order=2):
    """Smooth, continuous, periodic turbulence modulation (no discrete pulsing)."""
    f = np.maximum(freqs, 1e-3)
    mag = highpass_mag(f, f_lo, order) * lowpass_mag(f, f_hi, order) * (f ** -0.5)
    mag[0] = 0.0
    x = np.fft.irfft(mag * np.exp(2j * np.pi * rng.random(len(freqs))), N)
    return x / max(np.std(x), 1e-12)


def roar_layer(rng, hump, cut, slope=0.55):
    """Deep jet mixing roar with slow organic turbulence swells."""
    f = np.maximum(freqs, 1.0)
    hp = highpass_mag(f, 32.0, 2)
    hump_boost = 1.0 + 1.8 * np.exp(-0.5 * (np.log2(f / hump) / 0.85) ** 2)
    mag = (f ** -slope) * hump_boost * hp * lowpass_mag(f, cut, 3)
    x = colored_noise(rng, mag)
    env = np.clip(1.0 + 0.22 * smooth_turb(rng, 0.15, 1.8, 2), 0.55, 1.55)
    x *= env
    return x / max(np.sqrt(np.mean(x ** 2)), 1e-12)


def twin_fan_whine(rng, tones, twin_detune=0.012):
    """Aerodynamic, breathy twin-engine fan/compressor whine: narrowband turbulent noise + softly modulated
    detuned twin harmonics."""
    f = np.maximum(freqs, 1.0)
    out = np.zeros(N)
    for hz, level, q_noise in tones:
        for det in (1.0 - twin_detune * 0.5, 1.0 + twin_detune * 0.5):
            fc = periodic(hz * det)
            wander = 0.008 * smooth_turb(rng, 0.08, 0.7, 2) + 0.0025 * smooth_turb(rng, 1.5, 12.0, 2)
            wander -= np.mean(wander)
            phase = 2.0 * np.pi * fc * (t + np.cumsum(wander) / RATE)
            amp = np.clip(1.0 + 0.28 * smooth_turb(rng, 0.3, 4.5, 2), 0.3, 1.8)
            tone = amp * np.sin(phase + rng.uniform(0, 2 * np.pi))
            nb_mag = 1.0 / np.sqrt(1.0 + (q_noise * (f / fc - fc / f)) ** 2)
            breath = colored_noise(rng, nb_mag) * amp
            tone_mix = 0.58 if q_noise >= 25.0 else 0.44
            out += level * (tone_mix * tone + (1.0 - tone_mix) * breath)
    return out


def breathy_howl(rng, cfg):
    """Natural twin-engine intake/duct howl for turbojets (RF-4C J79, Su-25 R-95)."""
    f = np.maximum(freqs, 1.0)

    def formant_env(fr):
        g = np.zeros_like(fr)
        for fc, q in cfg["formants"]:
            g += 1.0 / np.sqrt(1.0 + (q * (fr / fc - fc / fr)) ** 2)
        return g

    out = np.zeros(N)
    detunes = cfg.get("detunes", (0.993, 1.007))
    for det in detunes:
        f0 = periodic(cfg["f0"] * det)
        wobble = 0.010 * smooth_turb(rng, 0.1, 0.8, 2) + 0.003 * smooth_turb(rng, 1.5, 14.0, 2)
        wobble -= np.mean(wobble)
        phase = 2.0 * np.pi * f0 * (t + np.cumsum(wobble) / RATE)
        voiced = np.zeros(N)
        max_k = min(22, int(3500 / f0))
        for k in range(1, max_k + 1):
            w = formant_env(np.array([k * f0]))[0] / (k ** 0.55)
            voiced += w * np.sin(k * phase + rng.uniform(0, 2 * np.pi))
        voiced /= max(np.sqrt(np.mean(voiced ** 2)), 1e-12)
        breath = colored_noise(rng, formant_env(f) * highpass_mag(f, 120.0, 2))
        turb = np.clip(1.0 + 0.22 * smooth_turb(rng, 0.25, 3.0, 2), 0.5, 1.6)
        out += (0.35 * voiced + 0.65 * breath) * turb
    out = np.fft.irfft(np.fft.rfft(out) * lowpass_mag(f, cfg.get("cut", 3200.0), 2), N)
    return cfg["level"] * out / max(np.sqrt(np.mean(out ** 2)), 1e-12)


def tf34_whistle(rng, base_bpf=2720.0, twin_detune=0.0085):
    """Twin TF34-GE-100 28-blade high-bypass fan whistle: crisp, slightly hollow twin-detuned blade-passing
    tone + sub-harmonic + faint upper partials with gentle atmospheric shimmer."""
    f = np.maximum(freqs, 1.0)
    out = np.zeros(N)
    partials = [
        (base_bpf, 1.00, 42.0, 0.72),
        (base_bpf * 0.5, 0.36, 26.0, 0.55),
        (base_bpf * 1.5, 0.16, 36.0, 0.50),
        (base_bpf * 2.0, 0.14, 48.0, 0.60),
    ]
    for hz, level, q_nb, tone_ratio in partials:
        for det in (1.0 - twin_detune * 0.5, 1.0 + twin_detune * 0.5):
            fc = periodic(hz * det)
            wander = 0.0035 * smooth_turb(rng, 0.08, 0.6, 2) + 0.0012 * smooth_turb(rng, 1.5, 10.0, 2)
            wander -= np.mean(wander)
            phase = 2.0 * np.pi * fc * (t + np.cumsum(wander) / RATE)
            amp = np.clip(1.0 + 0.22 * smooth_turb(rng, 0.35, 4.0, 2), 0.45, 1.65)
            tone = amp * np.sin(phase + rng.uniform(0, 2 * np.pi))
            nb_mag = 1.0 / np.sqrt(1.0 + (q_nb * (f / fc - fc / f)) ** 2)
            breath = colored_noise(rng, nb_mag) * amp
            out += level * (tone_ratio * tone + (1.0 - tone_ratio) * breath)
    return out / max(np.sqrt(np.mean(out ** 2)), 1e-12)


def tf34_duct_moan(rng, twin_detune=0.010):
    """Hollow upper fan-duct moan of the A-10's TF34 nacelles (720 Hz & 1480 Hz formants, no low foghorn)."""
    f = np.maximum(freqs, 1.0)

    def duct_env(fr):
        return (
            1.0 / np.sqrt(1.0 + (3.2 * (fr / 720.0 - 720.0 / fr)) ** 2)
            + 0.75 / np.sqrt(1.0 + (3.8 * (fr / 1480.0 - 1480.0 / fr)) ** 2)
        )

    out = np.zeros(N)
    for det in (1.0 - twin_detune * 0.5, 1.0 + twin_detune * 0.5):
        f0 = periodic(112.0 * det)
        wander = 0.005 * smooth_turb(rng, 0.1, 0.7, 2) + 0.0015 * smooth_turb(rng, 1.5, 10.0, 2)
        wander -= np.mean(wander)
        phase = 2.0 * np.pi * f0 * (t + np.cumsum(wander) / RATE)
        buzz = np.zeros(N)
        for k in range(4, 24):
            w = duct_env(np.array([k * f0]))[0] / (k ** 0.35)
            buzz += w * np.sin(k * phase + rng.uniform(0, 2 * np.pi))
        buzz /= max(np.sqrt(np.mean(buzz ** 2)), 1e-12)
        air = colored_noise(rng, duct_env(f) * highpass_mag(f, 350.0, 2))
        turb = np.clip(1.0 + 0.22 * smooth_turb(rng, 0.3, 3.2, 2), 0.5, 1.6)
        out += (0.42 * buzz + 0.58 * air) * turb
    return out / max(np.sqrt(np.mean(out ** 2)), 1e-12)


def bypass_whoosh(rng):
    """High-bypass turbofan cold-stream rush: airy, hollow mid-band whoosh (220-3800 Hz)."""
    f = np.maximum(freqs, 1.0)
    mag = highpass_mag(f, 160.0, 2) * lowpass_mag(f, 4200.0, 3) * (f ** -0.45)
    mag *= 1.0 + 0.7 * np.exp(-0.5 * (np.log2(f / 620.0) / 0.75) ** 2)
    x = colored_noise(rng, mag)
    x *= np.clip(1.0 + 0.20 * smooth_turb(rng, 0.25, 2.5, 2), 0.55, 1.55)
    return x / max(np.sqrt(np.mean(x ** 2)), 1e-12)


def light_core_rumble(rng):
    """Modest warm core exhaust for the A-10's high-bypass turbofans."""
    f = np.maximum(freqs, 1.0)
    mag = highpass_mag(f, 55.0, 2) * lowpass_mag(f, 950.0, 3) * (f ** -0.55)
    mag *= 1.0 + 1.1 * np.exp(-0.5 * (np.log2(f / 195.0) / 0.85) ** 2)
    x = colored_noise(rng, mag)
    x *= np.clip(1.0 + 0.22 * smooth_turb(rng, 0.2, 2.0, 2), 0.55, 1.55)
    return x / max(np.sqrt(np.mean(x ** 2)), 1e-12)


def airframe_rush(rng):
    """Smooth aerodynamic rush heard ahead of a fast-mover as it slices through air."""
    f = np.maximum(freqs, 1.0)
    mag = highpass_mag(f, 180.0, 2) * lowpass_mag(f, 4500.0, 2) * (f ** -0.4)
    x = colored_noise(rng, mag)
    x *= np.clip(1.0 + 0.18 * smooth_turb(rng, 0.3, 3.0, 2), 0.6, 1.5)
    return x / max(np.sqrt(np.mean(x ** 2)), 1e-12)


def turbojet_tear(rng, cfg, variant=0):
    """Continuous tearing/ripping jet exhaust + true high-skewness jet crackle + continuous sub-pressure billow.
    Replaces discrete Poisson Hann bursts & 10 Hz sine thumps so close-in turbojets tear cleanly without
    gunfire/helicopter stuttering."""
    f = np.maximum(freqs, 1.0)
    centre = cfg["tear_centre"] * (1.0, 0.91, 1.09, 0.96)[variant % 4]
    crackle_amt = cfg["crackle"]
    sub_hz = cfg["sub_hz"] * (1.0, 0.94, 1.06, 0.97)[variant % 4]

    hp = highpass_mag(f, 80.0, 3)
    shear_mag = (np.minimum(f / centre, 1.0) ** 0.8) * (np.maximum(f / centre, 1.0) ** -0.45) * hp * lowpass_mag(f, 6500.0, 3)
    shear = colored_noise(rng, shear_mag)
    surge = np.clip(1.0 + 0.30 * smooth_turb(rng, 0.6, 5.5, 2), 0.45, 1.75)
    shear *= surge

    if crackle_amt > 0:
        cr_mag = highpass_mag(f, 320.0, 2) * lowpass_mag(f, 5200.0, 2) * (f ** -0.2)
        cr_raw = colored_noise(rng, cr_mag)
        cr_mod = np.clip(0.85 + 0.45 * smooth_turb(rng, 2.0, 14.0, 2), 0.15, 2.0)
        c = cr_raw * cr_mod
        c_pos = np.maximum(c, 0.0)
        c_neg = np.maximum(-c, 0.0)
        c_skew = (c_pos ** 1.35) * 1.35 - (c_neg ** 0.82) * 0.75
        c_skew -= np.mean(c_skew)
        c_skew = np.fft.irfft(np.fft.rfft(c_skew) * highpass_mag(f, 180.0, 2) * lowpass_mag(f, 6500.0, 3), N)
        c_skew /= max(np.sqrt(np.mean(c_skew ** 2)), 1e-12)
    else:
        c_skew = np.zeros(N)

    sub_mag = np.exp(-0.5 * (np.log2(f / sub_hz) / 0.85) ** 2) * highpass_mag(f, 30.0, 3) * lowpass_mag(f, 260.0, 3)
    sub = colored_noise(rng, sub_mag)
    sub *= np.clip(1.0 + 0.38 * smooth_turb(rng, 0.5, 4.0, 2), 0.35, 1.85)
    sub /= max(np.sqrt(np.mean(sub ** 2)), 1e-12)

    x = 0.65 * shear + crackle_amt * c_skew + cfg["sub_level"] * sub
    x /= max(np.sqrt(np.mean(x ** 2)), 1e-12)
    x *= 0.45
    x = np.where(x > 0, np.tanh(1.5 * x) / 1.5, np.tanh(0.95 * x) / 0.95)
    x -= np.mean(x)
    return x / max(np.sqrt(np.mean(x ** 2)), 1e-12)


def ground_phasing(x, depth=0.30, cut=900.0):
    """Soft outdoor ground echo, low-passed to model terrain absorption and swept once per loop."""
    tau = 0.00525 + 0.00375 * np.sin(2.0 * np.pi * t / SECONDS + 0.7)
    pos = (np.arange(N) - tau * RATE) % N
    i0 = np.floor(pos).astype(int)
    frac = pos - i0
    echo = x[i0] * (1.0 - frac) + x[(i0 + 1) % N] * frac
    echo = lowpass_signal(echo, RATE, cut, 2)
    return x + depth * echo


def layer(spec, cut, is_near):
    rng = np.random.default_rng(spec["seed"] * 10 + (1 if is_near else 2))
    if not is_near:
        x = roar_layer(rng, spec["hump"], cut, slope=1.0)
        x = ground_phasing(x, depth=0.25, cut=450.0)
    elif spec.get("turbofan"):
        x = (
            0.55 * light_core_rumble(rng)
            + 0.75 * bypass_whoosh(rng)
            + 0.48 * tf34_whistle(rng, base_bpf=spec["bpf"])
            + 0.35 * tf34_duct_moan(rng)
        )
        x = ground_phasing(x, depth=0.28, cut=900.0)
    else:
        x = roar_layer(rng, spec["hump"], cut, slope=0.55)
        if spec["whine"]:
            x += 0.35 * twin_fan_whine(rng, spec["whine"])
        if spec.get("howl"):
            x += 0.45 * breathy_howl(rng, spec["howl"])
        x = ground_phasing(x, depth=0.30, cut=850.0)

    swell = 1.0 + 0.14 * np.sin(2.0 * np.pi * periodic(0.13) * t + 0.4) + 0.06 * np.sin(2.0 * np.pi * periodic(0.31) * t + 2.0)
    x *= swell
    for _ in range(4):  # -16 dBFS RMS; peaks rounded off softly
        x *= 10.0 ** (-16.0 / 20.0) / max(np.sqrt(np.mean(x ** 2)), 1e-12)
        x = 0.96 * np.tanh(x / 0.96)
    return x


def scream(spec, variant):
    """Close-in turbojet exhaust loop (0.9-1.2 km passes where the pass-by one-shot does not fire)."""
    rng = np.random.default_rng(spec["seed"] * 100 + 50 + variant)
    x = turbojet_tear(rng, spec["tear"], variant=variant)
    x = ground_phasing(x, depth=0.24, cut=850.0)
    x *= 1.0 + 0.12 * np.sin(2.0 * np.pi * periodic(0.19 + 0.04 * variant) * t + variant)
    x *= 10.0 ** (-16.0 / 20.0) / max(np.sqrt(np.mean(x ** 2)), 1e-12)
    return 0.96 * np.tanh(x / 0.96)


def cubic_interp(src, idx):
    """Catmull-Rom cubic interpolation for cleaner Doppler resampling."""
    n = len(src)
    i1 = np.clip(np.floor(idx).astype(int), 1, n - 3)
    u = idx - i1
    p0 = src[i1 - 1]
    p1 = src[i1]
    p2 = src[i1 + 1]
    p3 = src[i1 + 2]
    return p1 + 0.5 * u * (
        (p2 - p0)
        + u * ((2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3)
               + u * (3.0 * (p1 - p2) + p3 - p0))
    )


def lobe(theta, centre, width):
    return np.exp(-0.5 * ((theta - centre) / width) ** 2)


def flyby(spec, D, seed):
    """24 s pass-by rendered through retarded-time geometry, terrain-filtered ground bounce,
    and true retarded-distance air absorption."""
    v = spec["speed"]
    rng = np.random.default_rng(seed)
    loops = 6
    is_tf = spec.get("turbofan", False)
    prep_cut = 10500.0 if is_tf else 9500.0
    prep = lambda x: np.tile(lowpass_signal(x, RATE, prep_cut, 4), loops)

    if is_tf:
        parts = [
            (prep(tf34_whistle(rng, base_bpf=spec["bpf"])), "whistle"),
            (prep(tf34_duct_moan(rng)), "moan"),
            (prep(bypass_whoosh(rng)), "whoosh"),
            (prep(light_core_rumble(rng)), "core"),
        ]
    else:
        parts = [
            (prep(roar_layer(rng, spec["hump"], 5500.0, 0.55)), "roar"),
            (prep(airframe_rush(rng)), "rush"),
            (prep(turbojet_tear(rng, spec["tear"], variant=int(rng.integers(4)))), "tear"),
        ]
        if spec["whine"] or spec.get("howl"):
            fan = np.zeros(N)
            if spec["whine"]:
                fan += twin_fan_whine(rng, spec["whine"])
            if spec.get("howl"):
                fan += breathy_howl(rng, spec["howl"])
            fan /= max(np.sqrt(np.mean(fan ** 2)), 1e-12)
            parts.append((prep(fan), "fan"))

    n_out = int(FLYBY_SECONDS * FLYBY_RATE)
    tau = np.arange(n_out) / FLYBY_RATE
    hp, hl = D, 1.7
    direct_out = np.zeros(n_out)
    ground_out = np.zeros(n_out)
    r_direct_out = np.zeros(n_out)

    for path in ("direct", "ground"):
        dz = hp - hl if path == "direct" else hp + hl
        te = np.linspace(-45.0, 45.0, 450001)
        r = np.sqrt((v * te) ** 2 + dz ** 2)
        tr = te + r / SOUND_SPEED
        tr0 = (hp - hl) / SOUND_SPEED
        want = tau - FLYBY_PEAK_S + tr0
        te_out = np.interp(want, tr, te)
        r_out = np.sqrt((v * te_out) ** 2 + dz ** 2)
        if path == "direct":
            r_direct_out = r_out
        cos_t = np.clip(-v * te_out / r_out, -1.0, 1.0)
        theta = np.degrees(np.arccos(cos_t))

        idx = (te_out + 50.0) * RATE
        conv = 0.22 if is_tf else 0.25
        gain = (D / r_out) / (1.0 - conv * (v / SOUND_SPEED) * cos_t)

        acc = np.zeros(n_out)
        for src, kind in parts:
            sig = cubic_interp(src, idx)
            if kind == "whistle":
                d = spec["whistle_gain"] * (0.28 + 1.15 * lobe(theta, 48.0, 34.0) + 0.72 * lobe(theta, 128.0, 32.0))
            elif kind == "moan":
                d = spec["moan_gain"] * (0.20 + 0.90 * lobe(theta, 60.0, 36.0) + 0.65 * lobe(theta, 122.0, 32.0))
            elif kind == "whoosh":
                d = spec["whoosh_gain"] * (0.25 + 0.65 * lobe(theta, 95.0, 40.0) + 0.90 * lobe(theta, 132.0, 32.0))
            elif kind == "core":
                d = spec["core_gain"] * (0.25 + 0.95 * lobe(theta, 136.0, 36.0))
            elif kind == "roar":
                d = 0.40 + 0.75 * lobe(theta, 132.0, 38.0)
            elif kind == "rush":
                d = spec["rush_gain"] * (0.25 + 0.85 * lobe(theta, 65.0, 38.0))
            elif kind == "fan":
                d = spec["fan_gain"] * (0.15 + 0.95 * lobe(theta, 42.0, 28.0) + 0.35 * lobe(theta, 125.0, 26.0))
            else:  # tear
                d = spec["tear_gain"] * (0.06 + 1.45 * lobe(theta, 138.0, 24.0))
            acc += sig * d * gain

        if path == "direct":
            direct_out = acc
        else:
            ground_out = acc

    ground_cut = 900.0 if is_tf else 850.0
    ground_refl = 0.28 if is_tf else 0.32
    out = direct_out + ground_refl * lowpass_signal(ground_out, FLYBY_RATE, ground_cut, 2)

    # Air absorption using true retarded emission distance r_direct_out
    frame, hop = 2048, 512
    win = np.hanning(frame)
    ff = np.fft.rfftfreq(frame, 1.0 / FLYBY_RATE)
    if is_tf:
        alpha = 0.0032 * (np.maximum(ff, 1.0) / 1000.0) ** 1.30
    else:
        alpha = 0.0042 * (np.maximum(ff, 1.0) / 1000.0) ** 1.35
    padded = np.concatenate([np.zeros(frame), out, np.zeros(frame)])
    dist = np.concatenate([
        np.full(frame, r_direct_out[0]),
        r_direct_out,
        np.full(frame, r_direct_out[-1]),
    ])
    res = np.zeros_like(padded)
    norm = np.zeros_like(padded)
    for start in range(0, len(padded) - frame, hop):
        seg = padded[start:start + frame] * win
        rr = dist[start + frame // 2]
        spec_ = np.fft.rfft(seg) * 10.0 ** (-alpha * rr / 20.0)
        res[start:start + frame] += np.fft.irfft(spec_, frame) * win
        norm[start:start + frame] += win ** 2
    out = res[frame:frame + n_out] / np.maximum(norm[frame:frame + n_out], 1e-6)

    # Loudest 100 ms at -9 dBFS RMS, peaks rounded off softly
    k = FLYBY_RATE // 10
    short = np.sqrt(np.convolve(out ** 2, np.ones(k) / k, "same"))
    out *= 10.0 ** (-9.0 / 20.0) / max(short.max(), 1e-12)
    out = 0.96 * np.tanh(out / 0.96)
    ramp = int(0.5 * FLYBY_RATE)
    out[:ramp] *= np.linspace(0.0, 1.0, ramp)
    out[-ramp:] *= np.linspace(1.0, 0.0, ramp)
    return out


def resample_loop(x, rate):
    """A periodic loop at RATE to `rate`, by its spectrum: band-limited, and it still closes seamlessly."""
    if rate == RATE:
        return x
    n = int(round(len(x) * rate / RATE))
    spec = np.fft.rfft(x)[:n // 2 + 1]
    return np.fft.irfft(spec, n) * n / len(x)


def write_wav(path, x, rate=RATE):
    pcm = np.clip(np.round(x * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


def curve_gain(curve, d):
    """The sound project's curve as the game plays it: linear segments, Reciprocal ones as 1/distance."""
    for (x0, y0, recip), (x1, y1, _) in zip(curve, curve[1:]):
        if x0 <= d <= x1:
            if recip and y1 == 0:
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
    out *= 10 ** (14 / 20)
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
        base = k * 0x1C00
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
    has_scream = not spec.get("turbofan", False)
    if has_scream:
        loops += [("Scream" + ("_%d" % v if v else ""), scream(spec, v), SCREAM) for v in range(SCREAM_VARIANTS)]
    wavs = []
    for tag, loop, cfg in loops:
        wav = os.path.join(SAMPLES, "ACR_%s_Engine_%s_LP.wav" % (name, tag))
        write_wav(wav, resample_loop(loop, cfg["rate"]), cfg["rate"])
        wavs.append(posix(wav))
        job.append("WAV %s" % posix(wav))
    preview = loops[:3]
    flyby_preview([(x, cfg) for _, x, cfg in preview], os.path.join(PREVIEW, "ACR_%s_flyby.wav" % name))
    for v in range(SCREAM_VARIANTS if has_scream else 1):
        pick = loops[:2] + loops[2 + v:3 + v]
        flyby_preview([(x, cfg) for _, x, cfg in pick],
                      os.path.join(PREVIEW, "ACR_%s_overhead%s.wav" % (name, "_%d" % v if v else "")), closest=300)
    events = [("SOUND_ACR_JET_LP", "Near", [wavs[0]], NEAR), ("SOUND_ACR_JET_FAR_LP", "Far", [wavs[1]], FAR)]
    if has_scream:
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
