// C&C-style forward base building (Stage 1; Roadmap Item 7).
// Validates FOB sites (HQ footprint plus the preset upgrade spots), fells trees, manages the
// construction truck's 500-supply reservation, and scripts the HQ composition.
// An Establish FOB order inside your own FOB whose HQ was destroyed rebuilds that HQ.
class ACR_FobBuild
{
	static const float HQ_RADIUS = 11.0;            // HQ composition footprint
	static const float CP_RADIUS = 40.0;            // FOB circle: engineers count inside it
	static const float FOB_SPACING = 120.0;         // centre to centre: upgrade spots never overlap
	static const float MIN_BASE_DISTANCE = 40.0;    // clear of other bases' circles
	static const float SEARCH_RADIUS = 50.0;
	static const float BUILD_DURATION_S = 90.0;
	static const float LOADS_RESERVED = 20.0;
	static const int SUPPLIES_RESERVED = 500;
	static const int MIN_CLEAR_SLOTS = 5;           // of 8, including the radio relay
	static const vector PARKING_OFFSET = "18 0 6";  // truck stand-off clear of the HQ and every slot

	static const ResourceName HQ_US = "{16AF2FE47A6C297B}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_US_01.et";
	static const ResourceName HQ_USSR = "{D70BF977E6D6300C}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_USSR_01.et";
	static const ResourceName HQ_FIA = "{16E06E20F4641F8A}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_FIA_01.et";

	static ResourceName HqPrefabFor(string factionKey)
	{
		if (factionKey == "USSR" || factionKey == "RHS_AFRF") return HQ_USSR;
		if (factionKey == "FIA") return HQ_FIA;
		return HQ_US;
	}

	static vector ParkingSpot(vector site)
	{
		return ACR_Manager.Ground(site + PARKING_OFFSET);
	}

