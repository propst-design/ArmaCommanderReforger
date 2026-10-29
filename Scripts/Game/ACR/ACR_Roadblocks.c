// Roadblocks / Observation Posts (aca-f23).
// Infantry squads construct fortified checkpoints across road corridors.
// Includes dragon's teeth / sandbag breastworks, dynamic navmesh updates,
// and a mounted heavy machine gun turret manned by the constructing squad.
// Squads remain mobile (Option 1): redeploying leaves the physical roadblock in place.
// Sizes (port of ACA Field Engineering): small, medium and large checkpoints (the medium and
// large ones carry a machine gun) and plain barricades sized to the road. East sides (USSR,
// Russian, SLA) build the Soviet set, FIA log barricades. A construction truck with the squad
// pays in supplies (vanilla composition prices) when it has them, otherwise RP; a cancelled
// build refunds wherever it was paid from. Every roadblock keeps a chicane lane its side's trucks
// and APCs can drive (ACR_RoadblockLane), with the gun and the holders' post beside it.

enum ACR_ERoadblockType
{
	SMALL,
	MEDIUM,
	LARGE,
	BARRICADE
}

class ACR_Roadblock : Managed
{
	vector m_vPos;
	float m_fYaw;
	string m_sFaction;
	IEntity m_Entity;
	IEntity m_Turret;
	string m_sBuilderSquad;
	float m_fSpawnedAt;
	bool m_bAlerted;
	int m_iType = ACR_ERoadblockType.MEDIUM;
	IEntity m_PaidTruck;  // the construction truck that paid for it (none: RP)
	float m_fPaidLoads;
	ACR_DefenderPost m_Defenders; // its defenders (-acrRoadblockDefenders, ACR_Defenders)
	vector m_vPost;               // where its holders and defenders post, off the lane (ACR_RoadblockLane.Post)
	float m_fPostRadius = 15;
	int m_iLaneMoved;             // pieces slid out of the lane
}

class ACR_Roadblocks : Managed
{
	static ref array<ref ACR_Roadblock> s_aRoadblocks = {};

	static const ResourceName PREFAB_US = "{55D508179EF5E64A}Prefabs/Compositions/Slotted/SlotRoadMedium/Checkpoint_M_US_01.et";
	static const ResourceName PREFAB_USSR = "{7C85836D444E3797}Prefabs/Compositions/Slotted/SlotRoadMedium/Checkpoint_M_USSR_01.et";
	static const ResourceName CHECKPOINT_S_US = "{E660DE914A7211EB}Prefabs/Compositions/Slotted/SlotRoadSmall/Checkpoint_S_US_01.et";
	static const ResourceName CHECKPOINT_S_USSR = "{9483333BFD9E2D0F}Prefabs/Compositions/Slotted/SlotRoadSmall/Checkpoint_S_USSR_01.et";
	static const ResourceName CHECKPOINT_L_US = "{E74D2F51CD4F03D0}Prefabs/Compositions/Slotted/SlotRoadLarge/Checkpoint_L_US_01.et";
	static const ResourceName CHECKPOINT_L_USSR = "{9F9924B626C5FA2C}Prefabs/Compositions/Slotted/SlotRoadLarge/Checkpoint_L_USSR_01.et";
	static const ResourceName BARRICADE_S_US = "{24B7F96197053B39}Prefabs/Compositions/Slotted/SlotRoadSmall/Barricade_S_US_01.et";
	static const ResourceName BARRICADE_S_USSR = "{83A10E8547281E58}Prefabs/Compositions/Slotted/SlotRoadSmall/Barricade_S_USSR_01.et";
	static const ResourceName BARRICADE_M_US = "{172D03072FE6ECAB}Prefabs/Compositions/Slotted/SlotRoadMedium/Barricade_M_US_01.et";
	static const ResourceName BARRICADE_M_USSR = "{27F6A0815E6FF6EF}Prefabs/Compositions/Slotted/SlotRoadMedium/Barricade_M_USSR_01.et";
	static const ResourceName BARRICADE_L_US = "{AF770C825EA592CF}Prefabs/Compositions/Slotted/SlotRoadLarge/Barricade_L_US_01.et";
	static const ResourceName BARRICADE_L_USSR = "{2A27606856B8A914}Prefabs/Compositions/Slotted/SlotRoadLarge/Barricade_L_USSR_01.et";
	static const ResourceName LOGS_1 = "{3BAF6AA8C20D924D}Prefabs/Compositions/Misc/SubCompositions/Barricade_Logs_01.et";
	static const ResourceName LOGS_2 = "{57FB5E487EC91D63}Prefabs/Compositions/Misc/SubCompositions/Barricade_Logs_02.et";
	static const ResourceName LOGS_3 = "{B26712B1722C8A08}Prefabs/Compositions/Misc/SubCompositions/Barricade_Logs_03.et";

