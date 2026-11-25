// Operational supply lines & isolated/cut-off bases (-acrSupplyLines 1, off by default).
// Every RECOMPUTE_MS, builds each side's supply graph from its supply roots (HQ, held ports,
// online FOBs with a built Supply Depot, or bases with active truck/air-bridge relief). Two
// friendly bases connect if within MAX_HOP_M and the corridor between them is not interdicted by
// an enemy base or an enemy manned roadblock.
//
// An owned base unreachable from any supply root is Isolated:
//  - Income is halved (ISOLATED_INCOME_SHARE, read via ACR_TownSupport.IncomeShare).
//  - Deployment and Reinforce are blocked (ACR_LogisticsRules.CanDeploy / CanReinforce).
//  - Relief: clear the encirclement/roadblock, deliver supplies via truck (ACR_SupplyRuns.Deliver),
//    or land an air lift near the base (ACR_AirLift) for 10 min of emergency relief.
class ACR_SupplyLines
{
	static const float RECOMPUTE_MS = 5000;
	static const float MAX_HOP_M = 2600;
	static const float INTERDICT_BASE_M = 250;     // extra interdiction margin past an enemy base ring
	static const float INTERDICT_ROADBLOCK_M = 200; // interdiction radius of an enemy manned roadblock
	static const float ISOLATED_INCOME_SHARE = 0.5;
	static const float RELIEF_MS = 600000;         // 10 min emergency relief from a truck delivery or air lift
	static const float RELIEF_REACH_M = 250;       // air lift touchdown within ring + this relieves the base

	protected static ref map<ACR_Base, bool> s_Isolated;
	protected static ref map<ACR_Base, float> s_ReliefUntil;
	protected static ref map<ACR_Base, string> s_ReliefSide;
	protected static float s_fNextTick = -1;

	// Called by ACR_StaticInit at game start.
	static void ACR_InitStatics()
	{
		s_Isolated = new map<ACR_Base, bool>();
		s_ReliefUntil = new map<ACR_Base, float>();
		s_ReliefSide = new map<ACR_Base, string>();
	}

	static void Reset()
	{
		if (s_Isolated) s_Isolated.Clear();
		if (s_ReliefUntil) s_ReliefUntil.Clear();
		if (s_ReliefSide) s_ReliefSide.Clear();
		s_fNextTick = -1;
	}

	static bool On()
	{
		return ACR_LogisticsRules.Choice("acrSupplyLines", "0") == "1";
	}

	static bool IsIsolated(ACR_Base base)
	{
		if (!On() || !base || base.m_bRetired || base.m_sOwner.IsEmpty() || !s_Isolated) return false;
		bool cut = false;
		s_Isolated.Find(base, cut);
		return cut;
	}

	static float IncomeShare(ACR_Base base)
	{
		if (IsIsolated(base)) return ISOLATED_INCOME_SHARE;
		return 1.0;
	}

	static bool HasRelief(ACR_Base base, float now = -1)
	{
		if (!base || base.m_sOwner.IsEmpty() || !s_ReliefUntil || !s_ReliefSide) return false;
		if (now < 0) now = GetGame().GetWorld().GetWorldTime();
		float until;
		if (!s_ReliefUntil.Find(base, until) || now >= until) return false;
		string side;
		return s_ReliefSide.Find(base, side) && side == base.m_sOwner;
	}

	// Grant temporary emergency supply relief (truck delivery or air lift) to this base only.
	static void Relieve(ACR_Base base, string why = "supply relief")
	{
		if (!Replication.IsServer() || !base || base.m_bRetired || base.m_sOwner.IsEmpty() || !s_ReliefUntil || !s_ReliefSide) return;
		float now = GetGame().GetWorld().GetWorldTime();
		bool wasCut = IsIsolated(base);
		s_ReliefUntil.Set(base, now + RELIEF_MS);
		s_ReliefSide.Set(base, base.m_sOwner);
		if (s_Isolated) s_Isolated.Set(base, false);
		if (wasCut && !base.m_sOwner.IsEmpty())
		{
			ACR_Manager manager = ACR_Manager.GetInstance();
			if (manager)
			{
				ACR_Intel intel = manager.IntelFor(base.m_sOwner);
				if (intel) intel.AddFeed(now, string.Format("%1 relieved by %2 (10 min)", base.m_sName, why), base.GetOrigin());
			}
		}
	}

