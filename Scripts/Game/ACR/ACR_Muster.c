// Insurgent muster & disperse (0.8.0 #97 item 9): an insurgent role ability (not tied to FIA).
//  - Muster: pick a foot band from requisition and click a covered spot out of enemy sight, away
//    from army bases/roadblocks, not in a Loyal town, and within 3 km of a cold/warm hideout.
//    The band walks there unseen (never under 60 s; 1.5x slower in a Quiet town), showing a
//    "Mustering" ring on the insurgent map only. If an enemy watches the spot, it pauses and
//    aborts + refunds after WATCH_ABORT_S.
//  - Disperse: an insurgent infantry band out of contact, not spotted and not in a Loyal town
//    can disperse, returning part of its RP cost and trimming its hideout's muster cooldown.
class ACR_MusterJob : Managed
{
	string m_sFaction;
	int m_iEntry;
	string m_sName;
	vector m_vSpot;
	ACR_DefenderPost m_Hideout;
	int m_iPaid;
	float m_fDurationMs;
	float m_fProgressMs;
	float m_fWatchedMs;
	bool m_bPaused;
}

class ACR_Muster
{
	static const float MUSTER_REACH = 3000;      // m from a cold/warm hideout
	static const float ENEMY_CLEAR = 600;        // m from awake enemy soldiers
	static const float PLAYER_LOS_REACH = 1000;  // m for enemy player LOS check
	static const float BASE_CLEAR = 500;         // m from army base, FOB or roadblock
	static const float COVER_BUILDING = 40;      // m to a building counts as cover
	static const int COVER_WOODLAND = 2;         // ACR_Ambush.Cover score for woodland cover
	static const float WALK_SPEED = 1.5;         // m/s unseen walk from hideout
	static const float MIN_WALK_S = 60;          // minimum muster walk time
	static const float QUIET_SCALE = 1.5;        // slower in a Quiet town
	static const float COOLDOWN_S = 180;         // hideout rest after a muster completes
	static const float WATCH_ABORT_S = 180;      // watched spot aborts and refunds after this
	static const float COST_SCALE = 1.25;        // muster costs a little more than buying at a hideout
	static const float DISPERSE_CLEAR = 400;     // m from enemy soldiers to disperse
	static const int DISPERSE_PER_MAN = 3;       // RP refunded per living fighter
	static const int DISPERSE_MAX_REFUND = 15;

	static ref array<ref ACR_MusterJob> s_Jobs;
	protected static ref map<ACR_DefenderPost, float> s_CooldownUntil;

	// Called by ACR_StaticInit at game start.
	static void ACR_InitStatics()
	{
		s_Jobs = {};
		s_CooldownUntil = new map<ACR_DefenderPost, float>();
	}

	static void Reset()
	{
		if (s_Jobs) s_Jobs.Clear();
		if (s_CooldownUntil) s_CooldownUntil.Clear();
	}

	static int MusterCost(ACR_RequisitionEntry entry)
	{
		if (!entry) return 0;
		return Math.Ceil(entry.m_iCost * COST_SCALE);
	}

	static bool IsFootEntry(ACR_RequisitionEntry entry)
	{
		if (!entry) return false;
		return entry.m_sVehicle.IsEmpty() && !entry.m_bMortarTeam && !entry.m_bObserverTeam && entry.m_iSupportHeli == 0 && !entry.m_sGroup.IsEmpty();
	}

	static bool HasCover(vector point)
	{
		if (BBB_Terrain.IsWaterAt(point)) return false;
		array<IEntity> buildings = {};
		ACR_Garrison.BuildingsWithin(point, COVER_BUILDING, buildings);
		if (!buildings.IsEmpty()) return true;
		return ACR_Ambush.Cover(point) >= COVER_WOODLAND;
	}