	static const int ROADBLOCK_COST = 20; // the medium checkpoint's; Cost() per size
	static const int DISMANTLE_REFUND = 10;
	static const float BUILD_DURATION_S = 45.0;
	static const float MAX_ROAD_DIST = 20.0;
	static const float SNAP_END_SLACK = 2.0; // m: a piece the click falls inside of beats a nearer piece end by this
	static const float MIN_BASE_DIST = 120.0;
	static const float MIN_ROADBLOCK_DIST = 180.0;
	static const float CP_RADIUS = 15.0;
	static const float TRAVEL_SPEED = 1.0; // m/s allowed for the walk to the road, plus 3 minutes
	static const float STAGE_GAP = 4.0;    // the squad waits this far outside the footprint

	static bool IsEast(string factionKey)
	{
		return factionKey == "USSR" || factionKey.Contains("RU") || factionKey.Contains("AFRF") || factionKey.Contains("SLA");
	}

	static bool IsFia(string factionKey)
	{
		return !IsEast(factionKey) && factionKey.Contains("FIA");
	}

	// What a side actually builds: FIA has no checkpoint compositions, so every order is a log
	// barricade (priced as one).
	static int TypeFor(int type, string factionKey)
	{
		if (IsFia(factionKey)) return ACR_ERoadblockType.BARRICADE;
		return type;
	}

	static ResourceName PrefabFor(string factionKey)
	{
		return PrefabFor(ACR_ERoadblockType.MEDIUM, factionKey, 6);
	}

	// Barricades by road width, the add-on's steps: small up to 6 m, medium up to 9 m, else large
	// (FIA's logs: 5 m, 8 m; the large logs for a large order).
	static ResourceName PrefabFor(int type, string factionKey, float roadWidth)
	{
		bool east = IsEast(factionKey);
		if (IsFia(factionKey))
		{
			if (type == ACR_ERoadblockType.LARGE || roadWidth > 8) return LOGS_3;
			if (roadWidth > 5) return LOGS_2;
			return LOGS_1;
		}
		if (type == ACR_ERoadblockType.BARRICADE)
		{
			if (roadWidth <= 6)
			{
				if (east) return BARRICADE_S_USSR;
				return BARRICADE_S_US;
			}
			if (roadWidth <= 9)
			{
				if (east) return BARRICADE_M_USSR;
				return BARRICADE_M_US;
			}
			if (east) return BARRICADE_L_USSR;
			return BARRICADE_L_US;
		}
		if (type == ACR_ERoadblockType.SMALL)
		{
			if (east) return CHECKPOINT_S_USSR;
			return CHECKPOINT_S_US;
		}
		if (type == ACR_ERoadblockType.LARGE)
		{
			if (east) return CHECKPOINT_L_USSR;
			return CHECKPOINT_L_US;
		}
		if (east) return PREFAB_USSR;
		return PREFAB_US;
	}

	// RP, or construction truck supplies (vanilla composition prices).
	static int Cost(int type)
	{
		switch (type)
		{
			case ACR_ERoadblockType.SMALL: return 12;
			case ACR_ERoadblockType.LARGE: return 30;
			case ACR_ERoadblockType.BARRICADE: return 8;
		}
		return ROADBLOCK_COST;
	}

	static int Supplies(int type)
	{
		switch (type)
		{
			case ACR_ERoadblockType.SMALL: return 75;
			case ACR_ERoadblockType.LARGE: return 350;
			case ACR_ERoadblockType.BARRICADE: return 40;
		}
		return 225;
	}

