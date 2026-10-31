// Off-map supports the commander buys with RP and calls on the map (server side).
//  - Mortar barrages (HE, creeping, smoke) and illumination are fired by a real mortar
//    team (ACR_Mortars). The Game Master barrage modules below (spawned at the target,
//    they fire and clean up) are kept for the coming howitzer battery.
//  - Rocket strike and the default bomb run are "virtual aircraft": rockets launched
//    from a moving point in the sky / large explosions along a line. Bomb runs can
//    optionally add a flyover prefab or hand a configured prefab to the editable-entity
//    placement hook used by RHS and other scripted call-ins.
//  - Helicopter rocket run / gunship loiter use a real crewed gunship that the script
//    flies along a path (the AI cannot fly). Its door gunners fight on their own; it
//    fires rocket salvos itself. It can be shot down. The door-gunner support is the
//    transport version (no rockets) orbiting longer for less.
enum ACR_ESupportType
{
 MORTAR_HE_SMALL,
 MORTAR_HE_LARGE,
 MORTAR_CREEPING,
 MORTAR_SMOKE,
 ILLUMINATION,
 ROCKET_STRIKE,
 BOMB_RUN,
 HELI_GUN_RUN,   // "rocket run" (the enum name stays: saved settings use it)
 HELI_LOITER,
 HELI_DOOR_GUNS, // transport helicopter, door gunners only, orbiting
 ARTILLERY_BATTERY, // battalion howitzers: no range limit, three heavy salvos over a wide area
 AIR_LIFT          // off-map transport helicopter flies a squad to a landing zone (ACR_AirLift)
}

enum ACR_EBombRunDelivery
{
	VIRTUAL,
	FLYOVER,
	EDITABLE_CALL_IN
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_SupportEntry
{
 [Attribute("0", UIWidgets.ComboBox, "Kind of support", "", ParamEnumArray.FromEnum(ACR_ESupportType))]
 ACR_ESupportType m_eType;
 [Attribute("1", UIWidgets.CheckBox, "Offer this call-in")]
 bool m_bEnabled = true;
 [Attribute("", UIWidgets.EditBox, "Name in the command panel (empty = default)")]
 string m_sName;
 [Attribute("-1", UIWidgets.EditBox, "Cost in RP (-1 = default)")]
 int m_iCost;
 [Attribute("-1", UIWidgets.EditBox, "Seconds before it can be called again (-1 = default)")]
 int m_iCooldown;
 [Attribute("-1", UIWidgets.EditBox, "Air lift only: RP when the AI commander calls it (-1 = half the cost)")]
 int m_iAICost;
	[Attribute("0", UIWidgets.ComboBox, "Bomb run delivery: virtual blasts, flyover prefab plus virtual blasts, or an editable call-in prefab", "", ParamEnumArray.FromEnum(ACR_EBombRunDelivery))]
	ACR_EBombRunDelivery m_eBombRunDelivery;
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Bomb run aircraft/flyover prefab or editable call-in spawner prefab", "et")]
	ResourceName m_sBombRunPrefab;
	[Attribute("300", UIWidgets.EditBox, "Flyover altitude above the target in metres (at least 150 m over any hill on the way)")]
	float m_fBombRunAltitude = 300;
	[Attribute("180", UIWidgets.EditBox, "Flyover speed in metres per second")]
	float m_fBombRunSpeed = 180;
	[Attribute("4", UIWidgets.EditBox, "Number of virtual bombs in a flyover or virtual run")]
	int m_iBombRunBombs = 4;
	[Attribute("28", UIWidgets.EditBox, "Distance between virtual bomb impacts in metres")]
	float m_fBombRunSpacing = 28;
	[Attribute("5", UIWidgets.EditBox, "Random lateral scatter for virtual bomb impacts in metres")]
	float m_fBombRunScatter = 5;

 // runtime
 float m_fReadyAt;

 void Resolve()
 {
  if (m_sName.IsEmpty()) m_sName = ACR_Supports.DefaultName(m_eType);
  if (m_iCost < 0) m_iCost = ACR_Supports.DefaultCost(m_eType);
  if (m_iCooldown < 0) m_iCooldown = ACR_Supports.DefaultCooldown(m_eType);
  // The AI commander can't fly bought helicopters (no pads of its own on generated maps),
  // so its air lifts cost half (Jacob, 14 Oct).
  if (m_iAICost < 0) m_iAICost = m_iCost / 2;
 }

 // What this call costs the caller.
 int CostFor(bool byAI)
 {
  if (byAI && m_eType == ACR_ESupportType.AIR_LIFT) return m_iAICost;
  return m_iCost;
 }

 // A faction's own copy of a configured entry (R15): cooldowns are per copy, never shared
 // with the settings entity or with another faction made from the same template.
 ACR_SupportEntry Copy()
 {
  ACR_SupportEntry copy = new ACR_SupportEntry();
  copy.m_eType = m_eType;
  copy.m_bEnabled = m_bEnabled;
  copy.m_sName = m_sName;
  copy.m_iCost = m_iCost;
  copy.m_iCooldown = m_iCooldown;
  copy.m_iAICost = m_iAICost;
  copy.m_eBombRunDelivery = m_eBombRunDelivery;
  copy.m_sBombRunPrefab = m_sBombRunPrefab;
  copy.m_fBombRunAltitude = m_fBombRunAltitude;
  copy.m_fBombRunSpeed = m_fBombRunSpeed;
  copy.m_iBombRunBombs = m_iBombRunBombs;
  copy.m_fBombRunSpacing = m_fBombRunSpacing;
  copy.m_fBombRunScatter = m_fBombRunScatter;
  copy.m_fReadyAt = 0;
  copy.Resolve();
  return copy;
 }
}

// A script-driven flight or firing run, updated every server frame.
class ACR_SupportRun : Managed
{
 bool m_bDone;
 bool Frame(ACR_Supports supports, float dt, float now) { return false; }
 void Finish() {}
}

// Bomb-run aircraft model crossing the target, flown by every machine with a player on its
// own non-replicated copy (a server-moved entity doesn't replicate its movement: clients
// saw it frozen where it spawned). The payload stays AC:A's synchronized virtual blasts.
// Jacob, 9 Oct: an audible jet over the target, "not too low". RHS's FA-18 model carries
// its own engine sound (a loop that starts within 4 km of the listener, heard to 1.5 km).
class ACR_LocalFlyover : Managed
{
	protected static ref array<ref ACR_LocalFlyover> s_All = {};
	static int s_iStarted; // tests
	IEntity m_Plane;
	vector m_vStart;
	vector m_vDir;
	float m_fSpeed;
	float m_fAltitude;
	float m_fStartedAt;
	float m_fSeconds;
	vector m_vAxes[3];

	static void Start(ResourceName prefab, vector start, vector dir, float speed, float altitude, float seconds)
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
		// The configured route ends 2.5 km beyond the target, but listeners farther downrange
		// can still be inside RHS's 1.5 km sound range. Let the plane leave that range locally
		// before deleting it, so entity cleanup does not cut the engine loop off mid-tail.
		if (elapsed <= m_fSeconds) return;
		PlayerController controller = GetGame().GetPlayerController();
		IEntity listener = null;
		if (controller) listener = controller.GetControlledEntity();
		if (listener && vector.DistanceXZ(listener.GetOrigin(), position) <= 1800) return;
		GetGame().GetCallqueue().Remove(Step);
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

// One door/nose gun of a support helicopter. The script aims and fires it
// (the AI gunners barely engage from a moving helicopter): it picks an enemy the
// gun can reach and see, leads for the helicopter's own speed, and fires bursts.
class ACR_HeliGun : Managed
{
 BaseCompartmentSlot m_Slot;
 IEntity m_Turret;
 TurretComponent m_TurretComponent;
 TurretControllerComponent m_Controller;
 string m_sName;
 ref array<bool> m_aBins = {};  // 36 bins of 10 deg: can it reach a target at that bearing from the nose
 int m_iLastAmmo = -1;
 float m_fYawCmd;
 float m_fPitchCmd;
 float m_fYawSign = 1;
 float m_fPitchSign = 1;
 float m_fLastYawErr = 999;
 int m_iWorse;
 IEntity m_Target;
 bool m_bFiring;
 float m_fBurstEnd;
 float m_fPauseEnd;
 float m_fNextPick;
 vector m_Wander;        // slowly drifting aim error (metres at the target)
 vector m_WanderGoal;
 float m_fNextWander;
 float m_fSettleUntil;   // time to get on a new target before the first burst
 static const float WANDER_METRES = 2.5;

 static int Bin(float bearing)
 {
  int bin = Math.Round((bearing + 180) / 10);
  while (bin < 0) bin += 36;
  while (bin >= 36) bin -= 36;
  return bin;
 }
 bool CoversBearing(float bearing)
 {
  if (m_aBins.Count() != 36) return false;
  return m_aBins[Bin(bearing)];
 }
 string Describe()
 {
  string text = "";
  for (int i = 0; i < 36; i++) if (m_aBins[i]) text += (i * 10 - 180).ToString() + " ";
  return text;
 }

 int Ammo()
 {
  if (!m_Controller || !m_Controller.GetWeaponManager()) return -1;
  BaseWeaponComponent weapon = m_Controller.GetWeaponManager().GetCurrentWeapon();
  if (!weapon || !weapon.GetCurrentMuzzle()) return -1;
  return weapon.GetCurrentMuzzle().GetAmmoCount();
 }

 bool Covers(vector point)
 {
  if (!m_TurretComponent) return false;
  vector excess = m_TurretComponent.GetAimingAngleExcess(point);
  return Math.AbsFloat(excess[0]) < 0.5 && Math.AbsFloat(excess[1]) < 0.5;
 }

 IEntity Occupant()
 {
  if (!m_Slot) return null;
  return m_Slot.GetOccupant();
 }

 bool Visible(IEntity heli, vector point)
 {
  TraceParam trace = new TraceParam();
  vector from = m_Turret.GetOrigin();
  trace.Start = from + (point - from).Normalized() * 2;
  trace.End = point;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  trace.Exclude = heli;
  float fraction = GetGame().GetWorld().TraceMove(trace, null);
  if (fraction >= 0.97) return true;
  IEntity hit = trace.TraceEnt;
  return hit && (hit == m_Target || hit.GetRootParent() == m_Target);
 }

 // Where to point: chest height, led for the helicopter's own motion (bullets
 // inherit it) and a little bullet drop.
 vector LeadPoint(vector heliVelocity, IEntity target)
 {
  vector point = target.GetOrigin() + Vector(0, 1.1, 0);
  float flight = vector.Distance(point, m_Turret.GetOrigin()) / 820;
  point = point - heliVelocity * flight;
  point[1] = point[1] + 0.02 * flight * flight * 9.8 * 25;
  return point;
 }

 void StopFire()
 {
  if (m_bFiring && m_Controller) m_Controller.SetFireWeaponWanted(false);
  m_bFiring = false;
 }

 void Update(IEntity heli, vector heliVelocity, vector center, array<IEntity> enemies, float now)
 {
  IEntity gunner = Occupant();
  if (!m_Controller || !m_TurretComponent || !ACR_Respawn.IsAlive(gunner)) { StopFire(); return; }
  // The script is the gunner's brain: keep the AI from fighting over the gun.
  AIControlComponent ai = AIControlComponent.Cast(gunner.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();

  if (m_Target && (!ACR_Respawn.IsAlive(m_Target) || !Covers(LeadPoint(heliVelocity, m_Target)))) m_Target = null;
  if (!m_Target && now >= m_fNextPick)
  {
   m_fNextPick = now + 400;
   float best = float.MAX;
   foreach (IEntity enemy : enemies)
   {
    vector point = LeadPoint(heliVelocity, enemy);
    if (!Covers(point)) continue;
    float score = vector.Distance(enemy.GetOrigin(), center);
    if (score >= best) continue;
    if (!Visible(heli, point)) continue;
    best = score;
    m_Target = enemy;
   }
   m_iWorse = 0;
   m_fLastYawErr = 999;
   if (m_Target) m_fSettleUntil = now + Math.RandomFloat(900, 1600);
  }
  if (!m_Target) { StopFire(); return; }

  // A human gunner in a moving helicopter: the aim drifts around the target and
  // each new target takes a moment to get on. The gun's own dispersion adds on top.
  if (now >= m_fNextWander)
  {
   m_fNextWander = now + Math.RandomFloat(500, 1100);
   m_WanderGoal = Vector(Math.RandomFloat(-1, 1), Math.RandomFloat(-0.5, 0.7), Math.RandomFloat(-1, 1)) * WANDER_METRES;
  }
  m_Wander = m_Wander + (m_WanderGoal - m_Wander) * 0.05;
  vector aim = LeadPoint(heliVelocity, m_Target) + m_Wander;
  vector desired = (aim - m_Turret.GetOrigin()).Normalized();
  vector current = m_TurretComponent.GetAimingDirectionWorld();
  float errYaw = ACR_HeliRun.Wrap((Math.Atan2(desired[0], desired[2]) - Math.Atan2(current[0], current[2])) * Math.RAD2DEG);
  float errPitch = (Math.Asin(Math.Clamp(desired[1], -1, 1)) - Math.Asin(Math.Clamp(current[1], -1, 1))) * Math.RAD2DEG;
  // Closed loop on the gun's real aim; if the error keeps growing the axis sign is flipped.
  if (Math.AbsFloat(errYaw) > m_fLastYawErr + 0.2 && Math.AbsFloat(errYaw) > 5) m_iWorse++;
  else m_iWorse = 0;
  if (m_iWorse > 12)
  {
   m_fYawSign = -m_fYawSign;
   m_iWorse = 0;
   PrintFormat("[ACR] SUPPORT gun %1: flipped yaw sign", m_sName);
  }
  m_fLastYawErr = Math.AbsFloat(errYaw);
  m_fYawCmd = Math.Clamp(m_fYawCmd + m_fYawSign * errYaw * Math.DEG2RAD * 0.5, -3, 3);
  m_fPitchCmd = Math.Clamp(m_fPitchCmd + m_fPitchSign * errPitch * Math.DEG2RAD * 0.5, -1.5, 1.5);
  m_Controller.SetAimingAngles(m_fYawCmd, m_fPitchCmd);

  bool onTarget = Math.AbsFloat(errYaw) < 1.5 && Math.AbsFloat(errPitch) < 1.5;
  if (m_bFiring && now > m_fBurstEnd)
  {
   StopFire();
   m_fPauseEnd = now + Math.RandomFloat(400, 800);
  }
  else if (!m_bFiring && onTarget && now > m_fPauseEnd && now > m_fSettleUntil)
  {
   m_Controller.SetFireWeaponWanted(true);
   m_bFiring = true;
   m_fBurstEnd = now + Math.RandomFloat(700, 1300);
  }
  else if (m_bFiring && !onTarget && Math.AbsFloat(errYaw) > 6) StopFire();
 }
}

// A crewed gunship flown by script (the AI cannot fly).
//  Rocket run: one straight pass with a rocket salvo at enemies around the target
//  (or spread over the area), then a climb on the way out.
//  Loiter: the helicopter's guns are measured when it arrives, and it flies the
//  pattern that keeps the most guns on the target:
//   - side guns (UH-1H doors): a pylon-turn orbit with the gun side toward the target;
//   - nose/tail guns (Mi-8 bow and ramp PKTs): repeated attack passes, nose gun and
//     rockets on the way in, ramp gun covering the target on the way out.
class ACR_HeliRun : ACR_SupportRun
{
 static const float SPEED_RUN = 55;
 static const float SPEED_LOITER = 36;
 static const float LOITER_RADIUS = 280;
 static const float LOITER_TIME_MS = 90000;
 static const float ALT_TRANSIT = 70;
 static const float ALT_LOITER = 55;
 static const float ALT_EGRESS = 150;  // climb away after the attack (out of small-arms reach)
 static const float RUN_TARGET_RADIUS = 120; // rocket run: enemies this close to the target are aimed at
 static const float RUN_SPREAD = 35;   // rocket run with no enemy seen: spread over the area

