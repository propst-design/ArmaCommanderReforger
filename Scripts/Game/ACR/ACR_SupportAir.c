// Air-strike support runs (base run, falling bombs, local flyover, rocket strike); split from ACR_Supports.c.

// A script-driven flight or firing run, updated every server frame.
class ACR_SupportRun : Managed
{
 bool m_bDone;
 bool Frame(ACR_Supports supports, float dt, float now) { return false; }
 void Finish() {}
}

// One bomb of a flyover bomb run, on this machine only: hung on the jet's pylon from the start, let go
// so that it lands where and when the server's blast goes off (ACR_Supports.StartBombRun sends each
// impact), nose pitching down along its path. Purely visual: the blast and its damage stay the server's
// synchronized virtual explosion. A low-drag bomb falls under the jet. A high-drag one slows hard, drops
// steeply and lands well behind the jet, which is clear by then: right for a level drop from 150-300 m (Jacob,
// 4 Nov, "make them high drag bombs which would make sense at low alt"): the A-10's Mk 82 Snakeye, whose tail
// opens into four airbrake petals half a second out, and the Su-25's OFAB-250ShN, whose brake chute streams
// out of its tail housing (Jacob's pick for the Soviet side). The drag is modelled, not only shown: Simulate
// integrates the fall with it, the bomb follows that path, and the server times the blast by the same fall
// (Lag), so the blast goes off where and when the client's bomb lands. The blast's damage is unchanged.
class ACR_FallingBomb : Managed
{
	static const float GRAVITY = 9.81;
	static const float EJECT = 1.5; // m/s down off the ejector rack
	static const float SNAKEYE_K = 0.0025; // drag/mass (1/m) with the petals open: halves 200 m/s in 2 s, falls at ~63 m/s
	static const float CHUTE_K = 0.0048;   // the OFAB-250ShN's chute: halves 200 m/s in 1 s, falls at ~45 m/s
	static const float SNAKEYE_OPEN_S = 0.5; // s from release until the petals open...
	static const float CHUTE_OPEN_S = 0.8;   // ...until the chute is out
	static const float SAMPLE_DT = 0.05;
	static const float PYLON_DROP = 1;     // the pylons hang about this far under the jet's origin
	static const ResourceName MK82_OPEN = "{AC230926A00100E6}Prefabs/Vehicles/Aircraft/ACR_Mk82_Open.et";
	static const ResourceName OFAB_OPEN = "{AC230926A00100E8}Prefabs/Vehicles/Aircraft/ACR_OFAB250ShN_Open.et";
	// The Snakeye's blades swinging out from their hinges, 15, 30 and 45 degrees (fully open at 55: MK82_OPEN),
	// one STAGE_S apart: a quarter-second opening, not a sudden change of model.
	static const ResourceName MK82_OPEN_15 = "{AC230926A00100E9}Prefabs/Vehicles/Aircraft/ACR_Mk82_Open15.et";
	static const ResourceName MK82_OPEN_30 = "{AC230926A00100EA}Prefabs/Vehicles/Aircraft/ACR_Mk82_Open30.et";
	static const ResourceName MK82_OPEN_45 = "{AC230926A00100EB}Prefabs/Vehicles/Aircraft/ACR_Mk82_Open45.et";
	static const float STAGE_S = 0.06;
	IEntity m_Body;
	vector m_vSlot;        // where it hangs, in the jet's model axes
	bool m_bHung;          // on a pylon (false: it appears under the jet's belly at release)
	vector m_vImpact;
	float m_fImpactAt;     // flyover seconds when it lands
	float m_fReleaseAt;
	bool m_bFalling;
	bool m_bDone;
	vector m_vFrom;
	float m_fFrom;
	float m_fFall;
	ResourceName m_sPrefab;
	float m_fDrag;          // SNAKEYE_K, CHUTE_K, or 0 for a low-drag bomb
	float m_fOpenAfter;
	int m_iStage;           // how many of its opening stages are showing (0: shut)
	ref array<float> m_aAlong; // the simulated fall every SAMPLE_DT: metres forward...
	ref array<float> m_aDown;  // ...and metres down
	float m_fSimT;
	float m_fSimD;
	float m_fSimH;