	static float Duration(int type)
	{
		switch (type)
		{
			case ACR_ERoadblockType.SMALL: return 35 * s_fWorkScaleForTest;
			case ACR_ERoadblockType.LARGE: return 70 * s_fWorkScaleForTest;
			case ACR_ERoadblockType.BARRICADE: return 25 * s_fWorkScaleForTest;
		}
		return BUILD_DURATION_S * s_fWorkScaleForTest;
	}
	static float s_fWorkScaleForTest = 1;

	// The footprint a vehicle must leave before the composition goes in.
	static float Footprint(int type)
	{
		if (type == ACR_ERoadblockType.LARGE) return 11;
		if (type == ACR_ERoadblockType.MEDIUM) return 8;
		return 6;
	}

	static string Name(int type)
	{
		switch (type)
		{
			case ACR_ERoadblockType.SMALL: return "Roadblock S";
			case ACR_ERoadblockType.LARGE: return "Roadblock L";
			case ACR_ERoadblockType.BARRICADE: return "Barricade";
		}
		return "Roadblock";
	}

	// The map panel's button for the order.
	static string Button(int type)
	{
		if (type == ACR_ERoadblockType.MEDIUM) return "Roadblock M";
		return Name(type);
	}

	static string Hint(int type, string factionKey = "")
	{
		if (IsFia(factionKey))
			return string.Format("Right-click a road: log barricade (%1 RP or %2 truck supplies; click Barricade again to cancel)", Cost(ACR_ERoadblockType.BARRICADE), Supplies(ACR_ERoadblockType.BARRICADE));
		string what = "checkpoint with a machine gun";
		if (type == ACR_ERoadblockType.SMALL) what = "small checkpoint, no machine gun";
		if (type == ACR_ERoadblockType.LARGE) what = "large checkpoint with a heavy machine gun";
		if (type == ACR_ERoadblockType.BARRICADE) what = "barricade, no gun";
		return string.Format("Right-click a road: %1 (%2 RP or %3 truck supplies; click %4 again to cancel)", what, Cost(type), Supplies(type), Button(type));
	}

	// The roadblock a request orders (-1: not a roadblock request).
	static int TypeOfRequest(int command)
	{
		switch (command)
		{
			case ACR_ERequest.ORDER_ROADBLOCK: return ACR_ERoadblockType.MEDIUM;
			case ACR_ERequest.ORDER_ROADBLOCK_S: return ACR_ERoadblockType.SMALL;
			case ACR_ERequest.ORDER_ROADBLOCK_L: return ACR_ERoadblockType.LARGE;
			case ACR_ERequest.ORDER_BARRICADE: return ACR_ERoadblockType.BARRICADE;
		}
		return -1;
	}