 IEntity m_Vehicle;
 string m_sFaction;
 ACR_StationedHeli m_Home;  // flies from and back to this helipad (null = off-map helicopter, deleted after)
 static const float LIFT_HEIGHT = 30;   // m: straight up off the pad before heading out
 static const float LIFT_RATE = 4;      // m/s
 protected bool m_bLifting;
 bool m_bReturning;
 bool m_bLanded;
 protected float m_fSpeed;
 static const float FLIGHT_ACCEL = 1.5; // m/s squared: about 37 s to cruise from a hover
 static const float FLIGHT_BRAKE = 3;
 protected float m_fNextTerrain;
 protected float m_fTerrainY;
 protected float m_fTerrainSpeed = SPEED_RUN;
 protected ref array<IEntity> m_aFlightIgnore = {};

 static float ApproachSpeed(float speed, float wanted, float dt)
 {
  return speed + Math.Clamp(wanted - speed, -FLIGHT_BRAKE * dt, FLIGHT_ACCEL * dt);
 }

 // Reserve 3 s for vertical-speed response and use only 5 of the available 8 m/s climb.
 // Slow down to buy climb time when a ridge cannot be cleared at the current speed.
 static float ClimbSafeSpeed(float distance, float rise)
 {
  if (rise <= 0) return SPEED_RUN;
  return Math.Clamp((distance - 25) / (rise / 5 + 3), 0, SPEED_RUN);
 }

 protected void IgnoreFlightOccupants()
 {
  m_aFlightIgnore.Clear();
  m_aFlightIgnore.Insert(m_Vehicle);
  SCR_BaseCompartmentManagerComponent flightSeats = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<BaseCompartmentSlot> occupied = {};
  if (flightSeats) flightSeats.GetCompartments(occupied);
  foreach (BaseCompartmentSlot seat : occupied)
   if (seat.GetOccupant()) m_aFlightIgnore.Insert(seat.GetOccupant());
 }

 // Soldiers never block the rotor envelope (Jacob 19 Oct: a lift hovered 1 m up for 70 s
 // because its squad stood under the tail box, so they never got the get-in order).
 protected bool RotorTraceFilter(notnull IEntity e, vector start = "0 0 0", vector dir = "0 0 0")
 {
  IEntity root = e;
  for (IEntity part = e; part; part = part.GetParent())
  {
   if (ChimeraCharacter.Cast(part)) return false;
   root = part;
  }
  return !LooseGear(root);
 }

 // Nor does loose gear: weapons and kit dropped by casualties under a landing spot held a lift
 // on the ground until cleanup removed them (Everon soak 23 Oct: 6 min, 500 ms/s). Only a small
 // free-standing weapon or item counts; vehicles carry item components in their hierarchy.
 static bool LooseGear(IEntity root)
 {
  if (!root || Vehicle.Cast(root)) return false;
  if (!root.FindComponent(WeaponComponent) && !root.FindComponent(InventoryItemComponent)) return false;
  vector mins, maxs;
  root.GetBounds(mins, maxs);
  return Math.Max(maxs[0] - mins[0], Math.Max(maxs[1] - mins[1], maxs[2] - mins[2])) < 2.5;
 }

 // Last movement check, including vertical takeoff and descent. Prediction can be
 // stale when another vehicle moves in; never advance the rotor envelope through it.
 // vertical: straight up or down over a landing spot, whose ground was checked when it was
 // picked (up to 2.5 m of rise): terrain is left out, or a slope under the tail stops it short.
 protected bool RotorMoveClear(vector from, vector to, bool vertical)
 {
  TraceFlags flags = TraceFlags.WORLD | TraceFlags.ENTS;
  if (vertical) flags = TraceFlags.ENTS;
  TraceBox rotor = new TraceBox();
  rotor.Mins = Vector(-12, 2.5, -12);
  rotor.Maxs = Vector(12, 10, 12);
  rotor.Start = from;
  rotor.End = to;
  rotor.ExcludeArray = m_aFlightIgnore;
  rotor.Flags = flags;
  BaseWorld world = GetGame().GetWorld();
  if (world.TraceMove(rotor, RotorTraceFilter) < 1) { m_BlockedBy = rotor.TraceEnt; return false; }
  // A movement trace can permit an overlap already present at its start. Check
  // the destination volume too, including when a moving obstacle entered it.
  rotor.Start = to;
  if (world.TracePosition(rotor, RotorTraceFilter) < 0) { m_BlockedBy = rotor.TraceEnt; return false; }
  // The long, narrow tail extends beyond the main disk. Enclose its rotated bounds
  // in a world-aligned box rather than using a full rotor-width square at tail height.
  TraceBox tail = new TraceBox();
  vector nose = Heading(m_fBodyYaw);
  vector tailCenter = nose * -11;
  float halfX = Math.AbsFloat(nose[0]) * 6 + Math.AbsFloat(nose[2]) * 3;
  float halfZ = Math.AbsFloat(nose[2]) * 6 + Math.AbsFloat(nose[0]) * 3;
  tail.Mins = Vector(-halfX, 1, -halfZ);
  tail.Maxs = Vector(halfX, 10, halfZ);
  tail.Start = from + tailCenter;
  tail.End = to + tailCenter;
  tail.ExcludeArray = m_aFlightIgnore;
  tail.Flags = flags;
  if (world.TraceMove(tail, RotorTraceFilter) < 1) { m_BlockedBy = tail.TraceEnt; return false; }
  tail.Start = tail.End;
  if (world.TracePosition(tail, RotorTraceFilter) >= 0) return true;
  m_BlockedBy = tail.TraceEnt;
  return false;
 }

 // A blocked helicopter holds still and retries a few times a second rather than repeating
 // the rotor traces every frame (Everon soak 23 Oct: a lift held down for 6 min cost ~8 ms
 // per frame). The blocker is logged so it can be fixed at its source.
 static const float GUARD_RETRY_MS = 250;
 static const float GUARD_LOG_MS = 10000;
 protected IEntity m_BlockedBy;
 protected float m_fGuardRetryAt;
 protected float m_fGuardLogAt;

 protected void GuardFlightMove(vector previous, bool vertical = false)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fGuardRetryAt)
  {
   m_Pos = previous;
   m_fClimb = 0;
   m_fSpeed = 0;
   return;
  }
  IgnoreFlightOccupants();
  m_BlockedBy = null;
  if (RotorMoveClear(previous, m_Pos, vertical)) return;
  // An upward escape may still be clear even when forward movement is blocked.
  vector escape = previous;
  escape[1] = Math.Max(previous[1], m_Pos[1]);
  if (escape[1] > previous[1] && RotorMoveClear(previous, escape, vertical)) m_Pos = escape;
  else
  {
   m_Pos = previous;
   m_fClimb = 0;
   m_fGuardRetryAt = now + GUARD_RETRY_MS;
   if (now >= m_fGuardLogAt)
   {
    m_fGuardLogAt = now + GUARD_LOG_MS;
    string blocker = "terrain";
    if (m_BlockedBy)
    {
     blocker = m_BlockedBy.ClassName();
     if (m_BlockedBy.GetPrefabData()) blocker = blocker + " " + m_BlockedBy.GetPrefabData().GetPrefabName();
    }
    PrintFormat("[ACR] SUPPORT %1 blocked at %2 by %3", LogName(), previous, blocker);
   }
  }
  m_fSpeed = 0;
 }

 protected float FlightClearance(vector forward, float above, float now)
 {
  if (now < m_fNextTerrain) return m_fTerrainY;
  m_fNextTerrain = now + 250;
  BaseWorld world = GetGame().GetWorld();
  vector side = Vector(forward[2], 0, -forward[0]);
  float horizon = Math.Max(400, m_fSpeed * 20 + m_fSpeed * m_fSpeed / (FLIGHT_BRAKE * 2));
  m_fTerrainSpeed = SPEED_RUN;
  m_fTerrainY = m_Pos[1] - 20;
  for (float ahead = 0; ahead <= horizon; ahead += 20)
  {
   for (int lane = -1; lane <= 1; lane++)
   {
    vector probe = m_Pos + forward * ahead + side * (lane * 18);
    float surface = Math.Max(0, world.GetSurfaceY(probe[0], probe[2]));
    float rise = surface + 35 - m_Pos[1];
    m_fTerrainSpeed = Math.Min(m_fTerrainSpeed, ClimbSafeSpeed(ahead, rise));
    float time = ahead / Math.Max(10, m_fSpeed);
    m_fTerrainY = Math.Max(m_fTerrainY, surface + above - Math.Max(0, time - 3) * 5);
   }
  }
  // Sweep the whole rotor/tail envelope along the stopping corridor; terrain samples
  // alone know nothing about trees, buildings or powerline pylons. Ignore our own hull.
  TraceBox sweep = new TraceBox();
  sweep.Mins = Vector(-18, 1, -18);
  sweep.Maxs = Vector(18, 10, 18);
  sweep.Start = m_Pos;
  float reach = Math.Max(40, m_fSpeed * m_fSpeed / (FLIGHT_BRAKE * 2) + m_fSpeed * 3 + 30);
  sweep.End = m_Pos + forward * reach;
  // Seated characters are not necessarily children of the airframe in trace queries.
  // Their gear must not be mistaken for an obstacle after boarding.
  IgnoreFlightOccupants();
  sweep.ExcludeArray = m_aFlightIgnore;
  sweep.Flags = TraceFlags.ENTS;
  float hit = world.TraceMove(sweep, null);
  if (hit < 1)
  {
   if (System.IsCLIParam("acrAirLiftContactTest")) PrintFormat("[ACR] LIFTCONTACT obstacle %1 at %2, fraction %3, position %4", sweep.TraceEnt, sweep.End, hit, m_Pos);
   m_fTerrainSpeed = Math.Min(m_fTerrainSpeed, Math.Max(0, (reach * hit - 30) / (m_fSpeed / FLIGHT_BRAKE + 3)));
   m_fTerrainY = Math.Max(m_fTerrainY, m_Pos[1] + 25);
  }
  return m_fTerrainY;
 }
 bool m_bLoiter;
 bool m_bDoorGuns;   // transport version: no rockets
 static const float DOOR_SIDE_SWAP_MS = 30000;
 float m_fNextSideSwap;
 float m_fLoiterMs = LOITER_TIME_MS;
 float m_fClimb;     // smoothed vertical speed (m/s)
 float m_fNextAltLog;
 bool m_bAimedAtEnemy;
 vector m_Target;
 vector m_Dir;
 vector m_Pos;
 float m_fYaw;       // direction of travel
 float m_fBodyYaw;   // where the nose points (differs in a pylon turn)
 float m_fRoll;
 ResourceName m_Rocket;
 int m_iRockets;
 float m_fNextRocket;
 float m_fDiveUntil;  // nose held down a little while firing rockets
 int m_iSalvoLeft;          // loiter: rockets still to fire in the current salvo
 vector m_SalvoAim;
 int m_iPhase;       // 0 inbound, 1 on station, 2 outbound
 float m_fOrbitEnd;
 vector m_ExitDir;
 float m_fStarted;
 bool m_bDestroyed;
 ref array<ref ACR_HeliGun> m_Guns = {};
 bool m_bAnalyzed;
 bool m_bPasses;     // attack passes instead of an orbit
 float m_fSideBearing = -90; // orbit: target bearing relative to the nose
 bool m_bPassOutbound;
 float m_fNextGunLog;
 float m_fNextEnemyScan;
 float m_fNextSideCheck;
 int m_iStack;        // height band (0-2) so our helicopters don't share an altitude
 static const float STACK_STEP = 30;        // m between height bands
 static const float SEPARATION = 160;       // m: closer than this (and within 40 m height), steer apart
 static const float OPPORTUNITY_RADIUS = 450; // m around the helicopter: gunners also engage enemies here

 bool SideCanFire(float bearing)
 {
  foreach (ACR_HeliGun gun : m_Guns)
   if (gun.CoversBearing(bearing) && ACR_Respawn.IsAlive(gun.Occupant()) && gun.Ammo() != 0) return true;
  return false;
 }
 ref array<IEntity> m_aEnemies = {};