	// Drag once the retarder opens (0: none), when it opens, and the model with it open.
	static float DragOf(ResourceName bomb)
	{
		if (bomb == ACR_LocalFlyover.BOMB_MK82) return SNAKEYE_K;
		if (bomb == ACR_LocalFlyover.BOMB_OFAB) return CHUTE_K;
		return 0;
	}
	static float OpenAfterOf(ResourceName bomb)
	{
		if (bomb == ACR_LocalFlyover.BOMB_OFAB) return CHUTE_OPEN_S;
		return SNAKEYE_OPEN_S;
	}
	static ResourceName OpenedOf(ResourceName bomb)
	{
		if (bomb == ACR_LocalFlyover.BOMB_MK82) return MK82_OPEN;
		if (bomb == ACR_LocalFlyover.BOMB_OFAB) return OFAB_OPEN;
		return "";
	}
	// The models it goes through as its retarder opens, the last fully open.
	static int StageCount(ResourceName bomb)
	{
		if (bomb == ACR_LocalFlyover.BOMB_MK82) return 4;
		if (OpenedOf(bomb) != "") return 1;
		return 0;
	}
	static ResourceName StageOf(ResourceName bomb, int stage)
	{
		if (bomb == ACR_LocalFlyover.BOMB_MK82)
		{
			if (stage == 0) return MK82_OPEN_15;
			if (stage == 1) return MK82_OPEN_30;
			if (stage == 2) return MK82_OPEN_45;
		}
		return OpenedOf(bomb);
	}

	// A bomb let go at `v0` m/s level, `h` m up, with drag `k` from `openAfter` s on: samples of the fall
	// (along, down) every SAMPLE_DT, its time `T` and its distance forward `D`.
	static void Simulate(float h, float v0, float k, float openAfter, notnull array<float> along, notnull array<float> down, out float T, out float D)
	{
		along.Clear();
		down.Clear();
		float x = 0;
		float y = 0;
		float vx = v0;
		float vy = EJECT;
		float t = 0;
		float dt = 0.01;
		int step = 0;
		along.Insert(0);
		down.Insert(0);
		while (y < h && t < 60)
		{
			float kk = 0;
			if (t >= openAfter) kk = k;
			float speed = Math.Sqrt(vx * vx + vy * vy);
			vx -= kk * speed * vx * dt;
			vy += (GRAVITY - kk * speed * vy) * dt;
			x += vx * dt;
			y += vy * dt;
			t += dt;
			step++;
			if (step % 5 == 0)
			{
				along.Insert(x);
				down.Insert(Math.Min(y, h));
			}
		}
		T = t;
		D = x;
		along.Insert(x);
		down.Insert(h);
	}

	// How much later than the jet passes over it a bomb dropped `h` m up at `v0` m/s lands: 0 for a
	// low-drag bomb (it falls under the jet), seconds for a Snakeye.
	static float Lag(ResourceName bomb, float h, float v0)
	{
		float k = DragOf(bomb);
		if (k <= 0) return 0;
		array<float> along = {};
		array<float> down = {};
		float T, D;
		Simulate(Math.Max(1, h), v0, k, OpenAfterOf(bomb), along, down, T, D);
		return T - D / v0;
	}

	// Places the body with its nose (model +X) along `nose` and its fins' cross axis (model Z) level,
	// the same way round as a jet flying along `flat` (ACR_LocalFlyover.Orient).
	static void Pose(IEntity body, vector at, vector nose, vector flat)
	{
		vector up = "0 1 0";
		vector side = ACR_LocalFlyover.Cross(up, flat) * -1;
		side = side - nose * vector.Dot(side, nose);
		side.Normalize();
		// level flight: (flat, up, side) is the jet's frame; keep the same handedness when pitched
		vector top = ACR_LocalFlyover.Cross(side, nose);
		vector levelTop = ACR_LocalFlyover.Cross(ACR_LocalFlyover.Cross(up, flat) * -1, flat);
		if (vector.Dot(levelTop, up) < 0) top = top * -1;
		vector m[4];
		m[0] = nose;
		m[1] = top;
		m[2] = side;
		m[3] = at;
		body.SetTransform(m);
		body.Update();
	}
}