	static bool CanOrder(ACR_Squad squad, out string refusal)
	{
		refusal = "";
		if (!squad || squad.CountAlive() <= 0)
		{
			refusal = "Squad is eliminated";
			return false;
		}
		IEntity truck = squad.ConstructionTruck();
		if (!truck)
		{
			refusal = "Needs a construction truck";
			return false;
		}
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager)
		{
			refusal = "Manager unavailable";
			return false;
		}
		if (!manager.m_Logistics) manager.m_Logistics = new ACR_Logistics(manager);
		float stock = manager.m_Logistics.StockOf(truck);
		if (stock < LOADS_RESERVED)
		{
			int supplies = stock * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
			refusal = string.Format("Needs 500 supplies (truck has %1)", supplies);
			return false;
		}
		if (squad.OrderState() == ACR_EOrderState.BUILD_FOB)
		{
			refusal = "Already establishing a FOB";
			return false;
		}
		return true;
	}

	// Your own built FOB whose HQ is gone, containing the spot (rebuild target).
	static ACR_Base HqLostFobAt(vector spot, string faction)
	{
		if (faction.IsEmpty()) return null;
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_bHqLost || base.m_sOwner != faction) continue;
			if (vector.DistanceXZ(base.GetOrigin(), spot) <= base.m_fRadius) return base;
		}
		return null;
	}

	// One circular footprint: map bounds, water, slope, roads, buildings and walls,
	// other compositions (built or blueprint), and trees when they cannot be felled.
	// ignoreTrees: place over trees that cannot be felled (FOB defences clip through vegetation).
	// maxRise: allowed terrain rise across the footprint (0 = the default for its size).
	// roadMargin: how far past its radius a footprint must stay from a road's edge (a building's
	// access; Fortify's sandbags only keep off the road itself).
	static bool FootprintClear(vector spot, float radius, array<IEntity> ignore, out string reason, bool ignoreTrees = false, float maxRise = 0, float roadMargin = 1.0)
	{
		reason = "";
		if (!ACR_Manager.OnMap(spot))
		{
			reason = "out of world bounds";
			return false;
		}
		BaseWorld world = GetGame().GetWorld();
		float centerY = world.GetSurfaceY(spot[0], spot[2]);
		float minY = centerY;
		float maxY = centerY;
		if (BBB_Terrain.IsWaterAt(spot))
		{
			reason = "in water";
			return false;
		}
		for (int i = 0; i < 12; i++)
		{
			float angle = i * 30 * Math.DEG2RAD;
			float ca = Math.Cos(angle);
			float sa = Math.Sin(angle);
			vector rim = spot + Vector(ca * radius, 0, sa * radius);
			if (BBB_Terrain.IsWaterAt(rim))
			{
				reason = "touches water";
				return false;
			}
			float yOuter = world.GetSurfaceY(rim[0], rim[2]);
			float yMid = world.GetSurfaceY(spot[0] + ca * radius * 0.5, spot[2] + sa * radius * 0.5);
			minY = Math.Min(minY, Math.Min(yOuter, yMid));
			maxY = Math.Max(maxY, Math.Max(yOuter, yMid));
		}
		float allowedRise = maxRise;
		if (allowedRise <= 0) allowedRise = Math.Max(1.5, radius * 0.12);
		if (maxY - minY > allowedRise)
		{
			reason = string.Format("too steep (%1 m rise)", (int)(maxY - minY + 0.5));
			return false;
		}

		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld && aiWorld.GetRoadNetworkManager())
		{
			BaseRoad road;
			float roadDist;
			aiWorld.GetRoadNetworkManager().GetClosestRoad(spot, road, roadDist);
			if (road && roadDist - road.GetWidth() * 0.5 < radius + roadMargin)
			{
				reason = "on a road";
				return false;
			}
		}

		s_Ignore = ignore;
		s_Obstacles.Clear();
		world.QueryEntitiesBySphere(spot, radius, CollectObstacle, null, EQueryEntitiesFlags.STATIC);
		bool blocked = !s_Obstacles.IsEmpty();
		s_Obstacles.Clear();
		s_Ignore = null;
		if (blocked)
		{
			reason = "buildings or walls in the way";
			return false;
		}

		IEntity composition = ACR_Structures.CompositionOverlapping(spot, radius, ignore);
		if (composition)
		{
			reason = string.Format("%1 in the way", ACR_Structures.KindName(ACR_Structures.KindOf(composition)));
			return false;
		}

		if (!ignoreTrees && !CanFellTrees() && CountTrees(spot, radius * 0.8) > 0)
		{
			reason = "trees in the way";
			return false;
		}
		return true;
	}

	// A FOB site: the HQ footprint must be clear and enough upgrade spots usable.
	static bool CheckSpot(vector spot, out string reason, ACR_Squad ignoreBuilder = null)
	{
		int clearSlots;
		return CheckSpotCount(spot, reason, clearSlots, ignoreBuilder);
	}

	static bool CheckSpotCount(vector spot, out string reason, out int clearSlots, ACR_Squad ignoreBuilder = null, out int clearDefences = 0)
	{
		clearSlots = 0;
		clearDefences = 0;
		reason = "";
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		if (!ACR_Manager.OnMap(spot))
		{
			reason = "Out of world bounds";
			return false;
		}
		if (BBB_Terrain.IsWaterAt(spot))
		{
			reason = "Site is in water";
			return false;
		}

		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || base.m_bRetired) continue;
			float dist = vector.DistanceXZ(base.GetOrigin(), spot);
			float needed = base.m_fRadius + MIN_BASE_DISTANCE;
			if (base.m_bBuilt) needed = Math.Max(needed, FOB_SPACING);
			if (dist <= needed)
			{
				reason = string.Format("Too close to %1", base.m_sName);
				return false;
			}
		}

		// Reserve the whole layout before the HQ exists. Completion ignores only itself.
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager)
		{
			foreach (ACR_Squad builder : manager.m_Squads)
			{
				if (!builder || builder == ignoreBuilder || builder.OrderState() != ACR_EOrderState.BUILD_FOB || builder.FobReservedLoads() <= 0) continue;
				if (vector.DistanceXZ(builder.FobSite(), spot) <= FOB_SPACING)
				{
					reason = "Too close to a FOB construction site";
					return false;
				}
			}
		}

		// A completed HQ can wait for the structures tick before it becomes a base.
		foreach (SCR_CampaignBuildingCompositionComponent composition : ACR_Structures.s_Registered)
		{
			if (!composition || !composition.IsCompositionSpawned() || composition.ACR_BuildRejected()) continue;
			IEntity hq = composition.GetOwner();
			if (!hq || ACR_Structures.KindOf(hq) != ACR_EStructure.HQ) continue;
			if (vector.DistanceXZ(hq.GetOrigin(), spot) <= FOB_SPACING)
			{
				reason = "Too close to an existing headquarters";
				return false;
			}
		}

		string hqWhy;
		if (!FootprintClear(spot, HQ_RADIUS, null, hqWhy))
		{
			reason = "HQ spot: " + hqWhy;
			return false;
		}

		// Smart slots: count usable upgrade spots; the relay is required for deployment.
		int clear = 0;
		string firstWhy = "";
		string relayWhy = "";
		for (int s = 0; s < ACR_FobSlots.SLOT_COUNT; s++)
		{
			string slotWhy;
			if (ACR_FobSlots.SlotSpotClear(spot, s, 0, null, slotWhy))
			{
				clear++;
				continue;
			}
			if (s == ACR_FobSlots.SLOT_RELAY) relayWhy = slotWhy;
			if (firstWhy.IsEmpty()) firstWhy = string.Format("%1 %2", ACR_FobSlots.SlotName(s), slotWhy);
		}
		clearSlots = clear;
		if (!relayWhy.IsEmpty())
		{
			reason = "No room for the radio relay: " + relayWhy;
			return false;
		}
		if (clear < MIN_CLEAR_SLOTS)
		{
			reason = string.Format("Too few clear upgrade spots (%1)", firstWhy);
			return false;
		}

		int defClear = 0;
		int postClear = 0;
		for (int d = 0; d < ACR_FobDefences.SPOT_COUNT; d++)
		{
			string defWhy;
			if (ACR_FobDefences.DefenceSpotClear(spot, d, defWhy))
			{
				defClear++;
				if (d < ACR_FobDefences.POST_COUNT) postClear++;
			}
		}
		clearDefences = defClear;
		if (postClear < 1)
		{
			reason = "No viable perimeter defence post positions (steep terrain or obstructions)";
			return false;
		}
		return true;
	}

	// maxRing: search rings of 10 m out to this distance (the AI uses a smaller search).
	static bool ValidateSite(vector clicked, out vector validSpot, out string refusal, string faction = "", float maxRing = SEARCH_RADIUS)
	{
		ACR_Base lost = HqLostFobAt(clicked, faction);
		if (lost)
		{
			validSpot = lost.GetOrigin();
			refusal = "";
			return true;
		}

		clicked[1] = GetGame().GetWorld().GetSurfaceY(clicked[0], clicked[2]);
		int clickedClear = 0;
		int clickedDefences = 0;
		bool clickedValid = CheckSpotCount(clicked, refusal, clickedClear, null, clickedDefences);
		int clickedScore = (clickedClear * 10) + clickedDefences;
		if (clickedValid && clickedClear >= ACR_FobSlots.SLOT_COUNT && clickedDefences >= ACR_FobDefences.SPOT_COUNT)
		{
			validSpot = clicked;
			return true;
		}

		vector bestSpot = clicked;
		int bestScore = 0;
		int bestClear = 0;
		float bestDist = 999999.0;
		if (clickedValid)
		{
			bestScore = clickedScore;
			bestClear = clickedClear;
			bestDist = 0.0;
		}

		for (float r = 10.0; r <= maxRing; r += 10.0)
		{
			int steps = 8 + (int)(r * 0.2);
			bool foundFullOnRing = false;
			for (int i = 0; i < steps; i++)
			{
				float angle = (i * 360.0 / steps) * Math.DEG2RAD;
				vector candidate = clicked + Vector(Math.Cos(angle) * r, 0, Math.Sin(angle) * r);
				candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
				string dummy;
				int candidateClear = 0;
				int candidateDefences = 0;
				if (CheckSpotCount(candidate, dummy, candidateClear, null, candidateDefences))
				{
					int candidateScore = (candidateClear * 10) + candidateDefences;
					float dist = r;
					if (candidateScore > bestScore || (candidateScore == bestScore && dist < bestDist))
					{
						bestSpot = candidate;
						bestScore = candidateScore;
						bestClear = candidateClear;
						bestDist = dist;
					}
					if (candidateClear >= ACR_FobSlots.SLOT_COUNT && candidateDefences >= ACR_FobDefences.SPOT_COUNT)
					{
						foundFullOnRing = true;
					}
				}
			}
			if (foundFullOnRing)
				break;
		}

		if (bestClear >= MIN_CLEAR_SLOTS)
		{
			validSpot = bestSpot;
			refusal = "";
			return true;
		}

		return false;
	}

	// Trees need the vanilla destruction sync manager to fall on every machine.
	static bool CanFellTrees()
	{
#ifdef ENABLE_DESTRUCTION
		return SCR_DestructibleTreesSynchManager.instance != null;
#else
		return false;
#endif
	}

	static int CountTrees(vector center, float radius)
	{
		s_Trees.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, CollectTree, null, EQueryEntitiesFlags.STATIC);
		int count = s_Trees.Count();
		s_Trees.Clear();
		return count;
	}

	// Fell standing trees in a footprint (replicated by the vanilla tree sync manager).
	static int FellTrees(vector center, float radius)
	{
		if (!Replication.IsServer() || !CanFellTrees()) return 0;
		int felled;
		s_Trees.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, CollectTree, null, EQueryEntitiesFlags.STATIC);