 bool Start(ACR_Supports supports, ResourceName prefab, float now)
 {
  float startDistance = 2200;
  if (m_bLoiter) startDistance = 1500;
  m_Pos = SafeStart(m_Target - m_Dir * startDistance);
  vector toward = m_Target - m_Pos;
  toward[1] = 0;
  if (toward.Length() > 1) m_Dir = toward.Normalized();
  m_Pos[1] = SafeAltitude(m_Pos, m_Dir, ALT_TRANSIT + m_iStack * STACK_STEP);
  m_fYaw = Math.Atan2(m_Dir[0], m_Dir[2]) * Math.RAD2DEG;
  m_fBodyYaw = m_fYaw;
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return false;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(m_fYaw, 0, 0), parameters.Transform);
  parameters.Transform[3] = m_Pos;
  m_Vehicle = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!m_Vehicle) return false;
  supports.m_Vehicles.Insert(m_Vehicle);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) physics.EnableGravity(false);
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<ECompartmentType> seats = {ECompartmentType.PILOT, ECompartmentType.TURRET};
  if (compartments) compartments.SpawnDefaultOccupants(seats);
  ResourceName selectedCrew = supports.ConfiguredCrewPrefab(m_sFaction, !m_bDoorGuns);
  if (!selectedCrew.IsEmpty()) supports.Station().ApplyCrewPrefab(m_Vehicle, selectedCrew);
  BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(m_Vehicle.FindComponent(BaseVehicleControllerComponent));
  if (controller) controller.ForceStartEngine();
  // Arrives at full rotor speed, not spooling up from a standstill (Jacob, 2 Oct).
  Spool();
  m_fStarted = now;
  PrintFormat("[ACR] SUPPORT helicopter %1 inbound from %2 (%3 m out, clear of bases that aren't ours: %4)", prefab, m_Pos, (int)vector.DistanceXZ(m_Pos, m_Target), ClearOfEnemies(m_Pos));
  m_iRockets = 14;
  if (m_bLoiter) m_iRockets = 16;
  if (m_bDoorGuns) m_iRockets = 0;
  return true;
 }

 // Takes off from its helipad (the helicopter and crew are already there).
 bool StartFromPad(ACR_Supports supports, ACR_StationedHeli home, float now)
 {
  if (!home || !home.m_Vehicle) return false;
  m_Home = home;
  m_Vehicle = home.m_Vehicle;
  m_Pos = m_Vehicle.GetOrigin();
  m_Start = m_Pos;
  vector toward = m_Target - m_Pos;
  toward[1] = 0;
  if (toward.Length() > 1) m_Dir = toward.Normalized();
  m_fYaw = m_Vehicle.GetYawPitchRoll()[0];
  m_fBodyYaw = m_fYaw;
  m_bLifting = true;
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) { physics.EnableGravity(false); physics.SetVelocity(vector.Zero); }
  supports.Station().ReplaceCrew(m_Vehicle, m_sFaction, m_Home.m_bGunship);
  BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(m_Vehicle.FindComponent(BaseVehicleControllerComponent));
  if (controller) controller.ForceStartEngine();
  Spool();
  m_fStarted = now;
  m_iRockets = 14;
  if (m_bLoiter) m_iRockets = 16;
  if (m_bDoorGuns) m_iRockets = 0;
  PrintFormat("[ACR] SUPPORT %1 taking off from its helipad at %2, %3 m from the target", home.m_sName, m_Pos, (int)vector.DistanceXZ(m_Pos, m_Target));
  return true;
 }

 // Straight up off the pad, the nose swinging round toward the target.
 protected bool Lift(float dt)
 {
  vector previous = m_Pos;
  float ground = GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2]);
  float rise = LIFT_RATE;
  if (m_Pos[1] - ground < 3) rise = LIFT_RATE * 0.5;
  m_Pos[1] = m_Pos[1] + rise * dt;
  float wantYaw = Math.Atan2(m_Dir[0], m_Dir[2]) * Math.RAD2DEG;
  TurnToward(wantYaw, 15, dt);
  m_fBodyYaw = m_fYaw;
  GuardFlightMove(previous, true);
  vector velocity = vector.Zero;
  if (dt > 0) velocity = (m_Pos - previous) * (1 / dt);
  Fly(velocity, dt);
  if (m_Pos[1] - ground >= LIFT_HEIGHT)
  {
   m_bLifting = false;
   m_fClimb = rise;
   m_fSpeed = 0;
   m_fNextTerrain = 0;
   PrintFormat("[ACR] SUPPORT %1 off the ground (%2 m up), heading out", LogName(), (int)(m_Pos[1] - ground));
  }
  return true;
 }

 protected void Place(vector velocity, float pitch, float roll)
 {
  vector transform[4];
  Math3D.AnglesToMatrix(Vector(m_fBodyYaw, pitch, roll), transform);
  transform[3] = m_Pos;
  m_Vehicle.SetTransform(transform);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics)
  {
   physics.SetVelocity(velocity);
   physics.SetAngularVelocity(vector.Zero);
  }
 }

 // Attitude from the motion itself (Jacob 20 Oct: forward flight twitched, turns had no bank):
 // nose down to speed up and at cruise, nose up to brake, banked by the turn's sideways
 // acceleration. Eased and rate-limited, so it never snaps from one frame to the next.
 static const float MAX_BANK = 28;
 static const float BANK_RATE = 20;   // deg/s
 static const float PITCH_RATE = 12;  // deg/s
 float m_fPitch;
 protected vector m_vLastVelocity;
 protected vector m_vAccel;           // smoothed horizontal acceleration
 protected float m_fYawRate;          // deg/s the track is turning

 protected void Fly(vector velocity, float dt, float pitchBias = 0)
 {
  if (dt > 0)
  {
   vector raw = (velocity - m_vLastVelocity) * (1 / dt);
   raw[1] = 0;
   m_vAccel = m_vAccel + (raw - m_vAccel) * Math.Min(1, dt * 3);
  }
  m_vLastVelocity = velocity;
  vector nose = Heading(m_fBodyYaw);
  vector right = Vector(nose[2], 0, -nose[0]);
  float cruise = Math.Clamp(vector.Dot(velocity, nose) * 0.08, -2, 5);
  float wantPitch = pitchBias - cruise - Math.Atan2(vector.Dot(m_vAccel, nose) * 0.6, 9.81) * Math.RAD2DEG;
  float wantRoll = -Math.Atan2(vector.Dot(m_vAccel, right), 9.81) * Math.RAD2DEG;
  m_fPitch = Ease(m_fPitch, Math.Clamp(wantPitch, -14, 12), PITCH_RATE, dt);
  m_fRoll = Ease(m_fRoll, Math.Clamp(wantRoll, -MAX_BANK, MAX_BANK), BANK_RATE, dt);
  Place(velocity, m_fPitch, m_fRoll);
 }

 static float Ease(float value, float wanted, float maxRate, float dt)
 {
  float step = (wanted - value) * Math.Min(1, dt * 2.5);
  return value + Math.Clamp(step, -maxRate * dt, maxRate * dt);
 }

 // Turns the track toward wantYaw; the turn rate builds up and eases off instead of
 // switching between full rate and none.
 protected void TurnToward(float wantYaw, float maxRate, float dt)
 {
  float delta = Wrap(wantYaw - m_fYaw);
  m_fYawRate += (Math.Clamp(delta * 1.5, -maxRate, maxRate) - m_fYawRate) * Math.Min(1, dt * 3);
  float turn = m_fYawRate * dt;
  if (turn * delta > 0 && Math.AbsFloat(turn) > Math.AbsFloat(delta)) turn = delta;
  m_fYaw = Wrap(m_fYaw + turn);
 }

 // Back to the pad: slows down on the way in, hovers over the pad and sets down.
 protected bool FlyHome(ACR_Supports supports, float dt, float now)
 {
  foreach (ACR_HeliGun idle : m_Guns) idle.StopFire();
  if (!FlyToSpot(supports, m_Home.m_Pad.Spot(), dt, now)) return true;
  Touchdown(supports);
  return false;
 }

 // Flies to a landing spot (a pad, or an air lift's landing zone): slows down on the way in,
 // hovers over it and sets down. True once it is down.
 protected bool FlyToSpot(ACR_Supports supports, vector pad, float dt, float now)
 {
  vector flat = pad - m_Pos;
  flat[1] = 0;
  float d = flat.Length();
  vector toPad = m_Dir;
  if (d > 0.01) toPad = flat.Normalized();
  float padYaw = Math.Atan2(toPad[0], toPad[2]) * Math.RAD2DEG;
  float delta = Wrap(padYaw - m_fYaw);
  if (d > 3) TurnToward(padYaw, 24, dt);
  m_fBodyYaw = Wrap(m_fBodyYaw + Math.Clamp(Wrap(m_fYaw - m_fBodyYaw), -40 * dt, 40 * dt));
  // Slower near the pad, and while still turning toward it (no circling round it).
  float facing = Math.Max(0.2, Math.Cos(Math.Min(Math.AbsFloat(delta), 90) * Math.DEG2RAD));
  float wanted = Math.Clamp(d * 0.22, 0, SPEED_RUN) * facing;
  vector forward = Heading(m_fYaw);
  float terrainY = m_Pos[1];
  if (d > 25)
  {
   terrainY = FlightClearance(forward, ALT_TRANSIT + m_iStack * STACK_STEP, now);
   wanted = Math.Min(wanted, m_fTerrainSpeed);
  }
  // Brake early enough to stop with a bounded deceleration.
  wanted = Math.Min(wanted, Math.Sqrt(FLIGHT_BRAKE * 2 * d));
  m_fSpeed = ApproachSpeed(m_fSpeed, wanted, dt);
  vector previous = m_Pos;
  vector move;
  // The last stretch is flown straight at the pad whatever the nose does (a hover taxi).
  if (d < 40) move = toPad * Math.Min(Math.Min(m_fSpeed, d * 0.5 + 0.3) * dt, d);
  else move = forward * (m_fSpeed * dt);
  m_Pos = m_Pos + move;
  // Height: transit height far out, then a glide down to a hover over the pad, then down.
  float ground = GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2]);
  float wantedY;
  if (d > 300) wantedY = terrainY;
  else
  {
   wantedY = Math.Max(pad[1] + Math.Clamp((d - 8) * 0.35, 12, ALT_TRANSIT), ground + 12);
   if (d > 25) wantedY = Math.Max(wantedY, terrainY);
  }
  float rate = Math.Clamp((wantedY - m_Pos[1]) * 0.6, -6, 8);
  if (d < 1)
  {
   wantedY = pad[1];
   rate = -Math.Clamp((m_Pos[1] - pad[1]) * 0.4, 1.2, 4);
  }
  m_fClimb += (rate - m_fClimb) * Math.Min(1, dt * 1.5);
  m_Pos[1] = previous[1] + m_fClimb * dt;
  if (d < 1) m_Pos[1] = Math.Max(pad[1], m_Pos[1]);
  // Hold horizontal movement if the next step would enter terrain while still climbing.
  if (d > 40 && m_Pos[1] < ground + 20)
  {
   m_Pos[0] = previous[0];
   m_Pos[2] = previous[2];
   m_fSpeed = 0;
  }
  if (d > 300)
  {
   vector away = Separation(supports);
   if (away != vector.Zero) m_Pos = m_Pos + away * (2 * dt);
  }
  GuardFlightMove(previous, d < 1);
  vector velocity = vector.Zero;
  if (dt > 0) velocity = (m_Pos - previous) * (1 / dt);
  Fly(velocity, dt);
  if (now >= m_fNextAltLog)
  {
   m_fNextAltLog = now + 5000;
   PrintFormat("[ACR] SUPPORT %1 coming in: %2 m from its landing spot, %3 m up, %4 m/s", LogName(), (int)d, (int)(m_Pos[1] - ground), (int)m_fSpeed);
  }
  return d < 1 && m_Pos[1] <= pad[1] + 0.35;
 }

 // On the pad: physics back on, engine off, servicing starts.
 void Touchdown(ACR_Supports supports)
 {
  if (m_bLanded || !m_Vehicle || !m_Home) return;
  m_bLanded = true;
  vector pad = m_Home.m_Pad.Spot();
  m_Pos = pad;
  m_fBodyYaw = m_Vehicle.GetYawPitchRoll()[0];
  Place(vector.Zero, 0, 0);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) physics.EnableGravity(true);
  VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(m_Vehicle.FindComponent(VehicleHelicopterSimulation));
  if (rotors)
  {
   rotors.SetThrottle(0);
   rotors.EngineStop();
  }
  supports.Station().Landed(m_Home);
 }

 static const float SPOOL_MAX_SECONDS = 25;
 static const float ENEMY_CLEARANCE = 1200; // spool-up spot this far from bases that aren't ours (Jacob, 2 Oct)
 vector m_Start;

 // It sits ~20 s at its start point while the rotors spool up: not near anything that isn't
 // ours. Turn the start round the target until it is clear, else start at our nearest base.
 vector SafeStart(vector start)
 {
  vector away = start - m_Target;
  away[1] = 0;
  float distance = away.Length();
  if (distance < 1) return start;
  float yaw = Math.Atan2(away[0], away[2]);
  array<float> turns = {0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5};
  foreach (float turn : turns)
  {
   vector candidate = m_Target + Vector(Math.Sin(yaw + turn) * distance, 0, Math.Cos(yaw + turn) * distance);
   if (ACR_Manager.OnMap(candidate) && ClearOfEnemies(candidate)) { m_Start = candidate; return candidate; }
  }
  ACR_Base home;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), m_Target);
   if (d > 200 && d < best) { best = d; home = base; }
  }
  if (home) start = home.GetOrigin();
  m_Start = start;
  return start;
 }

 bool ClearOfEnemies(vector point)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner != m_sFaction && vector.DistanceXZ(base.GetOrigin(), point) < ENEMY_CLEARANCE) return false;
  return true;
 }
 protected bool m_bSpooled;
 // Full throttle: the rotors run at flying speed (the script flies the airframe itself).
 void Spool()
 {
  VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(m_Vehicle.FindComponent(VehicleHelicopterSimulation));
  if (!rotors) return;
  if (!rotors.EngineIsOn()) rotors.EngineStart();
  rotors.SetThrottle(1);
 }

 // Terrain-following height: clear the ground (and sea) ahead.
 static float SafeAltitude(vector pos, vector forward, float above)
 {
  BaseWorld world = GetGame().GetWorld();
  float ground = 0;
  // Looks 400 m ahead so climbs start early instead of at the slope's foot.
  for (int i = 0; i < 6; i++)
  {
   vector probe = pos + forward * (i * 80);
   ground = Math.Max(ground, world.GetSurfaceY(probe[0], probe[2]));
  }
  return ground + above;
 }

 bool IsDestroyed()
 {
  DamageManagerComponent damage = DamageManagerComponent.Cast(m_Vehicle.FindComponent(DamageManagerComponent));
  return damage && damage.GetState() == EDamageState.DESTROYED;
 }

 // Script flies the helicopter, so the game's own loss of lift never applied: with
 // a rotor shot off it kept flying (live 1 Oct, Mi-8). The damage manager's movement
 // damage combines engine, gearbox and every rotor (a lost tail rotor counts too).
 static const float CRIPPLED_MOVEMENT_DAMAGE = 0.75;
 float MovementDamage()
 {
  DamageManagerComponent damage = DamageManagerComponent.Cast(m_Vehicle.FindComponent(DamageManagerComponent));
  if (!damage) return 0;
  return damage.GetMovementDamage();
 }

 static vector Heading(float yaw)
 {
  return Vector(Math.Sin(yaw * Math.DEG2RAD), 0, Math.Cos(yaw * Math.DEG2RAD));
 }

 static float Wrap(float angle)
 {
  while (angle > 180) angle -= 360;
  while (angle < -180) angle += 360;
  return angle;
 }

 // Find every crewed gun and measure which target bearings (relative to the
 // nose, target 60 m below at 280 m) it can reach, then pick the flight pattern.
 void AnalyzeGuns(ACR_Supports supports)
 {
  m_bAnalyzed = true;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  // Some helicopters (the Mi-8) have no default crew for their guns: seat soldiers there.
  AIGroup crewGroup;
  foreach (BaseCompartmentSlot seat : slots)
  {
   if (seat.GetOccupant())
   {
    AIControlComponent seated = AIControlComponent.Cast(seat.GetOccupant().FindComponent(AIControlComponent));
    if (seated && seated.GetControlAIAgent()) crewGroup = seated.GetControlAIAgent().GetParentGroup();
    if (crewGroup) break;
   }
  }
  ResourceName crewman = supports.CrewPrefab(m_sFaction, !m_bDoorGuns);
  foreach (BaseCompartmentSlot empty : slots)
  {
   if (empty.GetOccupant() || empty.GetType() != ECompartmentType.TURRET) continue;
   IEntity seatedCrew = empty.SpawnCharacterInCompartment(crewman, crewGroup);
   PrintFormat("[ACR] SUPPORT seated %1 in %2: %3", crewman, empty.GetCompartmentName(), seatedCrew != null);
  }
  foreach (BaseCompartmentSlot slot : slots)
  {
   if (slot.GetType() != ECompartmentType.TURRET) continue;
   IEntity turret = slot.GetOwner();
   ACR_HeliGun gun = new ACR_HeliGun();
   gun.m_Slot = slot;
   gun.m_Turret = turret;
   gun.m_TurretComponent = TurretComponent.Cast(turret.FindComponent(TurretComponent));
   gun.m_Controller = TurretControllerComponent.Cast(turret.FindComponent(TurretControllerComponent));
   gun.m_sName = slot.GetCompartmentName();
   if (!gun.m_TurretComponent || !gun.m_Controller) continue;
   // Rocket pods and fixed guns are pilot weapons (no turret slot); this is crewed guns only.
   for (int bin = 0; bin < 36; bin++)
   {
    vector point = m_Pos + Heading(m_fBodyYaw + bin * 10 - 180) * LOITER_RADIUS;
    point[1] = m_Pos[1] - 60;
    gun.m_aBins.Insert(gun.Covers(point));
   }
   m_Guns.Insert(gun);
   PrintFormat("[ACR] SUPPORT gun %1 (occupant=%2 ammo=%3) reaches bearings: %4", gun.m_sName, slot.GetOccupant() != null, gun.Ammo(), gun.Describe());
  }
  // Best side bearing for an orbit: most guns covering +-(60..120).
  int bestCount = 0;
  float bestBearing = -90;
  array<int> signs = {-1, 1};
  array<int> sideBearings = {90, 80, 100, 70, 110, 60, 120};
  foreach (int b : sideBearings)
  {
   foreach (int sign : signs)
   {
    int count = 0;
    foreach (ACR_HeliGun g : m_Guns) if (g.CoversBearing(sign * b)) count++;
    if (count > bestCount) { bestCount = count; bestBearing = sign * b; }
   }
  }
  int noseTail = 0;
  foreach (ACR_HeliGun g2 : m_Guns)
  {
   if (g2.CoversBearing(0) || g2.CoversBearing(10) || g2.CoversBearing(-10)) noseTail++;
   else if (g2.CoversBearing(180) || g2.CoversBearing(170) || g2.CoversBearing(-170)) noseTail++;
  }
  m_fSideBearing = bestBearing;
  // Door gunners always orbit (the Mi-8's side window guns barely got a look in on passes).
  m_bPasses = bestCount == 0 || (!m_bDoorGuns && noseTail > bestCount);
  PrintFormat("[ACR] SUPPORT gunship pattern: %1 (side guns %2 at %3 deg, nose/tail guns %4)", m_bPasses, bestCount, bestBearing, noseTail);
 }

 // Steering away from our other helicopters nearby (2 Oct live: two orbiting near
 // each other collided). Zero when none is close.
 vector Separation(ACR_Supports supports)
 {
  vector push = vector.Zero;
  array<ACR_HeliRun> others = {};
  supports.Helicopters(others);
  foreach (ACR_HeliRun other : others)
  {
   if (other == this || !other.m_Vehicle || other.m_bDestroyed) continue;
   vector apart = m_Pos - other.m_Pos;
   float height = Math.AbsFloat(apart[1]);
   apart[1] = 0;
   float d = apart.Length();
   if (d >= SEPARATION || height > 40) continue;
   if (d < 1) apart = Heading(m_fYaw + 90);
   push = push + apart.Normalized() * ((1 - d / SEPARATION) * 2.5);
  }
  return push;
 }

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  if (!m_Vehicle) return false;
  float movementDamage = MovementDamage();
  if (IsDestroyed() || movementDamage >= CRIPPLED_MOVEMENT_DAMAGE)
  {
   m_bDestroyed = true;
   Physics wreck = m_Vehicle.GetPhysics();
   if (wreck) wreck.EnableGravity(true);
   supports.m_Manager.NotifyFaction(m_sFaction, "Support helicopter is going down!");
   PrintFormat("[ACR] SUPPORT helicopter going down (destroyed=%1 movement damage=%2)", IsDestroyed(), movementDamage);
   if (m_Home) supports.Station().Lost(m_Home);
   return false;
  }
  float limit = 300000;
  if (m_Home) limit = 600000; // there and back (Finish puts it on its pad if it ran out)
  if (now - m_fStarted > limit) return false;
  if (!m_bAnalyzed && !m_bLifting && now - m_fStarted > 500) AnalyzeGuns(supports);
  Spool();
  if (StillSpooling(now)) return true;
  if (m_bLifting) return Lift(dt);
  if (m_bReturning) return FlyHome(supports, dt, now);
  return FlySupport(supports, dt, now);
 }

 // The game spools the rotors up over ~20 s and has no way to set their speed, so the
 // helicopter waits out of sight at its start point until they're at flying speed.
 protected bool StillSpooling(float now)
 {
  if (!m_bSpooled)
  {
   float rpm, target;
   VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(m_Vehicle.FindComponent(VehicleHelicopterSimulation));
   if (rotors && rotors.RotorCount() > 0)
   {
    rpm = rotors.RotorGetRPM(0);
    target = rotors.RotorGetRPMTarget(0);
   }
   if (target <= 0 || rpm >= target * 0.9 || now - m_fStarted > SPOOL_MAX_SECONDS * 1000)
   {
    m_bSpooled = true;
    PrintFormat("[ACR] SUPPORT helicopter spooled up after %1 s (rotor %2 of %3 rpm)", (int)((now - m_fStarted) / 1000), (int)rpm, (int)target);
   }
   else
   {
    Physics still = m_Vehicle.GetPhysics();
    if (still) { still.SetVelocity(vector.Zero); still.SetAngularVelocity(vector.Zero); }
    return true;
   }
  }
  return false;
 }

 // The support mission itself: rocket run, gunship or door gunners.
 protected bool FlySupport(ACR_Supports supports, float dt, float now)
 {

  vector flat = m_Target - m_Pos;
  flat[1] = 0;
  float distance = flat.Length();
  vector toTarget = flat.Normalized();
  float yawToTarget = Math.Atan2(toTarget[0], toTarget[2]) * Math.RAD2DEG;
  vector desired = m_Dir;
  float bodyOffset = 0; // nose = track + offset
  float speed = SPEED_RUN;
  float altitude = ALT_TRANSIT;
  if (m_bLoiter)
  {
   speed = SPEED_LOITER;
   if (m_iPhase == 0 && distance < LOITER_RADIUS + 60)
   {
    m_iPhase = 1;
    m_fOrbitEnd = now + m_fLoiterMs;
    m_fNextSideSwap = now + DOOR_SIDE_SWAP_MS;
    supports.m_Manager.NotifyFaction(m_sFaction, StationName() + " on station.");
    PrintFormat("[ACR] SUPPORT gunship on station at %1", m_Pos);
   }
   if (m_iPhase == 1 && now > m_fOrbitEnd)
   {
    m_iPhase = 2;
    m_ExitDir = -toTarget;
    supports.m_Manager.NotifyFaction(m_sFaction, StationName() + " leaving station.");
    if (m_Home) { m_bReturning = true; m_fSpeed = SPEED_LOITER; }
   }
   if (m_iPhase == 0) desired = toTarget;
   else if (m_iPhase == 1)
   {
    altitude = ALT_LOITER;
    // Gunner on the working side down or out of ammo: turn the other side in.
    if (!m_bPasses && now >= m_fNextSideCheck)
    {
     m_fNextSideCheck = now + 2000;
     // Door gunners: turn the other door in every 30 s, so both gunners get to fire
     // (the one on the outside of the orbit never could, 1 Oct live test).
     bool turnAbout = m_bDoorGuns && now >= m_fNextSideSwap && SideCanFire(-m_fSideBearing);
     if (turnAbout || (!SideCanFire(m_fSideBearing) && SideCanFire(-m_fSideBearing)))
     {
      m_fSideBearing = -m_fSideBearing;
      m_fNextSideSwap = now + DOOR_SIDE_SWAP_MS;
      PrintFormat("[ACR] SUPPORT gunship switching to its other side (bearing %1)", m_fSideBearing);
     }
    }
    if (m_bPasses)
    {
     // Attack passes: run in, overfly, extend 650 m, come round again.
     if (!m_bPassOutbound && distance < 90) m_bPassOutbound = true;
     if (m_bPassOutbound && distance > 650) m_bPassOutbound = false;
     if (m_bPassOutbound) desired = Heading(m_fYaw);
     else desired = toTarget;
    }
    else
    {
     // Pylon turn: circle so the target stays on the gun side.
     // Target on the left (negative bearing) = circle with the left side inward.
     float side = -1;
     if (m_fSideBearing < 0) side = 1;
     vector tangent = Vector(toTarget[2], 0, -toTarget[0]) * side;
     desired = (tangent + toTarget * ((distance - LOITER_RADIUS) / LOITER_RADIUS * 1.5)).Normalized();
     // Point the nose so the target sits exactly at the guns' best bearing.
     bodyOffset = Wrap(yawToTarget - m_fSideBearing - Math.Atan2(desired[0], desired[2]) * Math.RAD2DEG);
     bodyOffset = Math.Clamp(bodyOffset, -35, 35);
    }
   }
   else
   {
    desired = m_ExitDir;
    altitude = ALT_EGRESS;
   }
  }
  // Rocket run: once past the firing window, climb out.
  else if (vector.Dot(m_Target - m_Pos, m_Dir) < 250) altitude = ALT_EGRESS;
  altitude += m_iStack * STACK_STEP;
  vector away = Separation(supports);
  if (away != vector.Zero) desired = (desired + away).Normalized();

  // Track: turn toward the desired heading at a helicopter-like rate (Fly banks into it).
  TurnToward(Math.Atan2(desired[0], desired[2]) * Math.RAD2DEG, 24, dt);
  // Body: the nose follows the track plus the pylon-turn offset.
  float bodyDelta = Wrap(m_fYaw + bodyOffset - m_fBodyYaw);
  m_fBodyYaw = Wrap(m_fBodyYaw + Math.Clamp(bodyDelta, -40 * dt, 40 * dt));

  // Off a helipad it picks up speed instead of jumping to it.
  if (m_Home)
  {
   m_fSpeed += (speed - m_fSpeed) * Math.Min(1, dt * 0.4);
   speed = Math.Max(m_fSpeed, 3);
  }
  vector forward = Heading(m_fYaw);
  vector nose = Heading(m_fBodyYaw);
  vector previous = m_Pos;
  m_Pos = m_Pos + forward * (speed * dt);
  float wantedY = SafeAltitude(m_Pos, forward, altitude);
  // Vertical speed eases toward what the terrain asks for (it used to jump straight
  // to the limit, so the flight looked rigid over hills).
  float maxUp = 12;
  if (altitude >= ALT_EGRESS) maxUp = 16;
  float wantedClimb = Math.Clamp((wantedY - previous[1]) * 0.5, -10, maxUp);
  m_fClimb += (wantedClimb - m_fClimb) * Math.Min(1, dt * 1.2);
  m_Pos[1] = previous[1] + m_fClimb * dt;
  float floorY = SafeAltitude(m_Pos, forward, 20);
  if (m_Pos[1] < floorY) { m_Pos[1] = previous[1] + Math.Min(floorY - previous[1], 20 * dt); m_fClimb = Math.Max(m_fClimb, 0); }
  // Fly adds cruise and acceleration pitch; this is the climb and the rocket dive.
  float pitch = Math.Clamp(m_fClimb * 0.2, -2, 2);
  if (now < m_fDiveUntil) pitch -= 4;
  if (now >= m_fNextAltLog)
  {
   m_fNextAltLog = now + 5000;
   PrintFormat("[ACR] SUPPORT heli height %1 m above ground, climb %2 m/s, phase %3, %4 m from target", (int)(m_Pos[1] - GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2])), (int)m_fClimb, m_iPhase, (int)distance);
  }

  // Rockets: gun run fires one pass on the way in; loiter fires salvos at
  // enemies near the target whenever the nose points at them.
  if (m_iRockets > 0 && now >= m_fNextRocket)
  {
   vector aim;
   bool shoot;
   if (!m_bLoiter)
   {
    float along = vector.Dot(m_Target - m_Pos, m_Dir);
    shoot = along < 650 && along > 280;
    if (shoot)
    {
     // Spread the salvo over the enemies seen around the target; with none seen,
     // walk it over the whole area.
     if (now >= m_fNextEnemyScan)
     {
      m_fNextEnemyScan = now + 500;
      m_aEnemies.Clear();
      supports.FindEnemies(m_sFaction, m_Target, RUN_TARGET_RADIUS, m_aEnemies);
     }
     IEntity runTarget;
     if (!m_aEnemies.IsEmpty()) runTarget = m_aEnemies[m_iRockets % m_aEnemies.Count()];
     m_bAimedAtEnemy = runTarget != null;
     if (runTarget) aim = runTarget.GetOrigin() + Vector(Math.RandomFloat(-6, 6), 1, Math.RandomFloat(-6, 6));
     else aim = m_Target + Vector(Math.RandomFloat(-RUN_SPREAD, RUN_SPREAD), 3, Math.RandomFloat(-RUN_SPREAD, RUN_SPREAD));
    }
   }
   else if (m_iSalvoLeft > 0)
   {
    // A salvo, once started, is fired complete at the same spot (no odd pair of rockets).
    shoot = true;
    aim = m_SalvoAim + Vector(Math.RandomFloat(-8, 8), 0, Math.RandomFloat(-8, 8));
   }
   else if (m_iPhase == 1)
   {
    IEntity enemy = supports.FindEnemyNear(m_sFaction, m_Target, 220);
    vector enemyFlat;
    if (enemy) enemyFlat = enemy.GetOrigin() - m_Pos;
    enemyFlat[1] = 0;
    shoot = enemy != null && vector.Dot(enemyFlat.Normalized(), nose) > 0.9 && enemyFlat.Length() < 650 && enemyFlat.Length() > 150;
    if (enemy) aim = enemy.GetOrigin() + Vector(Math.RandomFloat(-6, 6), 2, Math.RandomFloat(-6, 6));
    if (shoot)
    {
     m_iSalvoLeft = Math.Min(4, m_iRockets);
     m_SalvoAim = enemy.GetOrigin() + Vector(0, 2, 0);
    }
   }
   if (shoot)
   {
    aim[1] = Math.Max(aim[1], GetGame().GetWorld().GetSurfaceY(aim[0], aim[2]) + 0.5);
    vector right = Vector(nose[2], 0, -nose[0]);
    float sideOffset = 1.6;
    if (m_iRockets % 2 == 0) sideOffset = -1.6;
    supports.FireProjectile(m_Rocket, m_Pos + nose * 6 + right * sideOffset - Vector(0, 1.2, 0), aim, forward * speed);
    m_iRockets--;
    PrintFormat("[ACR] SUPPORT heli rocket, %1 left, range %2, at an enemy %3", m_iRockets, vector.Distance(m_Pos, aim), m_bAimedAtEnemy);
    m_fNextRocket = now + 280;
    m_fDiveUntil = now + 1200;
    if (m_bLoiter)
    {
     m_iSalvoLeft--;
     if (m_iSalvoLeft <= 0) m_fNextRocket = now + 8000;
    }
   }
  }

  // Gunnery: engage enemies near the target once within reach.
  vector heliVelocity = forward * speed;
  if (m_bLoiter && m_iPhase != 2 && distance < 700)
  {
   if (now >= m_fNextEnemyScan)
   {
    m_fNextEnemyScan = now + 500;
    m_aEnemies.Clear();
    supports.FindEnemies(m_sFaction, m_Target, 320, m_aEnemies);
    // Targets of opportunity around the helicopter itself: the gunner on the outside
    // of the orbit faces away from the target and had nothing to shoot (2 Oct live).
    supports.FindEnemies(m_sFaction, m_Pos, OPPORTUNITY_RADIUS, m_aEnemies);
   }
   foreach (ACR_HeliGun shooter : m_Guns) shooter.Update(m_Vehicle, heliVelocity, m_Target, m_aEnemies, now);
  }
  else foreach (ACR_HeliGun idle : m_Guns) idle.StopFire();

  // Gun report (loiter): which guns can reach the nearest enemy, and rounds fired.
  if (m_bLoiter && m_iPhase == 1 && now >= m_fNextGunLog)
  {
   m_fNextGunLog = now + 5000;
   IEntity nearest = supports.FindEnemyNear(m_sFaction, m_Target, 250);
   string report = "";
   foreach (ACR_HeliGun gun : m_Guns)
   {
    int ammo = gun.Ammo();
    int fired = 0;
    if (gun.m_iLastAmmo >= 0 && ammo >= 0) fired = gun.m_iLastAmmo - ammo;
    gun.m_iLastAmmo = ammo;
    bool reach = nearest != null && gun.Covers(nearest.GetOrigin());
    report += string.Format(" | %1 manned=%2 target=%3 inArc=%4 fired=%5", gun.m_sName, ACR_Respawn.IsAlive(gun.Occupant()), gun.m_Target != null, reach, fired);
   }
   PrintFormat("[ACR] SUPPORT gunship guns%1", report);
  }

  vector velocity = forward * speed;
  if (dt > 0) velocity[1] = (m_Pos[1] - previous[1]) / dt;
  Fly(velocity, dt, pitch);

  // From a helipad: turn for home once clear of the target.
  if (m_Home && !m_bLoiter && vector.Dot(m_Target - m_Pos, m_Dir) < -600)
  {
   m_bReturning = true;
   m_fSpeed = speed;
   PrintFormat("[ACR] SUPPORT %1 returning to its pad", m_Home.m_sName);
  }
  if (m_Home) return true;
  if (!m_bLoiter) return vector.Dot(m_Target - m_Pos, m_Dir) > -2000;
  if (m_iPhase == 2) return distance < 2000;
  return true;
 }

 // Log lines: the pad helicopter's own name, else the kind.
 string LogName()
 {
  if (m_Home) return m_Home.m_sName;
  return StationName();
 }

 string StationName()
 {
  if (m_bDoorGuns) return "Door gunners";
  return "Gunship";
 }

 override void Finish()
 {
  if (!m_Vehicle || m_bDestroyed) return;
  // A stationed helicopter is kept: if it never made it down (time ran out), it is put on its pad.
  if (m_Home)
  {
   if (!m_bLanded)
   {
    PrintFormat("[ACR] SUPPORT %1 ran out of time on its way back: set down on its pad", m_Home.m_sName);
    ACR_Manager manager = ACR_Manager.GetInstance();
    if (manager && manager.m_Supports) Touchdown(manager.m_Supports);
   }
   return;
  }
  array<IEntity> crew = {};
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (compartments)
  {
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartments(slots);
   foreach (BaseCompartmentSlot slot : slots) if (slot.GetOccupant()) crew.Insert(slot.GetOccupant());
  }
  // Only AI goes with it: a player aboard (he climbed into a lift in the air or a seat nobody
  // emptied) is put out rather than deleted with his loadout.
  foreach (IEntity member : crew)
  {
   if (EntityUtils.IsPlayer(member))
   {
    ChimeraCharacter player = ChimeraCharacter.Cast(member);
    CompartmentAccessComponent access;
    if (player) access = player.GetCompartmentAccessComponent();
    if (access) access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
    PrintFormat("[ACR] SUPPORT helicopter left the area with a player aboard: put out");
    continue;
   }
   SCR_EntityHelper.DeleteEntityAndChildren(member);
  }
  SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
  Print("[ACR] SUPPORT helicopter left the area");
 }
}