// Bomb-run aircraft model crossing the target, flown by every machine with a player on its
// own non-replicated copy (a server-moved entity doesn't replicate its movement: clients
// saw it frozen where it spawned). The payload stays AC:A's synchronized virtual blasts.
// Jacob, 9 Oct: an audible jet over the target, "not too low". RHS's FA-18 model carries
// its own engine sound (a loop that starts within 4 km of the listener, heard to 1.5 km).
// ACA's own jets (Prefabs/Vehicles/Aircraft) carry a SoundComponent with two engine loops
// (tools/audio/synth_jets.py): ENGINE_EVENT, the roar within ~2.5 km, and ENGINE_FAR_EVENT, a
// dull rumble out to 6 km, crossfaded by distance. Jacob, 4 Nov: the FA-18 is too loud, cuts in
// abruptly and out too soon. So each starts as the listener comes into its range (the far one
// at SOUND_START_M, the near one at NEAR_START_M: a layer started out of its range is
// dropped as inaudible), where its curve is near silent, with a 6 s fade-in on top, and the plane is kept until the listener is SOUND_KEEP_M
// away, where it reaches silence. The component follows the moving plane (dynamic simulation).
class ACR_LocalFlyover : Managed
{
	static const string ENGINE_EVENT = "SOUND_ACR_JET_LP";
	static const string ENGINE_FAR_EVENT = "SOUND_ACR_JET_FAR_LP";
	// The turbojets' close-in scream (RF-4C, Su-17M4R, Su-25; the A-10's turbofans have none), heard within
	// 1.2 km: Jacob, 4 Nov, "the non-turbofan jets need a better scream when they're overhead".
	static const string SCREAM_EVENT = "SOUND_ACR_JET_SCREAM_LP";
	static const float SCREAM_START_M = 1150;
	// The pass-by (tools/audio/synth_jets.py flyby()): a 24 s fly-past rendered through the real geometry, with
	// the Doppler fall of the pitch and the roar slamming in from behind just after the jet has gone over
	// (Jacob, 4 Nov: the A-10 "missing its signature noise", the turbojets "missing that Michael Bay oomph",
	// which no loop can do). Started so its closest moment, FLYBY_PEAK_S in, lands when this machine's listener
	// hears the jet nearest; the near and scream loops are left off (or let go) under it. Rendered at 250 m
	// (NEAR, used under FLYBY_NEAR_M) and 600 m (MID, out to FLYBY_MAX_M); farther passes keep the loops.
	static const string FLYBY_NEAR_EVENT = "SOUND_ACR_JET_FLYBY_NEAR";
	static const string FLYBY_MID_EVENT = "SOUND_ACR_JET_FLYBY_MID";
	static const float FLYBY_PEAK_S = 12;
	static const float FLYBY_NEAR_M = 420;
	static const float FLYBY_MAX_M = 900;
	static const float SOUND_START_M = 5800; // the acp's far curve is silent at 6000 m
	static const float NEAR_START_M = 2400;  // ...and its near curve at 2500 m
	static const float SOUND_KEEP_M = 6000;
	static const float FA18_KEEP_M = 1800;   // RHS's loop is heard to 1.5 km
	static const float MAX_TAIL_S = 60;
	static const float SPEED_OF_SOUND = 343;      // past the route's end, whoever is listening
	protected static ref array<ref ACR_LocalFlyover> s_All;
	// Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
	static void ACR_InitStatics()
	{
		s_All = {};
	}
	static int s_iStarted; // tests
	IEntity m_Plane;
	vector m_vStart;
	vector m_vDir;
	float m_fSpeed;
	float m_fAltitude;
	float m_fStartedAt;
	float m_fSeconds;
	vector m_vAxes[3];
	// AC: Actual's period jets on (the server's -acrPeriodJets, ACR_Aircraft): the engine loops, pass-bys,
	// hung and falling bombs below. Off, the flyover is as before them: a model moved along the track.
	bool m_bPeriodJets;
	SoundComponent m_Sound;
	bool m_bSounding;
	bool m_bSoundingNear;
	bool m_bDemoOverhead; // tests
	AudioHandle m_hEngine = AudioHandle.Invalid; // tests
	AudioHandle m_hEngineFar = AudioHandle.Invalid;
	AudioHandle m_hScream = AudioHandle.Invalid; // tests
	bool m_bScreams;
	bool m_bScreaming;
	bool m_bFlybys;
	bool m_bFlybyFired;
	AudioHandle m_hFlyby = AudioHandle.Invalid; // tests
	float m_fFlybyMiss; // tests
	ResourceName m_sBomb;
	ref array<vector> m_aHardpoints;
	ref array<ref ACR_FallingBomb> m_aBombs;
	int m_iLanded; // tests
	// Night-time nozzle glows (ACR_JetGlow; none on the A-10), and the recon jets' afterburner over the corridor.
	ref ACR_JetGlow m_Glow;
	bool m_bReconDash;
	static const float ATTACK_LEAD_S = 6;  // the glow is up a little from this long before a bomb is let go...
	static const float ATTACK_TAIL_S = 2;  // ...to this long after