#ifdef ENABLE_DESTRUCTION
		foreach (IEntity e : s_Trees)
		{
			SCR_DestructibleTreeV2 tree = SCR_DestructibleTreeV2.Cast(e);
			if (!tree || tree.GetIsDestroyed()) continue;
			tree.SetToDestroy();
			felled++;
		}
#endif
		s_Trees.Clear();
		if (felled > 0) PrintFormat("[ACR] FOB: felled %1 trees at %2 (radius %3 m)", felled, center, (int)radius);
		return felled;
	}

	// An enemy squad with a conscious soldier near the site (its leader, or any member while
	// the leader is down: the unconscious don't contest).
	static bool IsContested(vector site, string faction)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return false;
		array<AIAgent> agents = {};
		foreach (ACR_Squad squad : manager.m_Squads)
		{
			// Allies (the TvTvZ truce partner, FIA friends) don't contest, as in Defenders.EnemyWithin.
			if (!squad || !squad.GetFaction() || ACR_Insurgency.Friendly(squad.GetFaction().GetFactionKey(), faction)) continue;
			IEntity anchor = squad.GetAnchor();
			if (!anchor) continue;
			if (Awake(anchor))
			{
				if (vector.DistanceXZ(anchor.GetOrigin(), site) <= CP_RADIUS + 10) return true;
				continue;
			}
			squad.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				IEntity member = agent.GetControlledEntity();
				if (member && Awake(member) && vector.DistanceXZ(member.GetOrigin(), site) <= CP_RADIUS + 10) return true;
			}
		}
		return false;
	}

	static bool Awake(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || !character.GetCharacterController()) return false;
		return !character.GetCharacterController().IsDead() && !character.GetCharacterController().IsUnconscious();
	}

	protected static array<IEntity> s_Ignore;
	protected static ref array<IEntity> s_Obstacles;
	protected static bool CollectObstacle(IEntity e)
	{
		if (!e || e.GetParent()) return true;
		if (s_Ignore && s_Ignore.Contains(e)) return true;
		if (SCR_DestructibleBuildingEntity.Cast(e) || BaseBuilding.Cast(e) || WallGeneratorEntity.Cast(e))
			s_Obstacles.Insert(e);
		return true;
	}

	protected static ref array<IEntity> s_Trees;
	// Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
	static void ACR_InitStatics()
	{
		s_Obstacles = {};
		s_Trees = {};
	}
	// Bushes and saplings under 4 m are left alone (the compositions simply cover them).
	static const float TREE_MIN_HEIGHT = 4.0;
	protected static bool CollectTree(IEntity e)
	{
		if (!Tree.Cast(e)) return true;
		vector mins, maxs;
		e.GetBounds(mins, maxs);
		if (maxs[1] - mins[1] < TREE_MIN_HEIGHT) return true;
#ifdef ENABLE_DESTRUCTION
		SCR_DestructibleTreeV2 tree = SCR_DestructibleTreeV2.Cast(e);
		if (tree && tree.GetIsDestroyed()) return true;
#endif
		s_Trees.Insert(e);
		return true;
	}
}