// A support's danger area, shown on the map to that side until it is over.
class ACR_SupportZone : Managed
{
 string m_sFaction;
 vector m_Pos;
 float m_fRadius;
 string m_sName;
 float m_fImpactAt;
 float m_fEndAt;
}

class ACR_PendingLaunch : Managed
{
 IEntity m_Entity;
 vector m_Dir;
 vector m_Velocity;
 int m_iFrame;
}

class ACR_Supports : Managed
{
 static const ResourceName MORTAR_SMALL = "{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Small.et";
 static const ResourceName MORTAR_LARGE = "{1DDF7187608968F2}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Large.et";
 static const ResourceName MORTAR_CREEPING = "{F5FAFFDBC271088C}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Creeping.et";
 static const ResourceName MORTAR_SMOKE = "{2FDF3089DF6EC422}PrefabsEditable/EffectsModules/Mortar/Smoke/EffectModule_Zoned_MortarBarrage_Smoke_Medium.et";
 static const ResourceName FLARE = "{AA7F04F403FD2E00}PrefabsEditable/EffectsModules/Flares/EffectModule_Mortar_Flare_Illumination.et";
 static const ResourceName BOMB = "{72BEEF40AF179763}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et";
 // Howitzer battery (virtual: no guns on the map, no range limit).
 static int BATTERY_DELAY_MIN = 75;
 static int BATTERY_DELAY_MAX = 120;
 static const int BATTERY_SALVOS = 3;
 static const int BATTERY_ROUNDS = 6;
 static const int BATTERY_SALVO_SECONDS = 10;
 static const float BATTERY_RADIUS = 120;
 static const float BATTERY_DROP_HEIGHT = 700;  // the shell falls from here (~3 s): its travel sound is the incoming whistle
 static const int BATTERY_FALL_SECONDS = 3;
 static int s_iBatteryImpacts;                  // tests
 static ref array<vector> s_aBatteryImpacts = {};
 protected ref array<IEntity> m_BatteryShells = {};
 protected ref array<vector> m_BatteryLast = {};
 protected ref array<vector> m_BatteryAim = {};
 protected ref array<float> m_BatteryDeadline = {};
 static const ResourceName HELI_WEST = "{21E9A875C0A3C409}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HE.et";
 static const ResourceName HELI_EAST = "{3C6B3ED0C3AC30D5}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_gunship_HE.et";
 static const ResourceName HELI_TRANSPORT_WEST = "{DDDD9B51F1234DF3}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed.et";
 static const ResourceName HELI_TRANSPORT_EAST = "{7BD282AF716ED639}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed.et";
 static const ResourceName ROCKET_WEST = "{072A755D5CB85D47}Prefabs/Weapons/Ammo/Ammo_Rocket_Hydra70_HE_M229.et";
 static const ResourceName ROCKET_EAST = "{EF17BED6DCEE4DE4}Prefabs/Weapons/Ammo/Ammo_Rocket_S5_FRAG_S5MO.et";