	static const ResourceName BOMB_MK82 = "{AC230926A00100E4}Prefabs/Vehicles/Aircraft/ACR_Mk82.et";
	static const ResourceName BOMB_FAB250 = "{AC230926A00100E5}Prefabs/Vehicles/Aircraft/ACR_FAB250.et";
	static const ResourceName BOMB_OFAB = "{AC230926A00100E7}Prefabs/Vehicles/Aircraft/ACR_OFAB250ShN.et";

	// The bombs a jet drops and the pylons they hang on, in its model axes (x forward, y up, z across), in
	// release order: an inner pair, then the outer. ACA's bombers carry four (the bomb run's default count;
	// more appear under the belly as they are let go). Other prefabs (RHS's FA-18) have none hung.
	static ResourceName Hardpoints(ResourceName prefab, notnull array<vector> slots)
	{
		slots.Clear();
		if (prefab == ACR_Supports.BOMB_JET_WEST)
		{
			// A-10A: Mk 82s under the wing, 2.3 and 3.8 m out (tools/blender/build_aircraft.py)
			slots.Insert(Vector(0.9, -1.07, 2.3));
			slots.Insert(Vector(0.9, -1.07, -2.3));
			slots.Insert(Vector(0.9, -1.07, 3.8));
			slots.Insert(Vector(0.9, -1.07, -3.8));
			return BOMB_MK82;
		}
		if (prefab == ACR_Supports.BOMB_JET_EAST)
		{
			// Su-25: OFAB-250ShN parachute-retarded bombs, 3.0 and 4.4 m out, lugs (31 and 44 % back from the
			// nose) under the pylon
			slots.Insert(Vector(0.2, -0.29, 3.0));
			slots.Insert(Vector(0.2, -0.29, -3.0));
			slots.Insert(Vector(0.2, -0.29, 4.4));
			slots.Insert(Vector(0.2, -0.29, -4.4));
			return BOMB_OFAB;
		}
		return BOMB_MK82;
	}

	// The latest flyover started at `start` (each bomb run's jet is told apart by where it set off).
	static ACR_LocalFlyover Find(vector start)
	{
		for (int i = s_All.Count() - 1; i >= 0; i--)
			if (vector.Distance(s_All[i].m_vStart, start) < 5) return s_All[i];
		return null;
	}