	static bool WatchedByEnemy(vector point, string army)
	{
		if (army.IsEmpty()) return false;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager && manager.m_Bases && manager.m_Bases.MenWithin(point, ENEMY_CLEAR, army) > 0) return true;
		// Check enemy players with direct line of sight within PLAYER_LOS_REACH.
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			Faction f = SCR_FactionManager.SGetPlayerFaction(id);
			if (!f || f.GetFactionKey() != army) continue;
			IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (!body || !ACR_Respawn.IsAlive(body)) continue;
			if (vector.DistanceXZ(body.GetOrigin(), point) > PLAYER_LOS_REACH) continue;
			if (ACR_Ambush.SeesKillZone(body.GetOrigin(), point)) return true;
		}
		return false;
	}

	static bool NearArmyGround(vector point, string army)
	{
		if (army.IsEmpty()) return false;
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || base.m_bRetired || base.m_sOwner != army) continue;
			if (vector.DistanceXZ(base.GetOrigin(), point) < BASE_CLEAR) return true;
		}
		foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
		{
			if (!rb || !rb.m_Entity || rb.m_sFaction != army) continue;
			if (vector.DistanceXZ(rb.m_vPos, point) < BASE_CLEAR) return true;
		}
		return false;
	}

	// Returns the town containing 'point', or null.
	static ACR_Base TownAt(vector point)
	{
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!ACR_TownSupport.IsTown(base)) continue;
			float r = Math.Max(base.m_fRadius, ACR_TownSupport.RADIUS_MIN);
			if (vector.DistanceXZ(base.GetOrigin(), point) <= r) return base;
		}
		return null;
	}

	static bool HideoutBusy(ACR_DefenderPost hideout)
	{
		if (!hideout || !s_Jobs) return false;
		foreach (ACR_MusterJob j : s_Jobs) if (j.m_Hideout == hideout) return true;
		float until;
		if (s_CooldownUntil && s_CooldownUntil.Find(hideout, until))
			return GetGame().GetWorld().GetWorldTime() < until;
		return false;
	}

	// Validate a muster spot and find the nearest cold/warm ready hideout within MUSTER_REACH.
	static bool CanMuster(string faction, int entryIndex, vector point, out ACR_DefenderPost hideout, out float slowScale, out string refusal)
	{
		hideout = null;
		slowScale = 1.0;
		refusal = "";
		if (!ACR_InsurgentSide.IsInsurgent(faction)) { refusal = "only the insurgent commander can muster bands"; return false; }
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases || !manager.m_Requisition) { refusal = "server unavailable"; return false; }
		ACR_FactionData data = manager.m_Bases.GetFaction(faction);
		if (entryIndex < 0 || entryIndex >= data.m_aRequisitions.Count()) { refusal = "unknown unit"; return false; }
		ACR_RequisitionEntry entry = data.m_aRequisitions[entryIndex];
		if (!IsFootEntry(entry)) { refusal = "only foot infantry bands can muster"; return false; }
		if (entry.m_iRemaining <= 0) { refusal = entry.m_sName + ": none left"; return false; }
		if (manager.m_Requisition.OverSoldierCap(faction))
		{
			refusal = string.Format("too many fighters in the field (limit %1)", ACR_InsurgentSide.SoldierCap(faction, manager.m_iSoldierCap));
			return false;
		}
		point = ACR_Manager.Ground(point);
		if (!HasCover(point)) { refusal = "must muster in cover (buildings or woodland, never open ground)"; return false; }
		string army = ACR_Coin.Army();
		if (NearArmyGround(point, army)) { refusal = "too close to an army base, FOB or roadblock"; return false; }
		if (WatchedByEnemy(point, army)) { refusal = "enemy troops are too close or watching this spot"; return false; }
		ACR_Base town = TownAt(point);
		if (town && town.m_iSupportTier == ACR_TownSupport.LOYAL) { refusal = "cannot muster in a Loyal town"; return false; }
		if (town && town.m_iSupportTier == ACR_TownSupport.QUIET) slowScale = QUIET_SCALE;

		// Nearest standing hideout within MUSTER_REACH that is not hot/found and not busy.
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		float bestDist = MUSTER_REACH;
		bool hadInReach = false;
		bool hadCool = false;
		foreach (ACR_DefenderPost h : hideouts)
		{
			float d = vector.DistanceXZ(h.m_vPos, point);
			if (d > MUSTER_REACH) continue;
			hadInReach = true;
			string heat = ACR_FIAIntel.Heat(h, army);
			if (heat == "hot" || heat == "found") continue;
			hadCool = true;
			if (HideoutBusy(h)) continue;
			if (d < bestDist) { bestDist = d; hideout = h; }
		}
		if (!hideout)
		{
			if (!hadInReach) refusal = "no hideout within 3 km";
			else if (!hadCool) refusal = "nearby hideouts are too hot to muster from";
			else refusal = "nearby hideout is already mustering or resting";
			return false;
		}
		return true;
	}

	static bool Order(string faction, int entryIndex, vector point, out string refusal)
	{
		ACR_DefenderPost hideout;
		float slowScale;
		if (!CanMuster(faction, entryIndex, point, hideout, slowScale, refusal)) return false;
		ACR_Manager manager = ACR_Manager.GetInstance();
		ACR_FactionData data = manager.m_Bases.GetFaction(faction);
		ACR_RequisitionEntry entry = data.m_aRequisitions[entryIndex];
		int cost = MusterCost(entry);
		if (!manager.m_Bases.Spend(faction, cost))
		{
			refusal = string.Format("not enough RP (%1 needed to muster)", cost);
			return false;
		}
		point = ACR_Manager.Ground(point);
		float dist = vector.DistanceXZ(hideout.m_vPos, point);
		float walkS = Math.Max(MIN_WALK_S, dist / WALK_SPEED) * slowScale;
		ACR_MusterJob job = new ACR_MusterJob();
		job.m_sFaction = faction;
		job.m_iEntry = entryIndex;
		job.m_sName = entry.m_sName;
		job.m_vSpot = point;
		job.m_Hideout = hideout;
		job.m_iPaid = cost;
		job.m_fDurationMs = walkS * 1000;
		s_Jobs.Insert(job);
		manager.Command().Feed(faction, string.Format("Mustering %1 at %2 (%3 s, -%4 RP)", entry.m_sName, ACR_Manager.Grid(point), (int)walkS, cost), point);
		PrintFormat("[ACR] MUSTER %1 %2 from %3 to %4 (%5 s, -%6 RP)", faction, entry.m_sName, hideout.m_sName, ACR_Manager.Grid(point), (int)walkS, cost);
		return true;
	}

	// Server, every second (ACR_Manager.Tick).
	static void Tick(ACR_Manager manager, float stepMs = 1000)
	{
		if (!Replication.IsServer() || !s_Jobs || s_Jobs.IsEmpty()) return;
		string army = ACR_Coin.Army();
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = s_Jobs.Count() - 1; i >= 0; i--)
		{
			ACR_MusterJob job = s_Jobs[i];
			if (!job || !job.m_Hideout || job.m_Hideout.m_bGone)
			{
				if (job && manager && manager.m_Bases)
				{
					manager.m_Bases.Refund(job.m_sFaction, job.m_iPaid);
					manager.Command().Feed(job.m_sFaction, string.Format("Muster at %1 called off: hideout lost (+%2 RP)", ACR_Manager.Grid(job.m_vSpot), job.m_iPaid), job.m_vSpot);
				}
				s_Jobs.RemoveOrdered(i);
				continue;
			}
			if (WatchedByEnemy(job.m_vSpot, army))
			{
				job.m_bPaused = true;
				job.m_fWatchedMs += stepMs;
				if (job.m_fWatchedMs >= WATCH_ABORT_S * 1000)
				{
					if (manager && manager.m_Bases)
					{
						manager.m_Bases.Refund(job.m_sFaction, job.m_iPaid);
						manager.Command().Feed(job.m_sFaction, string.Format("Muster at %1 called off: area watched (+%2 RP)", ACR_Manager.Grid(job.m_vSpot), job.m_iPaid), job.m_vSpot);
					}
					PrintFormat("[ACR] MUSTER %1 at %2 aborted (watched, +%3 RP)", job.m_sName, ACR_Manager.Grid(job.m_vSpot), job.m_iPaid);
					s_Jobs.RemoveOrdered(i);
				}
				continue;
			}
			job.m_bPaused = false;
			job.m_fWatchedMs = 0;
			job.m_fProgressMs += stepMs;
			if (job.m_fProgressMs < job.m_fDurationMs) continue;
			// Stand up the band at job.m_vSpot!
			bool spawned = manager && manager.m_Requisition && manager.m_Requisition.SpawnFootEntryAt(job.m_sFaction, job.m_iEntry, job.m_vSpot);
			if (spawned)
			{
				s_CooldownUntil.Set(job.m_Hideout, now + COOLDOWN_S * 1000);
				manager.Command().Feed(job.m_sFaction, string.Format("%1 stood up at %2", job.m_sName, ACR_Manager.Grid(job.m_vSpot)), job.m_vSpot);
				PrintFormat("[ACR] MUSTER %1 stood up at %2", job.m_sName, ACR_Manager.Grid(job.m_vSpot));
			}
			else if (manager && manager.m_Bases)
			{
				manager.m_Bases.Refund(job.m_sFaction, job.m_iPaid);
			}
			s_Jobs.RemoveOrdered(i);
		}
	}

	// Tests: fast-forward active muster jobs by 'seconds'.
	static void AdvanceForTest(float seconds)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager) Tick(manager, seconds * 1000);
	}

	// ---------------------------------------------------------------- disperse

	static bool Disperse(ACR_Squad squad, out string refusal)
	{
		refusal = "";
		if (!squad || !squad.GetFaction() || !ACR_InsurgentSide.IsInsurgent(squad.GetFaction().GetFactionKey()))
		{
			refusal = "only an insurgent band can disperse";
			return false;
		}
		int alive = squad.CountAlive();
		if (alive <= 0 || !squad.GetAnchor()) { refusal = "band is unavailable"; return false; }
		if (!ACR_Fortify.Infantry(squad)) { refusal = "only foot bands can disperse"; return false; }
		vector pos = squad.GetAnchor().GetOrigin();
		string army = ACR_Coin.Army();
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!army.IsEmpty() && manager && manager.m_Bases && manager.m_Bases.MenWithin(pos, DISPERSE_CLEAR, army) > 0)
		{
			refusal = "cannot disperse while enemies are nearby — fade on foot first";
			return false;
		}
		ACR_Base town = TownAt(pos);
		if (town && town.m_iSupportTier == ACR_TownSupport.LOYAL)
		{
			refusal = "cannot disperse inside a Loyal town";
			return false;
		}
		string faction = squad.GetFaction().GetFactionKey();
		string name = squad.GetName();
		int refund = Math.Min(DISPERSE_MAX_REFUND, alive * DISPERSE_PER_MAN);
		// Remove the band's soldiers cleanly.
		array<AIAgent> agents = {};
		squad.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			IEntity body = agent.GetControlledEntity();
			if (body) SCR_EntityHelper.DeleteEntityAndChildren(body);
		}
		if (manager && manager.m_Bases)
		{
			manager.m_Bases.Refund(faction, refund);
			manager.Command().Feed(faction, string.Format("%1 dispersed (+%2 RP)", name, refund), pos);
		}
		// Halve the nearest hideout's remaining muster cooldown.
		ACR_DefenderPost near = ACR_HideoutHomes.NearestHome(pos, true);
		float until;
		float now = GetGame().GetWorld().GetWorldTime();
		if (near && s_CooldownUntil.Find(near, until) && until > now)
			s_CooldownUntil.Set(near, now + (until - now) * 0.5);
		PrintFormat("[ACR] DISPERSE %1 (%2 men, +%3 RP)", name, alive, refund);
		return true;
	}

	// Insurgent map markers for pending musters (called from ACR_InsurgentNetwork.Zones).
	static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
	{
		if (!s_Jobs || !ACR_InsurgentSide.IsInsurgent(faction)) return;
		foreach (ACR_MusterJob job : s_Jobs)
		{
			if (!job || job.m_sFaction != faction) continue;
			positions.Insert(job.m_vSpot);
			radii.Insert(35);
			int left = Math.Max(0, Math.Ceil((job.m_fDurationMs - job.m_fProgressMs) / 1000));
			if (job.m_bPaused) labels.Insert(string.Format("Mustering %1 (watched)", job.m_sName));
			else labels.Insert(string.Format("Mustering %1 (%2s)", job.m_sName, left));
		}
	}
}