 ACR_Manager m_Manager;
 ref array<IEntity> m_Vehicles = {};
 protected ref ACR_HeliStation m_Station;
 // Helipads and the helicopters stationed on them (ACR_Helipads.c).
 ACR_HeliStation Station()
 {
  if (!m_Station) m_Station = new ACR_HeliStation(this);
  return m_Station;
 }
 protected ref array<ref ACR_SupportRun> m_Runs = {};
 void AddRun(ACR_SupportRun run) { m_Runs.Insert(run); }

 void Helicopters(array<ACR_HeliRun> result)
 {
  result.Clear();
  foreach (ACR_SupportRun run : m_Runs)
  {
   ACR_HeliRun heli = ACR_HeliRun.Cast(run);
   if (heli) result.Insert(heli);
   ACR_AirLift lift = ACR_AirLift.Cast(run);
   if (lift)
    foreach (ACR_LiftHeli carrier : lift.m_Helis)
     if (carrier.m_Vehicle && !carrier.m_bDestroyed && !carrier.m_bAway) result.Insert(carrier);
  }
 }

 // Test hook: the running helicopter support of a side (null if none).
 ACR_HeliRun TestHeliRun(string faction)
 {
  foreach (ACR_SupportRun run : m_Runs)
  {
   ACR_HeliRun heli = ACR_HeliRun.Cast(run);
   if (heli && heli.m_sFaction == faction) return heli;
  }
  return null;
 }
 protected ref array<ref ACR_PendingLaunch> m_Launches = {};
 protected int m_iFrame;
 ref array<ref ACR_SupportZone> m_Zones = {};

 void AddZone(string factionKey, vector pos, float radius, string name, int delaySeconds, int durationSeconds)
 {
  ACR_SupportZone zone = new ACR_SupportZone();
  float now = GetGame().GetWorld().GetWorldTime();
  zone.m_sFaction = factionKey;
  zone.m_Pos = pos;
  zone.m_fRadius = radius;
  zone.m_sName = name;
  zone.m_fImpactAt = now + delaySeconds * 1000;
  zone.m_fEndAt = zone.m_fImpactAt + durationSeconds * 1000;
  m_Zones.Insert(zone);
 }