	// A bomb landing at `impact`, `impactAt` seconds into the flight: hung on the next free pylon now, let
	// go when a fall from the jet's height takes it there on time.
	void AddBomb(vector impact, float impactAt)
	{
		if (!m_bPeriodJets) return;
		ACR_FallingBomb bomb = new ACR_FallingBomb();
		bomb.m_vImpact = impact;
		bomb.m_vImpact[1] = GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]);
		bomb.m_fImpactAt = impactAt;
		if (m_aBombs.Count() < m_aHardpoints.Count())
		{
			bomb.m_vSlot = m_aHardpoints[m_aBombs.Count()];
			bomb.m_bHung = true;
			bomb.m_Body = ACR_Supports.SpawnLocal(m_sBomb, m_vStart, vector.Zero);
		}
		// the same fall the server timed the blast by (ACR_FallingBomb.Lag in StartBombRun)
		bomb.m_sPrefab = m_sBomb;
		bomb.m_fDrag = ACR_FallingBomb.DragOf(m_sBomb);
		bomb.m_fOpenAfter = ACR_FallingBomb.OpenAfterOf(m_sBomb);
		bomb.m_aAlong = {};
		bomb.m_aDown = {};
		float h = Math.Max(1, m_fAltitude - ACR_FallingBomb.PYLON_DROP - bomb.m_vImpact[1]);
		ACR_FallingBomb.Simulate(h, m_fSpeed, bomb.m_fDrag, bomb.m_fOpenAfter, bomb.m_aAlong, bomb.m_aDown, bomb.m_fSimT, bomb.m_fSimD);
		bomb.m_fSimH = h;
		bomb.m_fReleaseAt = impactAt - bomb.m_fSimT;
		m_aBombs.Insert(bomb);
		if (bomb.m_Body && m_Plane)
		{
			// on its pylon from the first frame
			vector transform[4];
			m_Plane.GetTransform(transform);
			StepBombs(transform, (GetGame().GetWorld().GetWorldTime() - m_fStartedAt) / 1000);
		}
	}

	// Hung bombs ride their pylons; released ones fall on a parabola that ends at their impact on time.
	protected void StepBombs(vector transform[4], float elapsed)
	{
		vector flat = m_vDir;
		flat[1] = 0;
		flat.Normalize();
		foreach (ACR_FallingBomb bomb : m_aBombs)
		{
			if (bomb.m_bDone) continue;
			vector pylon = transform[3] + transform[0] * bomb.m_vSlot[0] + transform[1] * bomb.m_vSlot[1] + transform[2] * bomb.m_vSlot[2];
			if (!bomb.m_bFalling && elapsed >= bomb.m_fReleaseAt)
			{
				bomb.m_bFalling = true;
				bomb.m_vFrom = pylon;
				if (!bomb.m_bHung) bomb.m_vFrom = transform[3] - "0 2 0";
				bomb.m_fFrom = elapsed;
				// word from the server that came late still lands it on time, falling faster
				bomb.m_fFall = Math.Max(0.3, bomb.m_fImpactAt - elapsed);
				if (!bomb.m_Body) bomb.m_Body = ACR_Supports.SpawnLocal(m_sBomb, bomb.m_vFrom, vector.Zero);
			}
			if (!bomb.m_Body) continue;
			if (!bomb.m_bFalling)
			{
				vector hung[4];
				hung[0] = transform[0];
				hung[1] = transform[1];
				hung[2] = transform[2];
				hung[3] = pylon;
				bomb.m_Body.SetTransform(hung);
				bomb.m_Body.Update();
				continue;
			}
			float t = elapsed - bomb.m_fFrom;
			if (t >= bomb.m_fFall)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(bomb.m_Body); // the blast goes off here now
				bomb.m_Body = null;
				bomb.m_bDone = true;
				m_iLanded++;
				continue;
			}
			// the simulated fall, stretched onto the real line from the pylon to the impact
			float s = t * bomb.m_fSimT / bomb.m_fFall;
			int last = bomb.m_aAlong.Count() - 2;
			int i = (int)Math.Floor(s / ACR_FallingBomb.SAMPLE_DT);
			if (i > last) i = last;
			float frac = Math.Clamp(s / ACR_FallingBomb.SAMPLE_DT - i, 0, 1);
			float along = bomb.m_aAlong[i] + (bomb.m_aAlong[i + 1] - bomb.m_aAlong[i]) * frac;
			float down = bomb.m_aDown[i] + (bomb.m_aDown[i + 1] - bomb.m_aDown[i]) * frac;
			vector across = bomb.m_vImpact - bomb.m_vFrom;
			across[1] = 0;
			float scaleX = across.Length() / Math.Max(1, bomb.m_fSimD);
			float scaleY = (bomb.m_vFrom[1] - bomb.m_vImpact[1]) / bomb.m_fSimH;
			across.Normalize();
			vector at = bomb.m_vFrom + across * (along * scaleX);
			at[1] = bomb.m_vFrom[1] - down * scaleY;
			vector velocity = across * ((bomb.m_aAlong[i + 1] - bomb.m_aAlong[i]) * scaleX);
			velocity[1] = -(bomb.m_aDown[i + 1] - bomb.m_aDown[i]) * scaleY;
			// the retarder opens (the Snakeye's blades swinging out through their stages, or the chute streaming
			// behind): each stage's model in turn, which the bomb's pose carries along its path and which goes
			// with the bomb when it lands
			int stages = ACR_FallingBomb.StageCount(bomb.m_sPrefab);
			int due = 0;
			if (bomb.m_fDrag > 0 && s >= bomb.m_fOpenAfter)
				due = 1 + (int)Math.Floor((s - bomb.m_fOpenAfter) / ACR_FallingBomb.STAGE_S);
			if (due > stages) due = stages;
			if (due > bomb.m_iStage)
			{
				bomb.m_iStage = due;
				IEntity opened = ACR_Supports.SpawnLocal(ACR_FallingBomb.StageOf(bomb.m_sPrefab, due - 1), at, vector.Zero);
				if (opened)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(bomb.m_Body);
					bomb.m_Body = opened;
				}
			}
			if (velocity.Length() > 0.01) ACR_FallingBomb.Pose(bomb.m_Body, at, velocity.Normalized(), flat);
		}
	}

	protected void ClearBombs()
	{
		foreach (ACR_FallingBomb bomb : m_aBombs)
			if (bomb.m_Body) SCR_EntityHelper.DeleteEntityAndChildren(bomb.m_Body);
		m_aBombs.Clear();
	}

	static void Start(ResourceName prefab, vector start, vector dir, float speed, float altitude, float seconds, bool periodJets = false)
	{
		IEntity plane = ACR_Supports.SpawnLocal(prefab, start, vector.Zero);
		if (!plane)
		{
			PrintFormat("[ACR] FLYOVER local spawn failed prefab=%1 at=%2", prefab, start);
			return;
		}
		ACR_LocalFlyover flyover = new ACR_LocalFlyover();
		flyover.m_Plane = plane;
		flyover.m_vStart = start;
		flyover.m_vDir = dir;
		flyover.m_fSpeed = speed;
		flyover.m_fAltitude = altitude;
		flyover.m_fSeconds = seconds;
		flyover.m_fStartedAt = GetGame().GetWorld().GetWorldTime();
		flyover.m_aHardpoints = {};
		flyover.m_aBombs = {};
		flyover.m_bPeriodJets = periodJets;
		if (periodJets)
		{
			flyover.m_Sound = SoundComponent.Cast(plane.FindComponent(SoundComponent));
			flyover.m_bScreams = prefab == ACR_ReconRun.RECON_JET_WEST || prefab == ACR_ReconRun.RECON_JET_EAST || prefab == ACR_Supports.BOMB_JET_EAST;
			flyover.m_bFlybys = flyover.m_bScreams || prefab == ACR_Supports.BOMB_JET_WEST;
			flyover.m_sBomb = Hardpoints(prefab, flyover.m_aHardpoints);
			flyover.m_Glow = ACR_JetGlow.Create(prefab);
			flyover.m_bReconDash = prefab == ACR_ReconRun.RECON_JET_WEST || prefab == ACR_ReconRun.RECON_JET_EAST;
		}
		flyover.Orient();
		s_All.Insert(flyover);
		s_iStarted++;
		GetGame().GetCallqueue().CallLater(flyover.Step, 0, true);
		flyover.Step();
	}

	// Model axes -> world axes. Up is the model's +Y (it stands on its wheels); the nose is
	// the model's root bone forward (+Z of V_F18: the engines sit at its -Z), flattened.
	// The FA-18 mesh's nose lies along the model's X axis, not Z.
	protected void Orient()
	{
		vector upModel = "0 1 0";
		vector noseModel = "1 0 0";
		Animation animation = m_Plane.GetAnimation();
		if (animation)
		{
			TNodeId bone = animation.GetBoneIndex("V_F18");
			if (bone >= 0)
			{
				vector boneMatrix[4];
				animation.GetBoneMatrix(bone, boneMatrix);
				vector nose = boneMatrix[2];
				nose[1] = 0;
				if (nose.Length() > 0.1) noseModel = nose.Normalized();
			}
		}
		PrintFormat("[ACR] FLYOVER %1 nose along model %2", FilePath.StripPath(m_Plane.GetPrefabData().GetPrefabName()), noseModel);
		vector rightModel = Cross(upModel, noseModel);
		vector noseWorld = m_vDir;
		noseWorld[1] = 0;
		noseWorld.Normalize();
		vector upWorld = "0 1 0";
		vector rightWorld = Cross(upWorld, noseWorld);
		for (int i = 0; i < 3; i++)
			m_vAxes[i] = rightWorld * rightModel[i] + upWorld * upModel[i] + noseWorld * noseModel[i];
	}

	// Where the plane was when the sound reaching `listener` now left it: the retarded time
	// t' with |P(t') - listener| = 343 (t - t'), by a few fixed-point steps.
	vector Emitted(vector listener, float elapsed)
	{
		float t = elapsed;
		vector at = m_vStart + m_vDir * (m_fSpeed * t);
		for (int i = 0; i < 4; i++)
		{
			at = m_vStart + m_vDir * (m_fSpeed * t);
			at[1] = m_fAltitude;
			t = elapsed - vector.Distance(at, listener) / SPEED_OF_SOUND;
		}
		return at;
	}

	// Where this machine's player is listening: the Game Master camera while the editor is open (the
	// game hears from that camera, wherever the GM's soldier stands; Jacob, 6 Nov), else the controlled
	// character, else the camera (a commander with no body). False when there is none.
	static bool Listener(out vector at)
	{
		SCR_EditorManagerEntity editor = SCR_EditorManagerEntity.GetInstance();
		if (editor && editor.IsOpened())
		{
			SCR_ManualCamera gmCamera = SCR_CameraEditorComponent.GetCameraInstance();
			if (gmCamera)
			{
				at = gmCamera.GetOrigin();
				return true;
			}
		}
		PlayerController controller = GetGame().GetPlayerController();
		if (controller && controller.GetControlledEntity())
		{
			at = controller.GetControlledEntity().GetOrigin();
			return true;
		}
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && cameras.CurrentCamera())
		{
			at = cameras.CurrentCamera().GetOrigin();
			return true;
		}
		return false;
	}

	// Where this flight passes nearest `listener`: if within FLYBY_MAX_M, starts the pass-by when its closest
	// moment will land on time (the jet's nearest point heard after its sound has crossed the gap). True while
	// one is still to come, so the close-range loops stay off; a pass-by that would have to start more than 2 s
	// late (a flight begun close by) is skipped and the loops play instead.
	protected bool Flyby(vector listener, float elapsed)
	{
		vector flat = m_vDir;
		flat[1] = 0;
		flat.Normalize();
		float nearestAt = vector.Dot(listener - m_vStart, flat) / m_fSpeed;
		vector nearest = m_vStart + flat * (m_fSpeed * nearestAt);
		nearest[1] = m_fAltitude;
		float miss = vector.Distance(listener, nearest);
		if (miss > FLYBY_MAX_M) return false;
		float startAt = nearestAt + miss / SPEED_OF_SOUND - FLYBY_PEAK_S;
		if (elapsed > startAt + 2) return false;
		if (elapsed < startAt) return true;
		string eventName = FLYBY_MID_EVENT;
		if (miss < FLYBY_NEAR_M) eventName = FLYBY_NEAR_EVENT;
		m_hFlyby = m_Sound.SoundEvent(eventName);
		m_bFlybyFired = true;
		m_fFlybyMiss = miss;
		// the loops (if a listener's move started them) fade out under it (their banks' 4 s termination fade)
		if (m_bSoundingNear) m_Sound.Terminate(m_hEngine);
		if (m_bScreaming) m_Sound.Terminate(m_hScream);
		return false;
	}

	// Where this machine's player looks from (the nozzle glows): the current camera, else where they listen.
	static bool Eye(out vector at)
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && cameras.CurrentCamera())
		{
			at = cameras.CurrentCamera().GetOrigin();
			return true;
		}
		return Listener(at);
	}

	// 0 to 1: the bomber on its attack run, from ATTACK_LEAD_S before a bomb is let go to ATTACK_TAIL_S after.
	float AttackRun(float elapsed)
	{
		float run = 0;
		foreach (ACR_FallingBomb bomb : m_aBombs)
			run = Math.Max(run, Math.Clamp(Math.Min(elapsed - bomb.m_fReleaseAt + ATTACK_LEAD_S, bomb.m_fReleaseAt + ATTACK_TAIL_S - elapsed), 0, 1));
		return run;
	}

	static ACR_LocalFlyover Latest()
	{
		if (s_All.IsEmpty()) return null;
		return s_All[s_All.Count() - 1];
	}

	static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}

	void Step()
	{
		float elapsed = (GetGame().GetWorld().GetWorldTime() - m_fStartedAt) / 1000;
		if (!m_Plane)
		{
			GetGame().GetCallqueue().Remove(Step);
			ClearBombs();
			if (m_Glow) m_Glow.Clear();
			m_Plane = null;
			s_All.RemoveItem(this);
			return;
		}
		vector position = m_vStart + m_vDir * (m_fSpeed * elapsed);
		position[1] = m_fAltitude;
		vector transform[4];
		transform[0] = m_vAxes[0];
		transform[1] = m_vAxes[1];
		transform[2] = m_vAxes[2];
		transform[3] = position;
		m_Plane.SetTransform(transform);
		if (!m_bPeriodJets)
		{
			// As before AC: Actual's period jets. The configured route ends 2.5 km beyond the target, but
			// listeners farther downrange can still be inside RHS's 1.5 km sound range. Let the plane leave
			// that range locally before deleting it, so entity cleanup does not cut the engine loop off mid-tail.
			if (elapsed <= m_fSeconds) return;
			PlayerController controller = GetGame().GetPlayerController();
			IEntity hearer = null;
			if (controller) hearer = controller.GetControlledEntity();
			if (hearer && vector.DistanceXZ(hearer.GetOrigin(), position) <= 1800) return;
			GetGame().GetCallqueue().Remove(Step);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Plane);
			m_Plane = null;
			s_All.RemoveItem(this);
			return;
		}
		// moves its bounds in the scene too: without it the jet was culled as if still where it
		// spawned, 5 km off, and never drawn overhead (Jacob, 4 Nov), and its sound sat astray
		m_Plane.Update();
		StepBombs(transform, elapsed);
		if (m_Glow)
		{
			vector eye;
			bool seeing = Eye(eye);
			float burner = 0;
			if (m_bReconDash) burner = ACR_JetGlow.Dash(elapsed);
			m_Glow.Step(transform, seeing, eye, Math.Max(burner, AttackRun(elapsed)), burner);
		}
		vector listener;
		bool heard = Listener(listener);
		// The engine is heard where the plane was when that sound left it (sound covers 343 m/s,
		// the jet 200): behind the plane, swinging round smoothly and loudest just after it has
		// passed. Placed on the plane itself it flipped to the far side the instant it crossed
		// overhead (Jacob, 4 Nov). Set every frame on both layers.
		if (m_Sound)
		{
			vector heardAt[4];
			heardAt[0] = transform[0];
			heardAt[1] = transform[1];
			heardAt[2] = transform[2];
			heardAt[3] = position;
			if (heard) heardAt[3] = Emitted(listener, elapsed);
			m_Sound.SetTransformation(heardAt);
			if (m_bSounding) m_Sound.SetSoundTransformation(m_hEngineFar, heardAt);
			if (m_bSoundingNear) m_Sound.SetSoundTransformation(m_hEngine, heardAt);
			if (m_bScreaming) m_Sound.SetSoundTransformation(m_hScream, heardAt);
			if (m_bFlybyFired) m_Sound.SetSoundTransformation(m_hFlyby, heardAt);
		}
		float distance = vector.Distance(listener, position);
		bool flybyComing = m_Sound && m_bFlybys && heard && !m_bFlybyFired && Flyby(listener, elapsed);
		if (m_Sound && !m_bSounding && (!heard || distance <= SOUND_START_M))
		{
			m_Sound.EnableDynamicSimulation(true);
			m_hEngineFar = m_Sound.SoundEvent(ENGINE_FAR_EVENT);
			m_bSounding = true;
		}
		if (m_Sound && !m_bSoundingNear && !flybyComing && !m_bFlybyFired && (!heard || distance <= NEAR_START_M))
		{
			m_hEngine = m_Sound.SoundEvent(ENGINE_EVENT);
			m_bSoundingNear = true;
		}
		if (m_Sound && m_bScreams && !m_bScreaming && !flybyComing && !m_bFlybyFired && (!heard || distance <= SCREAM_START_M))
		{
			m_hScream = m_Sound.SoundEvent(SCREAM_EVENT);
			m_bScreaming = true;
		}
		// The configured route ends 2.5 km beyond the target, but listeners farther downrange
		// can still hear it. Let the plane leave the sound's range locally before deleting it,
		// so entity cleanup does not cut the engine loop off mid-tail.
		if (elapsed <= m_fSeconds) return;
		float keep = FA18_KEEP_M;
		if (m_Sound) keep = SOUND_KEEP_M;
		if (heard && distance <= keep && elapsed <= m_fSeconds + MAX_TAIL_S) return;
		GetGame().GetCallqueue().Remove(Step);
		ClearBombs();
		if (m_Glow) m_Glow.Clear();
		SCR_EntityHelper.DeleteEntityAndChildren(m_Plane);
		m_Plane = null;
		s_All.RemoveItem(this);
	}
}

// Rockets fired from a point moving through the sky toward the target.
class ACR_RocketStrikeRun : ACR_SupportRun
{
 vector m_Target;
 vector m_Dir;
 vector m_Pos;
 ResourceName m_Rocket;
 int m_iLeft = 16;
 float m_fNext;

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  m_Pos = m_Pos + m_Dir * (110 * dt);
  float along = vector.Dot(m_Target - m_Pos, m_Dir);
  if (along < 950 && along > 420 && m_iLeft > 0 && now >= m_fNext)
  {
   vector right = Vector(m_Dir[2], 0, -m_Dir[0]);
   vector aim = m_Target + m_Dir * Math.RandomFloat(-40, 60) + right * Math.RandomFloat(-18, 18);
   aim[1] = GetGame().GetWorld().GetSurfaceY(aim[0], aim[2]) + 1;
   supports.FireProjectile(m_Rocket, m_Pos + right * Math.RandomFloat(-4, 4), aim);
   m_iLeft--;
   if (m_iLeft % 4 == 0) PrintFormat("[ACR] SUPPORT strike rockets left %1 from %2", m_iLeft, m_Pos);
   m_fNext = now + 110;
  }
  return along > 250;
 }
}