	// Relieve any friendly base near 'pos' (e.g. an air lift touching down inside a surrounded pocket).
	static void RelieveNear(string factionKey, vector pos, string why = "air lift")
	{
		if (!Replication.IsServer() || !On() || factionKey.IsEmpty()) return;
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || base.m_bRetired || base.m_sOwner != factionKey) continue;
			if (vector.DistanceXZ(base.GetOrigin(), pos) <= base.m_fRadius + RELIEF_REACH_M) Relieve(base, why);
		}
	}

	// Detail suffix for ACR_MapFeed (owner view).
	static string Describe(ACR_Base base)
	{
		if (!On() || !base) return "";
		if (IsIsolated(base)) return " | ISOLATED (cut off)";
		if (HasRelief(base)) return " | emergency supply";
		return "";
	}

	// ---------------------------------------------------------------- graph recompute

	static void Tick(ACR_Manager manager)
	{
		if (!Replication.IsServer() || !On() || !manager || !s_Isolated) return;
		float now = GetGame().GetWorld().GetWorldTime();
		if (s_fNextTick >= 0 && now < s_fNextTick) return;
		s_fNextTick = now + RECOMPUTE_MS;
		Prune(now);
		// Gather active factions owning at least one base.
		array<string> sides = {};
		foreach (ACR_Base b : ACR_Base.s_All)
		{
			if (!b || b.m_bRetired || b.m_sOwner.IsEmpty()) continue;
			if (!sides.Contains(b.m_sOwner)) sides.Insert(b.m_sOwner);
		}
		foreach (string side : sides) RecomputeSide(manager, side, now);
	}

	protected static void Prune(float now)
	{
		for (int i = s_Isolated.Count() - 1; i >= 0; i--)
		{
			ACR_Base b = s_Isolated.GetKey(i);
			if (!b || b.m_bRetired || b.m_sOwner.IsEmpty()) s_Isolated.RemoveElement(i);
		}
		for (int j = s_ReliefUntil.Count() - 1; j >= 0; j--)
		{
			ACR_Base b = s_ReliefUntil.GetKey(j);
			string relSide;
			bool ownerChanged = !b || !s_ReliefSide || !s_ReliefSide.Find(b, relSide) || b.m_sOwner != relSide;
			if (!b || b.m_bRetired || ownerChanged || now >= s_ReliefUntil.GetElement(j))
			{
				if (b && s_ReliefSide) s_ReliefSide.Remove(b);
				s_ReliefUntil.RemoveElement(j);
			}
		}
	}

	protected static bool IsRoot(ACR_Manager manager, ACR_Base base, string side, float now)
	{
		if (ACR_LogisticsRules.IsHQ(base)) return true;
		if (ACR_SupplyRuns.IsPort(base)) return true;
		// A FOB with a built supply depot sustains its sector only while it actually holds stock.
		if (base.m_bBuilt && !base.m_bHqLost && base.m_FobSupply && ACR_FobSupply.Stock(base.m_FobSupply) > 0 && manager.m_Structures && manager.m_Structures.HasDepot(base)) return true;
		return false;
	}

	protected static void RecomputeSide(ACR_Manager manager, string side, float now)
	{
		// Insurgent sides don't rely on conventional army supply lines.
		if (ACR_InsurgentSide.IsInsurgent(side)) return;
		array<ACR_Base> held = {};
		array<ACR_Base> queue = {};
		array<ACR_Base> reached = {};
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || base.m_bRetired || base.m_sOwner != side) continue;
			held.Insert(base);
			if (IsRoot(manager, base, side, now))
			{
				queue.Insert(base);
				reached.Insert(base);
			}
		}
		// If a side has no root at all (e.g. custom scenario with no HQ/port), don't isolate everything.
		if (queue.IsEmpty())
		{
			foreach (ACR_Base b : held) s_Isolated.Set(b, false);
			return;
		}
		int head = 0;
		while (head < queue.Count())
		{
			ACR_Base cur = queue[head];
			head++;
			vector a = cur.GetOrigin();
			foreach (ACR_Base next : held)
			{
				if (reached.Contains(next)) continue;
				vector bPos = next.GetOrigin();
				if (vector.DistanceXZ(a, bPos) > MAX_HOP_M) continue;
				if (CorridorInterdicted(manager, side, a, bPos, cur, next)) continue;
				reached.Insert(next);
				queue.Insert(next);
			}
		}
		ACR_Intel intel = manager.IntelFor(side);
		foreach (ACR_Base base : held)
		{
			bool wasCut = false;
			s_Isolated.Find(base, wasCut);
			// Emergency truck/air relief un-isolates this specific base without making it a root for others.
			bool nowCut = !reached.Contains(base) && !HasRelief(base, now);
			s_Isolated.Set(base, nowCut);
			if (nowCut && !wasCut)
			{
				if (intel) intel.AddFeed(now, string.Format("%1 is cut off from supply!", base.m_sName), base.GetOrigin());
				ACR_RadioAudio.NotifyFaction(side, ACR_ERadioCue.BASE_ATTACK);
			}
			else if (!nowCut && wasCut)
			{
				if (intel) intel.AddFeed(now, string.Format("%1 supply line restored", base.m_sName), base.GetOrigin());
			}
		}
	}

	// True if an enemy base or an enemy manned roadblock sits across the XZ segment A->B.
	protected static bool CorridorInterdicted(ACR_Manager manager, string side, vector a, vector b, ACR_Base skipA, ACR_Base skipB)
	{
		foreach (ACR_Base enemy : ACR_Base.s_All)
		{
			if (!enemy || enemy == skipA || enemy == skipB || enemy.m_bRetired || enemy.m_sOwner.IsEmpty()) continue;
			if (enemy.m_sOwner == side || ACR_Insurgency.Friendly(enemy.m_sOwner, side)) continue;
			float blockR = enemy.m_fRadius + INTERDICT_BASE_M;
			if (SegmentDistXZ(a, b, enemy.GetOrigin()) <= blockR) return true;
		}
		foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
		{
			if (!rb || !rb.m_Entity || rb.m_sFaction.IsEmpty() || rb.m_sFaction == side || ACR_Insurgency.Friendly(rb.m_sFaction, side)) continue;
			bool manned = rb.m_Defenders && (rb.m_Defenders.m_iReserve > 0 || rb.m_Defenders.m_Group);
			if (!manned && manager.m_Bases) manned = manager.m_Bases.MenWithin(rb.m_vPos, 40, rb.m_sFaction) > 0;
			if (!manned) continue;
			if (SegmentDistXZ(a, b, rb.m_vPos) <= INTERDICT_ROADBLOCK_M) return true;
		}
		return false;
	}

	// Shortest XZ distance from point 'p' to line segment 'a'->'b'.
	static float SegmentDistXZ(vector a, vector b, vector p)
	{
		float abx = b[0] - a[0];
		float abz = b[2] - a[2];
		float lenSq = abx * abx + abz * abz;
		if (lenSq < 1) return vector.DistanceXZ(a, p);
		float t = ((p[0] - a[0]) * abx + (p[2] - a[2]) * abz) / lenSq;
		t = Math.Clamp(t, 0, 1);
		vector closest = Vector(a[0] + abx * t, 0, a[2] + abz * t);
		return vector.DistanceXZ(closest, p);
	}
}