 static float ZoneRadius(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 30;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 60;
   case ACR_ESupportType.MORTAR_CREEPING: return 80;
   case ACR_ESupportType.MORTAR_SMOKE: return 40;
   case ACR_ESupportType.ILLUMINATION: return 90;
   case ACR_ESupportType.ROCKET_STRIKE: return 70;
   case ACR_ESupportType.BOMB_RUN: return 70;
   case ACR_ESupportType.HELI_GUN_RUN: return 40;
   case ACR_ESupportType.HELI_LOITER: return ACR_HeliRun.LOITER_RADIUS;
   case ACR_ESupportType.HELI_DOOR_GUNS: return ACR_HeliRun.LOITER_RADIUS;
   case ACR_ESupportType.ARTILLERY_BATTERY: return BATTERY_RADIUS + 30;
  }
  return 50;
 }
 static int ZoneDuration(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.ILLUMINATION: return 80;
   case ACR_ESupportType.MORTAR_SMOKE: return 60;
   case ACR_ESupportType.HELI_LOITER: return 100;
   case ACR_ESupportType.HELI_DOOR_GUNS: return 130;
   case ACR_ESupportType.MORTAR_CREEPING: return 50;
   case ACR_ESupportType.ARTILLERY_BATTERY: return BATTERY_SALVOS * BATTERY_SALVO_SECONDS + 10;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 40;
   case ACR_ESupportType.MORTAR_HE_SMALL: return 25;
  }
  return 15;
 }

 void ACR_Supports(ACR_Manager manager) { m_Manager = manager; }

 static string DefaultName(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return "Mortars: HE (small)";
   case ACR_ESupportType.MORTAR_HE_LARGE: return "Mortars: HE (large)";
   case ACR_ESupportType.MORTAR_CREEPING: return "Mortars: creeping barrage";
   case ACR_ESupportType.MORTAR_SMOKE: return "Mortars: smoke screen";
   case ACR_ESupportType.ILLUMINATION: return "Illumination flares";
   case ACR_ESupportType.ROCKET_STRIKE: return "Air: rocket strike";
   case ACR_ESupportType.BOMB_RUN: return "Air: bomb run";
   case ACR_ESupportType.HELI_GUN_RUN: return "Helicopter: rocket run";
   case ACR_ESupportType.HELI_LOITER: return "Helicopter: gunship (90 s)";
   case ACR_ESupportType.HELI_DOOR_GUNS: return "Helicopter: door gunners (120 s)";
   case ACR_ESupportType.ARTILLERY_BATTERY: return "Artillery: howitzer battery";
   case ACR_ESupportType.AIR_LIFT: return "Helicopter: air lift";
  }
  return "Support";
 }
 static int DefaultCost(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 10;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 25;
   case ACR_ESupportType.MORTAR_CREEPING: return 25;
   case ACR_ESupportType.MORTAR_SMOKE: return 6;
   case ACR_ESupportType.ILLUMINATION: return 4;
   case ACR_ESupportType.ROCKET_STRIKE: return 25;
   case ACR_ESupportType.BOMB_RUN: return 35;
   case ACR_ESupportType.HELI_GUN_RUN: return 30;
   case ACR_ESupportType.HELI_LOITER: return 45;
   case ACR_ESupportType.HELI_DOOR_GUNS: return 20;
   case ACR_ESupportType.ARTILLERY_BATTERY: return 60;
   case ACR_ESupportType.AIR_LIFT: return 40;
  }
  return 20;
 }
 static int DefaultCooldown(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 90;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 180;
   case ACR_ESupportType.MORTAR_CREEPING: return 180;
   case ACR_ESupportType.MORTAR_SMOKE: return 60;
   case ACR_ESupportType.ILLUMINATION: return 45;
   case ACR_ESupportType.ROCKET_STRIKE: return 180;
   case ACR_ESupportType.BOMB_RUN: return 240;
   case ACR_ESupportType.HELI_GUN_RUN: return 180;
   case ACR_ESupportType.HELI_LOITER: return 300;
   case ACR_ESupportType.HELI_DOOR_GUNS: return 150;
   case ACR_ESupportType.ARTILLERY_BATTERY: return 300;
   case ACR_ESupportType.AIR_LIFT: return 240;
  }
  return 120;
 }
 // Seconds from the call until rounds land / aircraft arrive (helicopters: flight time).
 // Mortar missions take a realistic, somewhat random time: the call goes up the
 // chain, the section lays the tubes, then the rounds' time of flight.
 static int DefaultDelay(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return Math.RandomIntInclusive(45, 90);
   case ACR_ESupportType.MORTAR_HE_LARGE: return Math.RandomIntInclusive(50, 95);
   case ACR_ESupportType.MORTAR_CREEPING: return Math.RandomIntInclusive(60, 110);
   case ACR_ESupportType.MORTAR_SMOKE: return Math.RandomIntInclusive(40, 75);
   case ACR_ESupportType.ILLUMINATION: return Math.RandomIntInclusive(30, 60);
   case ACR_ESupportType.ARTILLERY_BATTERY: return Math.RandomIntInclusive(BATTERY_DELAY_MIN, BATTERY_DELAY_MAX);
   case ACR_ESupportType.ROCKET_STRIKE: return 15;
   case ACR_ESupportType.BOMB_RUN: return 20;
  }
  return 0;
 }

 // Every support type, used when a faction's settings list none.
 static void BuildDefaults(array<ref ACR_SupportEntry> entries)
 {
  for (int type = ACR_ESupportType.MORTAR_HE_SMALL; type <= ACR_ESupportType.AIR_LIFT; type++)
  {
   ACR_SupportEntry entry = new ACR_SupportEntry();
   entry.m_eType = type;
   entry.m_iCost = -1;
   entry.m_iAICost = -1;
   entry.m_iCooldown = -1;
   entry.Resolve();
   // With RHS loaded, both sides' bomb run is an audible FA-18 flyover (Jacob, 9 Oct).
   if (type == ACR_ESupportType.BOMB_RUN && HasPrefab(FA18_FLYOVER))
   {
    entry.m_eBombRunDelivery = ACR_EBombRunDelivery.FLYOVER;
    entry.m_sBombRunPrefab = FA18_FLYOVER;
   }
   entries.Insert(entry);
  }
 }

 static const ResourceName FA18_FLYOVER = "{76571A125EF9ECD7}Prefabs/Vehicles/Airplanes/FA18A/FA18A_Mesh.et";

 static bool HasPrefab(ResourceName prefab)
 {
  Resource resource = Resource.Load(prefab);
  return resource && resource.IsValid();
 }

 // ("USSR" contains "US": the old Contains test gave the Soviets the UH-1H and Hydras.)
 static bool IsWestern(string factionKey)
 {
  if (factionKey == "RHS_AFRF") return false;
  if (factionKey.StartsWith("USSR")) return false;
  return factionKey == "US" || factionKey.Contains("US") || factionKey == "NATO" || factionKey == "BLUFOR";
 }

 // Groups that have crewed a support vehicle. A crew that bails out (a helicopter shot up on
 // its pad) is no longer seated, and must still never be taken for a squad (4 Oct live: the AI
 // commander ordered a support helicopter's crew about).
 ref array<SCR_AIGroup> m_CrewGroups = {};
 ref map<IEntity, SCR_AIGroup> m_CrewOf = new map<IEntity, SCR_AIGroup>();

 void RememberCrew(IEntity vehicle)
 {
  if (!vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  foreach (BaseCompartmentSlot slot : slots)
  {
   // Passengers (an air lift's squad) are not crew.
   if (slot.GetType() == ECompartmentType.CARGO) continue;
   IEntity occupant = slot.GetOccupant();
   if (!occupant || !ACR_Respawn.IsAlive(occupant)) continue;
   AIControlComponent control = AIControlComponent.Cast(occupant.FindComponent(AIControlComponent));
   if (!control || !control.GetControlAIAgent()) continue;
   SCR_AIGroup group = SCR_AIGroup.Cast(control.GetControlAIAgent().GetParentGroup());
   if (!group) continue;
   m_CrewOf.Set(vehicle, group);
   if (m_CrewGroups.Contains(group)) continue;
   m_CrewGroups.Insert(group);
   PrintFormat("[ACR] SUPPORT crew group remembered for %1", vehicle.GetPrefabData().GetPrefabName());
  }
 }

 SCR_AIGroup CrewOf(IEntity vehicle)
 {
  SCR_AIGroup group;
  m_CrewOf.Find(vehicle, group);
  return group;
 }

 bool IsSupportGroup(SCR_AIGroup group)
 {
  if (m_CrewGroups.Contains(group)) return true;
  if (m_Vehicles.IsEmpty()) return false;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.GetCompartmentAccessComponent()) continue;
   BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
   if (slot && slot.GetType() != ECompartmentType.CARGO && m_Vehicles.Contains(slot.GetVehicle())) return true;
  }
  return false;
 }

 // Called by the commander (or AI). Returns the feedback message.
 // squad: the squad to fly (air lift only).
 string Call(string factionKey, int index, vector target, bool byAI = false, int callerPlayerId = -1, ACR_Squad squad = null, ResourceName finalPrefab = "", vector finalTarget = vector.Zero, float finalRadius = -1, string finalOrder = "", int finalKind = 0)
 {
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(factionKey);
  if (index < 0 || index >= data.m_aSupports.Count()) return "Unknown support";
  ACR_SupportEntry entry = data.m_aSupports[index];
  float now = GetGame().GetWorld().GetWorldTime();
  bool waveSlot = entry.m_eType == ACR_ESupportType.AIR_LIFT && ACR_AirLift.WaveSlotAvailable(factionKey, now);
  if (now < entry.m_fReadyAt && !waveSlot) return string.Format("%1 is not ready (%2 s)", entry.m_sName, Math.Ceil((entry.m_fReadyAt - now) / 1000));
  // Mortar supports are fired by a real mortar team (ACR_Mortars): nothing is spent unless one can.
  if (ACR_Mortars.IsMortarType(entry.m_eType)) return CallMortars(factionKey, entry, target, byAI);
  if (entry.m_eType == ACR_ESupportType.AIR_LIFT) return ACR_AirLift.Call(this, factionKey, entry, target, byAI, squad, finalPrefab, finalTarget, finalRadius, finalOrder, finalKind);
  if (entry.m_eType == ACR_ESupportType.BOMB_RUN && entry.m_eBombRunDelivery != ACR_EBombRunDelivery.VIRTUAL)
  {
   if (entry.m_sBombRunPrefab.IsEmpty()) return "Choose a bomb-run prefab in this faction's support settings";
   Resource bombRunResource = Resource.Load(entry.m_sBombRunPrefab);
   if (!bombRunResource.IsValid()) return "Bomb-run prefab could not be loaded";
  }
  // With helipads in the world, helicopter supports fly a helicopter bought for a pad.
  ACR_StationedHeli home;
  if (ACR_HeliStation.IsHeliType(entry.m_eType) && ACR_HeliStation.UsesPads(factionKey))
  {
   home = Station().ReadyFor(factionKey, ACR_HeliStation.NeedsGunship(entry.m_eType), target);
   if (!home)
   {
    string why = Station().WhyNot(factionKey, ACR_HeliStation.NeedsGunship(entry.m_eType));
    PrintFormat("[ACR] SUPPORT %1 %2 refused: %3", factionKey, entry.m_sName, why);
    return why;
   }
  }
  if (!m_Manager.m_Bases.Spend(factionKey, entry.m_iCost)) return string.Format("Not enough RP for %1 (%2 needed)", entry.m_sName, entry.m_iCost);
  entry.m_fReadyAt = now + entry.m_iCooldown * 1000;
  target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
  vector dir = ApproachDirection(factionKey, target);
  string where = ACR_Manager.Grid(target);
  int delay = DefaultDelay(entry.m_eType);
  PrintFormat("[ACR] SUPPORT %1 %2 at %3 (delay %4 s)", factionKey, entry.m_sName, target, delay);
  switch (entry.m_eType)
  {
   case ACR_ESupportType.ROCKET_STRIKE:
   {
    ACR_RocketStrikeRun strike = new ACR_RocketStrikeRun();
    strike.m_Target = target;
    strike.m_Dir = dir;
    strike.m_Pos = target - dir * (950 + 110 * delay);
    strike.m_Pos[1] = target[1] + 170;
    strike.m_Rocket = RocketFor(factionKey);
    m_Runs.Insert(strike);
    break;
   }
   case ACR_ESupportType.ARTILLERY_BATTERY:
   {
    // Three salvos of six over a wide area, each round a falling shell (its own incoming
    // whistle on every machine) that lands with a heavy blast.
    ResourceName shell = BatteryShellFor(factionKey);
    for (int salvo = 0; salvo < BATTERY_SALVOS; salvo++)
    {
     for (int gun = 0; gun < BATTERY_ROUNDS; gun++)
     {
      float shellAngle = Math.RandomFloat(0, Math.PI2);
      float shellSpread = BATTERY_RADIUS * Math.Sqrt(Math.RandomFloat(0, 1));
      vector shellAt = target + Vector(Math.Cos(shellAngle) * shellSpread, 0, Math.Sin(shellAngle) * shellSpread);
      int shellMs = (delay - BATTERY_FALL_SECONDS) * 1000 + salvo * BATTERY_SALVO_SECONDS * 1000 + gun * Math.RandomIntInclusive(150, 600);
      if (shellMs < 0) shellMs = 0;
      GetGame().GetCallqueue().CallLater(FireBatteryRound, shellMs, false, shell, shellAt);
     }
    }
    break;
   }
   case ACR_ESupportType.BOMB_RUN:
    if (!StartBombRun(entry, target, dir, delay, callerPlayerId))
    {
     m_Manager.m_Bases.Refund(factionKey, entry.m_iCost);
     entry.m_fReadyAt = now;
     return "Bomb-run prefab could not be spawned";
    }
    break;
   case ACR_ESupportType.HELI_GUN_RUN:
   case ACR_ESupportType.HELI_LOITER:
   case ACR_ESupportType.HELI_DOOR_GUNS:
   {
    ACR_HeliRun heli = new ACR_HeliRun();
    heli.m_sFaction = factionKey;
    heli.m_bDoorGuns = entry.m_eType == ACR_ESupportType.HELI_DOOR_GUNS;
    heli.m_bLoiter = entry.m_eType == ACR_ESupportType.HELI_LOITER || heli.m_bDoorGuns;
    if (heli.m_bDoorGuns) heli.m_fLoiterMs = 120000;
    heli.m_Target = target;
    heli.m_Dir = dir;
    heli.m_Rocket = RocketFor(factionKey);
    ResourceName helicopter = HelicopterFor(factionKey);
    if (heli.m_bDoorGuns) helicopter = TransportHelicopterFor(factionKey);
    bool started;
    if (home) started = heli.StartFromPad(this, home, now);
    else started = heli.Start(this, helicopter, now);
    if (!started)
    {
     // Nothing flew: give the RP and the cooldown back.
     m_Manager.m_Bases.Refund(factionKey, entry.m_iCost);
     entry.m_fReadyAt = now;
     return "No helicopter available";
    }
    // Each helicopter in the air gets its own height band.
    array<ACR_HeliRun> flying = {};
    Helicopters(flying);
    array<int> taken = {};
    foreach (ACR_HeliRun up : flying) if (up.m_Vehicle && !up.m_bDestroyed) taken.Insert(up.m_iStack);
    while (taken.Contains(heli.m_iStack) && heli.m_iStack < 2) heli.m_iStack++;
    m_Runs.Insert(heli);
    delay = (int)(vector.DistanceXZ(heli.m_Start, target) / ACR_HeliRun.SPEED_RUN + ACR_HeliRun.SPOOL_MAX_SECONDS * 0.9);
    if (home)
    {
     Station().Launched(home);
     delay += (int)(ACR_HeliRun.LIFT_HEIGHT / ACR_HeliRun.LIFT_RATE) + 3;
    }
    break;
   }
  }
  AddZone(factionKey, target, ZoneRadius(entry.m_eType), entry.m_sName, delay, ZoneDuration(entry.m_eType));
  // Long waits are only announced roughly (to the nearest 15 s).
  string eta = string.Format("%1 s", delay);
  if (delay >= 30) eta = string.Format("about %1 s", (int)Math.Round(delay / 15.0) * 15);
  string message = string.Format("%1 on %2, ETA %3.", entry.m_sName, where, eta);
  if (!byAI) m_Manager.NotifyFaction(factionKey, message);
  return message;
 }

 protected bool StartBombRun(ACR_SupportEntry entry, vector target, vector dir, int delay, int callerPlayerId)
 {
  if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.EDITABLE_CALL_IN)
  {
   // RHS's call-in container spawns its prefab, then invokes this same engine hook so
   // editable entities can initialize their slots, timing and warhead instigator.
   GetGame().GetCallqueue().CallLater(SpawnBombRunCallIn, delay * 1000, false, entry.m_sBombRunPrefab, target, dir, callerPlayerId);
   return true;
  }

  if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.FLYOVER)
  {
   Resource resource = Resource.Load(entry.m_sBombRunPrefab);
   if (!resource.IsValid()) return false;
   float speed = Math.Clamp(entry.m_fBombRunSpeed, 20, 400);
   vector flat = dir;
   flat[1] = 0;
   flat.Normalize();
   vector start = target - flat * (speed * delay);
   // Overhead at the set height, and never lower than 150 m over a hill on the way in or
   // the first 2.5 km out.
   float altitude = target[1] + Math.Clamp(entry.m_fBombRunAltitude, 30, 1000);
   float pathLength = speed * delay + 2500;
   for (float d = 0; d <= pathLength; d += 100)
   {
    vector point = start + flat * d;
    altitude = Math.Max(altitude, GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 150);
   }
   start[1] = altitude;
   float seconds = delay + 2500 / speed;
   SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
   if (mode) mode.ACR_BroadcastFlyover(entry.m_sBombRunPrefab, start, flat, speed, altitude, seconds);
   if (GetGame().GetPlayerController()) ACR_LocalFlyover.Start(entry.m_sBombRunPrefab, start, flat, speed, altitude, seconds);
   PrintFormat("[ACR] SUPPORT flyover %1 at %2 m (%3 m over the target), %4 m/s", FilePath.StripPath(entry.m_sBombRunPrefab), Math.Round(altitude), Math.Round(altitude - target[1]), speed);
  }

  int count = Math.Clamp(entry.m_iBombRunBombs, 1, 12);
  float spacing = Math.Clamp(entry.m_fBombRunSpacing, 1, 500);
  float scatter = Math.Clamp(entry.m_fBombRunScatter, 0, 100);
  vector right = Vector(dir[2], 0, -dir[0]);
  int interval = Math.Round(spacing / Math.Clamp(entry.m_fBombRunSpeed, 20, 400) * 1000);
  if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.VIRTUAL)
  {
   spacing = 28;
   interval = 350;
  }
  for (int bomb = 0; bomb < count; bomb++)
  {
   float along = (bomb - (count - 1) * 0.5) * spacing;
   vector impact = target + dir * along + right * Math.RandomFloat(-scatter, scatter);
   int impactDelay = delay * 1000 + bomb * interval;
   if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.FLYOVER)
    impactDelay = Math.Max(0, delay * 1000 + Math.Round(along / Math.Clamp(entry.m_fBombRunSpeed, 20, 400) * 1000));
   GetGame().GetCallqueue().CallLater(Explode, impactDelay, false, impact);
  }
  return true;
 }

 protected void SpawnBombRunCallIn(ResourceName prefab, vector target, vector dir, int callerPlayerId)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid())
  {
   PrintFormat("[ACR] SUPPORT bomb-run call-in prefab unavailable: %1", prefab);
   return;
  }
  vector transform[4];
  Math3D.AnglesToMatrix(Vector(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG, 0, 0), transform);
  transform[3] = target;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  for (int i = 0; i < 4; i++) parameters.Transform[i] = transform[i];
  IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!entity)
  {
   PrintFormat("[ACR] SUPPORT bomb-run call-in spawn failed: %1", prefab);
   return;
  }
  SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(entity.FindComponent(SCR_EditableEntityComponent));
  if (!editable)
  {
   PrintFormat("[ACR] SUPPORT bomb-run prefab has no editable call-in hook: %1", prefab);
   SCR_EntityHelper.DeleteEntityAndChildren(entity);
   return;
  }
  SCR_EditableEntityComponent parent;
  editable.EOnEditorPlace(parent, null, 0, false, callerPlayerId);
 }

 protected string CallMortars(string factionKey, ACR_SupportEntry entry, vector target, bool byAI)
 {
  if (m_Manager.m_Bases.GetRP(factionKey) < entry.m_iCost) return string.Format("Not enough RP for %1 (%2 needed)", entry.m_sName, entry.m_iCost);
  target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
  string team;
  int eta;
  if (!m_Manager.Mortars().Fire(factionKey, entry.m_eType, target, team, eta))
  {
   PrintFormat("[ACR] SUPPORT %1 %2 refused: %3", factionKey, entry.m_sName, team);
   return team;
  }
  m_Manager.m_Bases.Spend(factionKey, entry.m_iCost);
  ACR_MortarTeam started = m_Manager.Mortars().m_LastStarted;
  if (started && started.m_bFiring) started.m_iPaid = entry.m_iCost;
  float now = GetGame().GetWorld().GetWorldTime();
  entry.m_fReadyAt = now + entry.m_iCooldown * 1000;
  PrintFormat("[ACR] SUPPORT %1 %2 at %3 by %4 (ETA %5 s)", factionKey, entry.m_sName, target, team, eta);
  AddZone(factionKey, target, ZoneRadius(entry.m_eType), entry.m_sName, eta, ZoneDuration(entry.m_eType));
  string message = string.Format("%1 on %2: %3 firing, first rounds in about %4 s.", entry.m_sName, ACR_Manager.Grid(target), team, eta);
  if (!byAI) m_Manager.NotifyFaction(factionKey, message);
  return message;
 }

 ResourceName HelicopterFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  ACR_SupportUnitSettings unit;
  if (settings) unit = settings.SupportUnit(ACR_ESupportUnitRole.GUNSHIP);
  if (unit && !unit.m_sVehiclePrefab.IsEmpty()) return unit.m_sVehiclePrefab;
  if (settings && !settings.m_sSupportHelicopter.IsEmpty()) return settings.m_sSupportHelicopter;
  ResourceName prefab = HELI_EAST;
  if (IsWestern(factionKey)) prefab = HELI_WEST;
  return ACR_FactionCatalog.ResolvePrefab(factionKey, prefab, EEntityCatalogType.VEHICLE);
 }

 // Door-gunner support: the side's transport helicopter with its door guns.
 ResourceName TransportHelicopterFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  ACR_SupportUnitSettings unit;
  if (settings) unit = settings.SupportUnit(ACR_ESupportUnitRole.DOOR_GUN_HELI);
  if (unit && !unit.m_sVehiclePrefab.IsEmpty()) return unit.m_sVehiclePrefab;
  if (settings && !settings.m_sSupportTransportHelicopter.IsEmpty()) return settings.m_sSupportTransportHelicopter;
  ResourceName prefab = HELI_TRANSPORT_EAST;
  if (IsWestern(factionKey)) prefab = HELI_TRANSPORT_WEST;
  return ACR_FactionCatalog.ResolvePrefab(factionKey, prefab, EEntityCatalogType.VEHICLE);
 }

 ResourceName RocketFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && !settings.m_sSupportRocket.IsEmpty()) return settings.m_sSupportRocket;
  ResourceName prefab = ROCKET_EAST;
  if (IsWestern(factionKey)) prefab = ROCKET_WEST;
  return ACR_FactionCatalog.ResolvePrefab(factionKey, prefab, EEntityCatalogType.VEHICLE);
 }

 // Aircraft and creeping barrages come from the side's nearest base.
 vector ApproachDirection(string factionKey, vector target)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey) continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), target);
   if (distance > 60 && distance < bestDistance) { bestDistance = distance; best = base; }
  }
  vector dir = Vector(0, 0, 1);
  if (best) dir = target - best.GetOrigin();
  dir[1] = 0;
  if (dir.Length() < 1) dir = Vector(0, 0, 1);
  return dir.Normalized();
 }

 protected void ScheduleModule(ResourceName prefab, vector target, vector dir, int delaySeconds)
 {
  GetGame().GetCallqueue().CallLater(SpawnModule, Math.Max(0, delaySeconds - 3) * 1000, false, prefab, target, dir);
 }

 // Effect modules fire their first round about 3 s after spawning.
 protected void SpawnModule(ResourceName prefab, vector target, vector dir)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) { PrintFormat("[ACR] SUPPORT missing prefab %1", prefab); return; }
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG, 0, 0), parameters.Transform);
  parameters.Transform[3] = target;
  IEntity module = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  PrintFormat("[ACR] SUPPORT module %1 spawned=%2", prefab, module != null);
 }

 void ExplodeAt(vector impact) { Explode(impact); } // tests

 static int s_iExploded; // tests
 protected void Explode(vector impact)
 {
  s_iExploded++;
  impact[1] = GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]) + 0.3;
  // Local on the server (damage only): the prefab replicates, and the replicated copy
  // died before it could show anything on clients. Each client makes its own (below).
  IEntity bomb = SpawnLocal(BOMB, impact, vector.Zero);
  int struck = BlastDamage(impact);
  PrintFormat("[ACR] SUPPORT bomb at %1 spawned=%2, blast struck %3", impact, bomb != null, struck);
  // Every client spawns its own explosion and plays the blast sound itself: the
  // explosion's own sound is cut off when the entity removes itself (1 Oct MP: the
  // bombs were seen but not heard).
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_BroadcastExplosion(impact);
  if (GetGame().GetPlayerController()) PlayBlast(impact); // a hosting player hears it too
 }

 // FOB HQ cook-off (Jacob, 18 Oct: a bigger blast when a demolished HQ goes up): one heavy
 // blast (the bomb run's, with its scripted lethality) shortly after the charge, then
 // COOKOFF_SECONDARIES medium blasts (vanilla Medium TNT: its own small damage radius) at
 // random spots round the tent over the next few seconds, as stored ammunition and fuel go.
 static const ResourceName COOKOFF_SECONDARY = "{564D57EA34A75775}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Medium.et";
 static const ResourceName COOKOFF_SOUND = "{B9347A5498317F65}Sounds/Particles/Logistics/Explosion/TNT/Particles_Explosions_TNT_Medium.acp";
 static int COOKOFF_MAIN_MS = 700;
 static int COOKOFF_SECONDARIES = 4;
 static int COOKOFF_FIRST_MS = 1800;
 static int COOKOFF_LAST_MS = 7500;
 static float COOKOFF_SPREAD = 7;
 static int s_iCookOffs; // tests: cook-offs started
 static int s_iSecondaries; // tests: secondary blasts gone off
 static bool s_bCookOffOff; // tests: fixtures next to other test subjects

 // Returns the time (ms) until the last blast.
 int CookOff(vector at)
 {
  if (s_bCookOffOff) return 0;
  s_iCookOffs++;
  PrintFormat("[ACR] DEMOLITION cook-off at %1: main blast in %2 ms, %3 secondaries", at, COOKOFF_MAIN_MS, COOKOFF_SECONDARIES);
  GetGame().GetCallqueue().CallLater(Explode, COOKOFF_MAIN_MS, false, at);
  int last = COOKOFF_MAIN_MS;
  for (int i = 0; i < COOKOFF_SECONDARIES; i++)
  {
   float angle = Math.RandomFloat(0, Math.PI2);
   float dist = Math.RandomFloat(2, COOKOFF_SPREAD);
   vector spot = at + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
   int delay = Math.RandomIntInclusive(COOKOFF_FIRST_MS, COOKOFF_LAST_MS);
   if (delay > last) last = delay;
   GetGame().GetCallqueue().CallLater(Secondary, delay, false, spot);
  }
  return last;
 }

 protected void Secondary(vector at)
 {
  s_iSecondaries++;
  at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.5;
  IEntity blast = SpawnLocal(COOKOFF_SECONDARY, at, vector.Zero);
  PrintFormat("[ACR] DEMOLITION secondary blast at %1 spawned=%2", at, blast != null);
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_BroadcastExplosionOf(at, COOKOFF_SECONDARY, COOKOFF_SOUND);
  if (GetGame().GetPlayerController()) PlaySound(COOKOFF_SOUND, at);
 }

 // Spawns a rocket/shell pointing at the target; it is launched next frame on the
 // server and every client (projectiles are simulated locally everywhere).
 // Like the game's own weapons, each machine fires its own local projectile: the
 // server's does the damage, the clients' show and sound the flight and impact. (A
 // replicated rocket was deleted on clients when the server's hit first, often
 // before the client's copy reached the ground, so its blast never played there.)
 ResourceName BatteryShellFor(string factionKey)
 {
  ResourceName shell = "{98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et";
  if (IsWestern(factionKey)) shell = "{38BAE094333E31BF}Prefabs/Weapons/Ammo/Ammo_Shell_81mm_HE_M821.et";
  return ACR_FactionCatalog.ResolvePrefab(factionKey, shell, EEntityCatalogType.VEHICLE);
 }

 // One howitzer round: a shell dropped from high above the impact point (every machine flies
 // its own, so everyone hears it come in); where the server's lands, a heavy blast follows.
 void FireBatteryRound(ResourceName shell, vector impact)
 {
  impact[1] = GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]);
  vector from = impact + Vector(Math.RandomFloat(-25, 25), BATTERY_DROP_HEIGHT, Math.RandomFloat(-25, 25));
  IEntity falling = FireProjectile(shell, from, impact);
  m_BatteryShells.Insert(falling);
  m_BatteryLast.Insert(from);
  m_BatteryAim.Insert(impact);
  m_BatteryDeadline.Insert(GetGame().GetWorld().GetWorldTime() + 20000);
 }

 protected void TrackBatteryShells(float now)
 {
  for (int i = m_BatteryShells.Count() - 1; i >= 0; i--)
  {
   IEntity falling = m_BatteryShells[i];
   if (falling && now < m_BatteryDeadline[i])
   {
    m_BatteryLast[i] = falling.GetOrigin();
    continue;
   }
   vector at = m_BatteryLast[i];
   bool landed = !falling;
   if (falling) { SCR_EntityHelper.DeleteEntityAndChildren(falling); at = m_BatteryAim[i]; }
   PrintFormat("[ACR] SUPPORT battery round down after %1 s, %2 m from its aim point (shell landed itself: %3)", ((now - m_BatteryDeadline[i] + 20000) / 1000), (int)vector.DistanceXZ(at, m_BatteryAim[i]), landed);
   // Landed (or never did): the blast where it came down.
   if (vector.DistanceXZ(at, m_BatteryAim[i]) > 60) at = m_BatteryAim[i];
   Explode(at);
   s_iBatteryImpacts++;
   s_aBatteryImpacts.Insert(at);
   m_BatteryShells.RemoveOrdered(i);
   m_BatteryLast.RemoveOrdered(i);
   m_BatteryAim.RemoveOrdered(i);
   m_BatteryDeadline.RemoveOrdered(i);
  }
 }

 IEntity FireProjectile(ResourceName prefab, vector from, vector to, vector parentVelocity = "0 0 0")
 {
  vector dir = (to - from).Normalized();
  IEntity projectile = SpawnLocal(prefab, from, dir);
  if (!projectile) return null;
  SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (gameMode) gameMode.ACR_BroadcastFire(prefab, from, dir, parentVelocity);
  ACR_PendingLaunch launch = new ACR_PendingLaunch();
  launch.m_Entity = projectile;
  launch.m_Dir = dir;
  launch.m_Velocity = parentVelocity;
  launch.m_iFrame = m_iFrame;
  m_Launches.Insert(launch);
  return projectile;
 }

 // A non-replicated copy of prefab at pos, nose along dir (zero: upright).
 // Heavy blasts (bomb run, howitzer battery) wrecked buildings but rarely killed (Jacob, 4 Oct):
 // the TNT prefab's own damage reaches only a few metres. On top of it, soldiers in the open
 // with a clear line to the blast take explosive damage that falls off with distance: lethal
 // inside BLAST_KILL, wounding (bleeding limbs and body) out to BLAST_WOUND. Cover between them
 // and the blast (a wall, a building, a rise) cuts it to BLAST_COVER; lying down halves it.
 // s_bBlastOff turns it off (the old behaviour; test comparison).
 static float BLAST_KILL = 12;
 static float BLAST_WOUND = 32;
 static float BLAST_COVER = 0.25;
 static bool s_bBlastOff;
 static const float BLAST_DAMAGE = 120; // per hit zone at the kill radius (tuned with srv:blast)
 static ref array<IEntity> s_BlastFound = {};

 static bool CollectBlast(IEntity e)
 {
  if (ChimeraCharacter.Cast(e)) s_BlastFound.Insert(e);
  return true;
 }

 int BlastDamage(vector impact)
 {
  if (s_bBlastOff || System.IsCLIParam("acrBlastOldTest")) return 0;
  s_BlastFound.Clear();
  vector reach = Vector(BLAST_WOUND, BLAST_WOUND, BLAST_WOUND);
  GetGame().GetWorld().QueryEntitiesByAABB(impact - reach, impact + reach, CollectBlast);
  int struck;
  foreach (IEntity e : s_BlastFound)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(e);
   if (!character || !character.GetCharacterController()) continue;
   if (character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD) continue;
   float distance = vector.Distance(character.GetOrigin(), impact);
   if (distance > BLAST_WOUND) continue;
   SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.GetDamageManager());
   if (!damage) continue;
   // Inside the kill radius full damage, falling off to nothing at the wound radius.
   float share = 1;
   if (distance > BLAST_KILL) share = 1 - (distance - BLAST_KILL) / (BLAST_WOUND - BLAST_KILL);
   share = share * share;
   if (!ClearLine(impact + Vector(0, 0.5, 0), character.GetOrigin() + Vector(0, 0.9, 0), character)) share *= BLAST_COVER;
   if (character.GetCharacterController().GetStance() == ECharacterStance.PRONE) share *= 0.5;
   if (share < 0.02) continue;
   vector hit[3];
   hit[0] = character.GetOrigin() + Vector(0, 1, 0);
   hit[1] = (character.GetOrigin() - impact).Normalized();
   hit[2] = -hit[1];
   // Body and limbs nearest the blast: two to four zones depending on how close.
   int zones = 2;
   if (share > 0.4) zones = 3;
   if (share > 0.8) zones = 4;
   array<HitZone> all = {};
   damage.GetAllHitZones(all);
   array<HitZone> body = {};
   foreach (HitZone candidate : all) if (SCR_CharacterHitZone.Cast(candidate)) body.Insert(candidate);
   if (body.IsEmpty()) continue;
   for (int z = 0; z < zones; z++)
   {
    HitZone zone = body.GetRandomElement();
    SCR_DamageContext context = new SCR_DamageContext(EDamageType.EXPLOSIVE, BLAST_DAMAGE * share, hit, character, zone, Instigator.CreateInstigator(null), null, -1, -1);
    damage.HandleDamage(context);
   }
   struck++;
  }
  return struck;
 }

 protected static IEntity s_BlastTarget;
 static bool BlastFilter(notnull IEntity e)
 {
  if (e == s_BlastTarget || ChimeraCharacter.Cast(e)) return false;
  return true;
 }

 // Nothing solid between the blast and the soldier (other people don't shield).
 static bool ClearLine(vector from, vector to, IEntity target)
 {
  s_BlastTarget = target;
  TraceParam trace = new TraceParam();
  trace.Start = from;
  trace.End = to;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float hit = GetGame().GetWorld().TraceMove(trace, BlastFilter);
  return hit >= 0.99;
 }

 static IEntity SpawnLocal(ResourceName prefab, vector pos, vector dir)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  if (dir != vector.Zero) Math3D.MatrixFromForwardVec(dir, parameters.Transform);
  else Math3D.MatrixIdentity4(parameters.Transform);
  parameters.Transform[3] = pos;
  return GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), parameters);
 }

 static const ResourceName BOMB_SOUND = "{E4EF3755472EC669}Sounds/Particles/Logistics/Explosion/TNT/Particles_Explosions_TNT_Large.acp";
 // Plays the large blast at a point, independent of any entity (clients only).
 static bool PlayBlast(vector at)
 {
  return PlaySound(BOMB_SOUND, at);
 }

 static bool PlaySound(ResourceName sound, vector at)
 {
  // Straight through the audio system (the sound manager route stayed silent in the
  // 1 Oct live test); the sound graph reads distance and surroundings itself.
  vector mat[4];
  Math3D.MatrixIdentity4(mat);
  mat[3] = at;
  AudioHandle handle = AudioSystem.PlayEvent(sound, "SOUND_EXPLOSION", mat);
  bool ok = handle != AudioHandle.Invalid;
  PrintFormat("[ACR] SUPPORT blast sound at %1 played=%2", at, ok);
  return ok;
 }

 static void LaunchLocal(IEntity projectile, vector dir, vector parentVelocity)
 {
  if (!projectile) return;
  ProjectileMoveComponent move = ProjectileMoveComponent.Cast(projectile.FindComponent(ProjectileMoveComponent));
  if (move) move.Launch(dir, parentVelocity, 1, projectile, null, null, null, null);
 }

 ResourceName ConfiguredCrewPrefab(string factionKey, bool gunship)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (!settings) return "";
  ACR_ESupportUnitRole role = ACR_ESupportUnitRole.DOOR_GUN_HELI;
  if (gunship) role = ACR_ESupportUnitRole.GUNSHIP;
  ACR_SupportUnitSettings unit = settings.SupportUnit(role);
  if (unit) return unit.m_sCrewPrefab;
  return "";
 }

 ResourceName CrewPrefab(string factionKey, bool gunship = true)
 {
  ResourceName configured = ConfiguredCrewPrefab(factionKey, gunship);
  if (!configured.IsEmpty()) return configured;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(factionKey);
  if (!data.m_aGarrison.IsEmpty()) return data.m_aGarrison[0];
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  SCR_BasePlayerLoadout loadout;
  if (faction) loadout = GetGame().GetLoadoutManager().GetRandomFactionLoadout(faction);
  if (loadout) return loadout.GetLoadoutResource();
  return "";
 }

 // Living enemy soldiers (squads, garrisons and players) within radius.
 void FindEnemies(string factionKey, vector center, float radius, array<IEntity> result)
 {
  array<AIAgent> agents = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == factionKey) continue;
   agents.Clear();
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents) AddIfNear(agent.GetControlledEntity(), center, radius, result);
  }
  foreach (SCR_AIGroup garrison : m_Manager.m_Bases.m_GarrisonGroups)
  {
   if (!garrison || !garrison.GetFaction() || garrison.GetFaction().GetFactionKey() == factionKey) continue;
   agents.Clear();
   garrison.GetAgents(agents);
   foreach (AIAgent guard : agents) AddIfNear(guard.GetControlledEntity(), center, radius, result);
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!faction || faction.GetFactionKey() == factionKey) continue;
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (pc) AddIfNear(pc.GetControlledEntity(), center, radius, result);
  }
 }

 protected void AddIfNear(IEntity soldier, vector center, float radius, array<IEntity> result)
 {
  if (!ACR_Respawn.IsAlive(soldier) || result.Contains(soldier)) return;
  if (vector.DistanceXZ(soldier.GetOrigin(), center) <= radius) result.Insert(soldier);
 }

 IEntity FindEnemyNear(string factionKey, vector center, float radius)
 {
  IEntity best;
  float bestDistance = radius;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == factionKey) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!ACR_Respawn.IsAlive(soldier)) continue;
    float distance = vector.DistanceXZ(soldier.GetOrigin(), center);
    if (distance < bestDistance) { bestDistance = distance; best = soldier; }
   }
  }
  return best;
 }

 // Every server frame.
 protected float m_fNextCrewCheck;

 void Frame(float dt)
 {
  m_iFrame++;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int l = m_Launches.Count() - 1; l >= 0; l--)
  {
   ACR_PendingLaunch launch = m_Launches[l];
   if (launch.m_iFrame >= m_iFrame) continue;
   m_Launches.Remove(l);
   if (!launch.m_Entity) continue;
   LaunchLocal(launch.m_Entity, launch.m_Dir, launch.m_Velocity);
  }
  for (int r = m_Runs.Count() - 1; r >= 0; r--)
  {
   ACR_SupportRun run = m_Runs[r];
   if (run.Frame(this, dt, now)) continue;
   run.Finish();
   m_Runs.Remove(r);
  }
  TrackBatteryShells(now);
  if (m_Station) m_Station.Tick(now);
  for (int v = m_Vehicles.Count() - 1; v >= 0; v--)
  {
   if (!m_Vehicles[v]) m_Vehicles.Remove(v);
   else if (now >= m_fNextCrewCheck) RememberCrew(m_Vehicles[v]);
  }
  if (now >= m_fNextCrewCheck)
  {
   m_fNextCrewCheck = now + 1000;
   for (int g = m_CrewGroups.Count() - 1; g >= 0; g--) if (!m_CrewGroups[g]) m_CrewGroups.Remove(g);
  }
  for (int z = m_Zones.Count() - 1; z >= 0; z--) if (now > m_Zones[z].m_fEndAt) m_Zones.Remove(z);
 }
}