	// Who pays: the squad's construction truck (ACR_Squad.ConstructionTruck: never one reserved
	// for another squad's FOB) when truckMayPay and it holds the supplies (truck, loads), else RP
	// (truck null). Both count what the squad's current job gives back, since the new order
	// replaces it first. False: neither can pay.
	static bool Payer(ACR_Squad squad, int type, bool truckMayPay, out IEntity truck, out float loads)
	{
		truck = null;
		loads = Supplies(type) / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!squad || !manager) return false;
		if (!manager.m_Logistics) manager.m_Logistics = new ACR_Logistics(manager);
		if (truckMayPay)
		{
			IEntity candidate = squad.ConstructionTruck();
			if (candidate)
			{
				float capacity = ACR_Logistics.Capacity(ACR_Logistics.ServiceOf(candidate));
				if (Math.Min(manager.m_Logistics.StockOf(candidate) + squad.PendingLoads(candidate), capacity) >= loads)
				{
					truck = candidate;
					return true;
				}
			}
		}
		string factionKey = "";
		if (squad.GetFaction()) factionKey = squad.GetFaction().GetFactionKey();
		return manager.m_Bases.GetRP(factionKey) + squad.PendingRP() >= Cost(type);
	}

	// Beside the road, clear of the footprint: the squad heads there, so a truck it brings does
	// not park on the spot. On the side of its post (the composition's -X side, where every
	// checkpoint has its gun: ACR_RoadblockLane.Post), so the squad crosses the road before the
	// wire is up: from the far side it walked through the finished checkpoint's barbed wire to its
	// post and bled (srv:rblane, 27 Oct). With the lane off, the squad's own side as before.
	static vector Stage(vector site, float yaw, int type, vector from)
	{
		float rad = yaw * Math.DEG2RAD;
		vector side = Vector(Math.Cos(rad), 0, -Math.Sin(rad));
		vector toSquad = from - site;
		if (!ACR_RoadblockLane.s_bOff) side = side * -1;
		else if (toSquad[0] * side[0] + toSquad[2] * side[2] < 0) side = side * -1;
		return ACR_Manager.Ground(site + side * (Footprint(type) + STAGE_GAP));
	}

	static float RoadWidthAt(vector pos)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return 6;
		BaseRoad road;
		float distance;
		aiWorld.GetRoadNetworkManager().GetClosestRoad(pos, road, distance);
		if (!road || road.GetWidth() <= 0) return 6;
		return road.GetWidth();
	}

	// Like any new order, a roadblock order replaces the squad's current job (a roadblock or FOB
	// build too), which refunds itself; Payer counts that refund.
	static bool CanOrder(ACR_Squad squad, out string refusal, int type = ACR_ERoadblockType.MEDIUM, bool truckMayPay = true)
	{
		if (!squad || squad.CountAlive() <= 0)
		{
			refusal = "squad is unavailable";
			return false;
		}
		if (squad.IsVehicle())
		{
			refusal = "vehicles cannot construct roadblocks";
			return false;
		}
		string factionKey = "";
		if (squad.GetFaction()) factionKey = squad.GetFaction().GetFactionKey();
		type = TypeFor(type, factionKey);
		IEntity truck;
		float loads;
		if (!Payer(squad, type, truckMayPay, truck, loads))
		{
			refusal = string.Format("not enough RP (%1 needed) or construction truck supplies (%2)", Cost(type), Supplies(type));
			if (!truckMayPay) refusal = string.Format("not enough RP (%1 needed)", Cost(type));
			return false;
		}
		return true;
	}

	// A road piece's nearest point to pos (XZ): its segment, the point (at pos's height) and the
	// distance; 'inside' unless the point is clamped to the piece's very first or last point.
	static float NearestOnPiece(notnull array<vector> points, vector pos, out int segment, out vector point, out bool inside)
	{
		segment = -1;
		inside = false;
		float best = float.MAX;
		int last = points.Count() - 2;
		for (int i = 0; i <= last; i++)
		{
			vector p1 = points[i];
			vector seg = points[i + 1] - p1;
			seg[1] = 0;
			float lenSq = seg.LengthSq();
			if (lenSq < 0.01) continue;
			vector v = pos - p1;
			v[1] = 0;
			float raw = (v[0] * seg[0] + v[2] * seg[2]) / lenSq;
			float t = Math.Clamp(raw, 0.0, 1.0);
			vector proj = p1 + seg * t;
			proj[1] = pos[1];
			float d = vector.DistanceXZ(pos, proj);
			if (d >= best) continue;
			best = d;
			segment = i;
			point = proj;
			inside = !((i == 0 && raw <= 0) || (i == last && raw >= 1));
		}
		return best;
	}

	static bool ValidateSite(vector pos, out vector outRoadPos, out float outYaw, out string refusal, string faction = "", vector facing = vector.Zero)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
		{
			refusal = "road network unavailable";
			return false;
		}

		// Real road pieces only: navlinks (the network's invisible junction links) put the
		// roadblock in the middle of the junction.
		BaseRoad road;
		float roadDist;
		aiWorld.GetRoadNetworkManager().GetClosestRoad(pos, road, roadDist, true);

		// The road piece the click is on: of the pieces near it, the one whose line is nearest,
		// one the click falls inside of before one it only meets at an end. The piece
		// GetClosestRoad gives can be the neighbouring one, and clamped to its end a click by a
		// joint or a T-crossing slid to the joint or the T, off the road clicked (25 Oct).
		array<BaseRoad> nearRoads = {};
		// (A tall box: road pieces' bounds don't sit at the ground's height.)
		aiWorld.GetRoadNetworkManager().GetRoadsInAABB(pos - Vector(MAX_ROAD_DIST, 1000, MAX_ROAD_DIST), pos + Vector(MAX_ROAD_DIST, 1000, MAX_ROAD_DIST), nearRoads);
		// (GetClosestRoad can give a piece 40 m off with one 2 m from the click: its distance
		// decides nothing.)
		if (road && !nearRoads.Contains(road)) nearRoads.Insert(road);
		array<vector> points = {};
		int bestIdx = -1;
		float bestDist = float.MAX;
		bool bestInside;
		vector bestProj = pos;
		foreach (BaseRoad piece : nearRoads)
		{
			if (!piece) continue;
			array<vector> piecePoints = {};
			piece.GetPoints(piecePoints);
			int idx;
			vector proj;
			bool inside;
			float dist = NearestOnPiece(piecePoints, pos, idx, proj, inside);
			if (idx < 0) continue;
			// Inside a piece wins unless a piece's end is more than SNAP_END_SLACK m nearer.
			bool better = dist < bestDist;
			if (inside && !bestInside) better = dist < bestDist + SNAP_END_SLACK;
			else if (!inside && bestInside) better = dist + SNAP_END_SLACK < bestDist;
			if (!better) continue;
			bestDist = dist;
			bestIdx = idx;
			bestProj = proj;
			bestInside = inside;
			points = piecePoints;
		}

		if (bestIdx < 0 || bestDist > MAX_ROAD_DIST)
		{
			refusal = "target position must be on or near a road";
			return false;
		}

		vector dir = points[bestIdx + 1] - points[bestIdx];
		dir[1] = 0;
		float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;

		// Orient facing if direction specified
		if (facing != vector.Zero)
		{
			vector faceDir = facing - bestProj;
			faceDir[1] = 0;
			if (dir[0] * faceDir[0] + dir[2] * faceDir[2] < 0)
			{
				yaw += 180.0;
			}
		}

		outRoadPos = bestProj;
		outRoadPos[1] = GetGame().GetWorld().GetSurfaceY(outRoadPos[0], outRoadPos[2]);
		outYaw = yaw;

		// Validate where the composition will actually spawn, not the map click.
		// Distance check to bases
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (vector.DistanceXZ(outRoadPos, base.GetOrigin()) < MIN_BASE_DIST)
			{
				refusal = "too close to base perimeter";
				return false;
			}
		}

		// Distance check to other roadblocks
		PruneRoadblocks();
		foreach (ACR_Roadblock rb : s_aRoadblocks)
		{
			if (vector.DistanceXZ(outRoadPos, rb.m_vPos) < MIN_ROADBLOCK_DIST)
			{
				refusal = "too close to existing roadblock";
				return false;
			}
		}

		// Check for vehicles on the spot
		if (ACR_FobSlots.VehicleOnSpot(outRoadPos, 8.0))
		{
			refusal = "vehicles blocking road spot";
			return false;
		}

		return true;
	}

	// Defenders held in reserve at a checkpoint (not a plain barricade), as at a Fortify position
	// (ACR_Defenders): -acrRoadblockDefenders N, 0 (off) by default, at most 8.
	static int Defenders(int type)
	{
		if (type == ACR_ERoadblockType.BARRICADE) return 0;
		return Math.ClampInt(ACR_LogisticsRules.Choice("acrRoadblockDefenders", "0").ToInt(), 0, 8);
	}

	// The layout record's length in ints (0: none, the prefab's layout everywhere).
	static int LayoutSize(IEntity ent)
	{
		SCR_CampaignBuildingCompositionComponent comp = ACR_StructureLayout.Comp(ent);
		if (!comp) return 0;
		return comp.ACR_LayoutSize();
	}

	// lane false: no lane for anyone's vehicles (an FIA ambush's barricade is there to stop the convoy).
	static ACR_Roadblock SpawnRoadblock(string factionKey, vector pos, float yaw, string squadName = "", int type = ACR_ERoadblockType.MEDIUM, bool lane = true)
	{
		ResourceName prefab = PrefabFor(type, factionKey, RoadWidthAt(pos));
		type = TypeFor(type, factionKey);
		if (prefab.IsEmpty()) return null;

		Resource res = Resource.Load(prefab);
		if (!res.IsValid()) return null;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), spawnParams.Transform);
		spawnParams.Transform[3] = pos;

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), spawnParams);
		if (!ent) return null;

		ent.SetYawPitchRoll(Vector(yaw, 0, 0));
		ACR_GroundTilt.Place(ent, prefab, pos, yaw); // barricades lie on the ground; checkpoints stay upright
		// A lane our own trucks and APCs can drive, before the navmesh is rebuilt round it.
		int laneMoved;
		if (lane) laneMoved = ACR_RoadblockLane.Open(ent, type == ACR_ERoadblockType.BARRICADE);
		// Every machine shows the lane: a client builds the composition from its prefab (ACR_StructureLayout).
		if (laneMoved > 0) ACR_StructureLayout.Record(ent);

		SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(ent.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (comp)
		{
			comp.ACR_SetFaction(factionKey);
			comp.ACR_MarkBuildCharged();
			comp.ACR_SetCompositionSpawned(true);
		}

		ACR_Navmesh.RebuildAfterSpawn(ent);

		ACR_Roadblock rb = new ACR_Roadblock();
		rb.m_vPos = pos;
		rb.m_fYaw = yaw;
		rb.m_sFaction = factionKey;
		rb.m_Entity = ent;
		rb.m_Turret = FindTurretInHierarchy(ent);
		rb.m_sBuilderSquad = squadName;
		rb.m_fSpawnedAt = GetGame().GetWorld().GetWorldTime();
		rb.m_iType = type;
		rb.m_iLaneMoved = laneMoved;
		rb.m_vPost = pos;
		if (lane && !ACR_RoadblockLane.s_bOff)
		{
			rb.m_vPost = ACR_RoadblockLane.Post(ent, rb.m_Turret);
			rb.m_fPostRadius = ACR_RoadblockLane.POST_RADIUS;
		}
		int defenders = Defenders(type);
		if (defenders > 0)
		{
			rb.m_Defenders = ACR_Defenders.Add(factionKey, rb.m_vPost, string.Format("%1 roadblock", factionKey), rb.m_fPostRadius);
			rb.m_Defenders.m_iReserve = defenders;
		}

		s_aRoadblocks.Insert(rb);
		PrintFormat("[ACR] ROADBLOCK spawned for %1 at %2 (%3, turret=%4, lane: %5 pieces moved, post %6 m off, layout record %7 bytes)", factionKey, pos, Name(type), rb.m_Turret != null, laneMoved, vector.DistanceXZ(rb.m_vPost, pos), ACR_StructureLayout.Bytes(LayoutSize(ent)));
		return rb;
	}

	static void DismantleRoadblock(ACR_Roadblock rb)
	{
		if (!rb) return;
		if (rb.m_Turret) VacateTurret(rb.m_Turret);
		if (rb.m_Entity) ACR_Navmesh.DeleteStructure(rb.m_Entity);

		// Half its price back where it was paid from (the medium checkpoint's DISMANTLE_REFUND RP);
		// a destroyed truck's half is lost.
		ACR_Manager manager = ACR_Manager.GetInstance();
		string refund = "nothing";
		if (rb.m_fPaidLoads > 0)
		{
			if (manager && manager.m_Logistics && rb.m_PaidTruck && ACR_Logistics.Health(rb.m_PaidTruck) > 0)
			{
				manager.m_Logistics.AddStock(rb.m_PaidTruck, rb.m_fPaidLoads * 0.5);
				refund = string.Format("%1 supplies to the construction truck", Math.Round(rb.m_fPaidLoads * 0.5 * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION));
			}
		}
		else
		{
			int rp = Cost(rb.m_iType) * DISMANTLE_REFUND / ROADBLOCK_COST;
			if (manager) manager.m_Bases.Refund(rb.m_sFaction, rp);
			refund = string.Format("%1 RP", rp);
		}

		// Removing it from the list frees it when the caller holds no strong reference.
		vector pos = rb.m_vPos;
		string faction = rb.m_sFaction;
		ACR_Defenders.Remove(rb.m_Defenders);
		s_aRoadblocks.RemoveItem(rb);
		PrintFormat("[ACR] ROADBLOCK dismantled at %1, refunded %2 to %3", pos, refund, faction);
	}

	static IEntity FindTurretInHierarchy(IEntity root)
	{
		if (!root) return null;
		if (root.FindComponent(TurretControllerComponent)) return root;
		SCR_BaseCompartmentManagerComponent comp = SCR_BaseCompartmentManagerComponent.Cast(root.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (comp)
		{
			array<BaseCompartmentSlot> slots = {};
			comp.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
			if (!slots.IsEmpty()) return root;
		}
		IEntity child = root.GetChildren();
		while (child)
		{
			IEntity found = FindTurretInHierarchy(child);
			if (found) return found;
			child = child.GetSibling();
		}
		return null;
	}

	static bool SeatSoldierInTurret(IEntity turretEnt, ChimeraCharacter soldier)
	{
		if (!turretEnt || !soldier || !ACR_Respawn.IsAlive(soldier)) return false;
		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(turretEnt.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compMgr) return false;
		array<BaseCompartmentSlot> slots = {};
		compMgr.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
		if (slots.IsEmpty()) return false;
		BaseCompartmentSlot seat = slots[0];
		if (seat.GetOccupant()) return false;
		CompartmentAccessComponent access = soldier.GetCompartmentAccessComponent();
		if (!access) return false;
		return access.GetInVehicle(turretEnt, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
	}

	static bool IsTurretOccupied(IEntity turretEnt)
	{
		if (!turretEnt) return false;
		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(turretEnt.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compMgr) return false;
		array<BaseCompartmentSlot> slots = {};
		compMgr.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
		if (slots.IsEmpty()) return false;
		return slots[0].GetOccupant() != null;
	}

	static void VacateTurret(IEntity turretEnt)
	{
		if (!turretEnt) return;
		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(turretEnt.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compMgr) return;
		array<BaseCompartmentSlot> slots = {};
		compMgr.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
		if (slots.IsEmpty()) return;
		IEntity occupant = slots[0].GetOccupant();
		if (!occupant) return;
		ChimeraCharacter character = ChimeraCharacter.Cast(occupant);
		if (!character) return;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (access) access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
	}

	static void PruneRoadblocks()
	{
		for (int i = s_aRoadblocks.Count() - 1; i >= 0; i--)
		{
			ACR_Roadblock rb = s_aRoadblocks[i];
			if (!rb || !rb.m_Entity || rb.m_Entity.IsDeleted())
			{
				if (rb) ACR_Defenders.Remove(rb.m_Defenders);
				s_aRoadblocks.RemoveOrdered(i);
			}
		}
	}

	// Our own AI drivers take a roadblock's lane at LANE_SPEED_KMH from LANE_SLOW_RANGE out
	// (ACR_Squad.LaneSpeed): the chicane is no place for a truck at road speed.
	static const float LANE_SPEED_KMH = 12;
	static const float LANE_SLOW_RANGE = 30;

	// The nearest of the side's roadblocks within range of pos (null: none).
	static ACR_Roadblock OwnNear(vector pos, string factionKey, float range = LANE_SLOW_RANGE)
	{
		ACR_Roadblock best = null;
		float bestDist = range;
		foreach (ACR_Roadblock rb : s_aRoadblocks)
		{
			if (!rb || !rb.m_Entity || rb.m_sFaction != factionKey) continue;
			float d = vector.DistanceXZ(pos, rb.m_vPos);
			if (d >= bestDist) continue;
			bestDist = d;
			best = rb;
		}
		return best;
	}

	static ACR_Roadblock FindNearest(vector pos, float maxDist = 50.0)
	{
		PruneRoadblocks();
		ACR_Roadblock best = null;
		float bestDist = maxDist;
		foreach (ACR_Roadblock rb : s_aRoadblocks)
		{
			float d = vector.DistanceXZ(pos, rb.m_vPos);
			if (d < bestDist)
			{
				bestDist = d;
				best = rb;
			}
		}
		return best;
	}
}