// The game mode entity is replicated, so it carries the "launch this projectile" broadcast.
modded class SCR_BaseGameMode
{
 protected override void OnPlayerConnected(int playerId)
 {
  super.OnPlayerConnected(playerId);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && Replication.IsServer()) manager.PlayerConnected(playerId);
 }

 protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
 {
  // Possessing a squad soldier: hand him back first. Vanilla deletes the player's controlled
  // entity, which was that soldier, and left his command unit standing with nobody in it
  // (audit 31 Oct; as ACR_Requests.ReturnToCommandUnit).
  SCR_PlayerController possessor = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (Replication.IsServer() && possessor && possessor.IsPossessing())
  {
   ACR_Manager owner = ACR_Manager.GetInstance();
   ACR_MapHandoff.Release(possessor, possessor.GetControlledEntity());
   if (owner) owner.ReleaseAuthority(possessor);
   possessor.SetPossessedEntity(null);
   PrintFormat("[ACR] RETURN player=%1 (disconnected while possessing)", playerId);
  }
  super.OnPlayerDisconnected(playerId, cause, timeout);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && Replication.IsServer()) manager.PlayerDisconnected(playerId);
 }

 // Every death and destroyed vehicle goes to the cleanup list.
 protected override void OnControllableDestroyed(IEntity entity, IEntity killerEntity, notnull Instigator instigator)
 {
  super.OnControllableDestroyed(entity, killerEntity, instigator);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && manager.m_Cleanup && Replication.IsServer()) manager.m_Cleanup.OnDestroyed(entity);
 }

 void ACR_BroadcastFire(ResourceName prefab, vector from, vector dir, vector parentVelocity)
 {
  Rpc(ACR_RpcFire, prefab, from, dir, parentVelocity);
 }
 // Client: its own copy of a support projectile, launched a frame after it exists
 // (as on the server).
 [RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
 protected void ACR_RpcFire(ResourceName prefab, vector from, vector dir, vector parentVelocity)
 {
  if (Replication.IsServer()) return;
  IEntity projectile = ACR_Supports.SpawnLocal(prefab, from, dir);
  if (projectile) GetGame().GetCallqueue().CallLater(ACR_LaunchLater, 20, false, projectile, dir, parentVelocity);
 }
 protected void ACR_LaunchLater(IEntity projectile, vector dir, vector parentVelocity)
 {
  ACR_Supports.LaunchLocal(projectile, dir, parentVelocity);
 }
 void ACR_BroadcastFlyover(ResourceName prefab, vector start, vector dir, float speed, float altitude, float seconds)
 {
  Rpc(ACR_RpcFlyover, prefab, start, dir, speed, altitude, seconds);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcFlyover(ResourceName prefab, vector start, vector dir, float speed, float altitude, float seconds)
 {
  if (Replication.IsServer()) return;
  PrintFormat("[ACR] FLYOVER RPC received prefab=%1 at=%2", prefab, start);
  ACR_LocalFlyover.Start(prefab, start, dir, speed, altitude, seconds);
 }
 void ACR_BroadcastExplosion(vector at)
 {
  Rpc(ACR_RpcExplosion, at);
 }
 // Any explosion prefab and its sound (FOB cook-off secondaries).
 void ACR_BroadcastExplosionOf(vector at, ResourceName prefab, ResourceName sound)
 {
  Rpc(ACR_RpcExplosionOf, at, prefab, sound);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcExplosionOf(vector at, ResourceName prefab, ResourceName sound)
 {
  if (Replication.IsServer()) return;
  IEntity effect = ACR_Supports.SpawnLocal(prefab, at, vector.Zero);
  bool played = ACR_Supports.PlaySound(sound, at);
  PrintFormat("[ACR] SUPPORT explosion RPC received at=%1 prefab=%2 effect=%3 sound=%4", at, prefab, effect != null, played);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcExplosion(vector at)
 {
  if (Replication.IsServer()) return;
  IEntity effect = ACR_Supports.SpawnLocal(ACR_Supports.BOMB, at, vector.Zero);
  bool sound = ACR_Supports.PlayBlast(at);
  PrintFormat("[ACR] SUPPORT explosion RPC received at=%1 effect=%2 sound=%3", at, effect != null, sound);
 }
}
