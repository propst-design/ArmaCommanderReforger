// FOB defences (Jacob, 20 Oct): a separate three-tier upgrade, ordered from the FOB panel like
// the building slots (same supplies, timer, contest pause, engineer speed-up and one-build lock).
// Fortifications stand at 7 preset spots on a 50 m ring, in the gaps between the building
// footprints at their largest tier, facing outwards.
//   Defences I:   sandbag positions at the 4 post spots.
//   Defences II:  machine-gun nests at the 3 nest spots (the sandbags stay).
//   Defences III: the posts become bunkers and guard towers (FIA: bunkers).
// A blocked spot tries points beside it (FindSpot), then is skipped; a tier needs at least one
// of its spots. Trees never block a piece and slopes up to MAX_RISE are fine. The upgrade is
// lost when every piece is gone. ACR_FobSlots dispatches here for SLOT_DEFENCES.

class ACR_FobDefences : Managed
{
	static const int SPOT_COUNT = 7;
	static const int POST_COUNT = 4;    // spots 0-3: sandbag positions, later bunkers/towers
	static const float RING = 50.0;
	static const float PIECE_RADIUS = 6.0;
	static const float MAX_RISE = 4.0;  // m across a piece's 12 m footprint (buildings: 1.5)

	static const ResourceName SANDBAGS_US = "{B8138FFE79F61D33}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SandbagPosition_S_US_01.et";
	static const ResourceName SANDBAGS_USSR = "{9BC88E502B836DBF}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SandbagPosition_S_USSR_01.et";
	static const ResourceName SANDBAGS_FIA = "{CE74090473B270DE}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SandbagPosition_S_FIA_01.et";

	static const ResourceName NEST_US = "{02C9F8198A7B9897}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_MachineGunNest_S_US_01.et";
	static const ResourceName NEST_USSR = "{047B9C8AAB50CE0F}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_MachineGunNest_S_USSR_01.et";
	static const ResourceName NEST_FIA = "{336DB98AC1B21594}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_MachineGunNest_S_FIA_02.et";

	static const ResourceName BUNKER_US = "{E87F2E0B5C56A1C3}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Bunker_S_US_01.et";
	static const ResourceName BUNKER_USSR = "{4885DEA01D687DB3}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Bunker_S_USSR_01.et";
	static const ResourceName BUNKER_FIA = "{1EED86CBA983241E}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Bunker_S_FIA_01.et";

	static const ResourceName TOWER_US = "{E5043AB2B2C5DFE1}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_GuardTower_S_US_01.et";
	static const ResourceName TOWER_USSR = "{9C13C175AB00BCA8}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_GuardTower_S_USSR_01.et";

	// Compass bearing (degrees from north) of each spot: posts NE, SE, SW, NW; nests E, S, W.
	static float SpotBearing(int spot)
	{
		switch (spot)
		{
			case 0: return 45;
			case 1: return 129;
			case 2: return 222;
			case 3: return 330;
			case 4: return 79;
			case 5: return 190;
			case 6: return 250;
		}
		return 0;
	}

	static vector SpotOffset(int spot)
	{
		float rad = SpotBearing(spot) * Math.DEG2RAD;
		return Vector(Math.Sin(rad) * RING, 0, Math.Cos(rad) * RING);
	}

	static vector SpotPosition(vector center, int spot)
	{
		vector pos = center + SpotOffset(spot);
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		return pos;
	}

	// Each piece faces outwards (their weapons and firing ports point along +Z).
	static float SpotYaw(int spot)
	{
		return BearingYaw(SpotBearing(spot));
	}

	static float BearingYaw(float bearing)
	{
		float yaw = bearing;
		while (yaw > 180) yaw -= 360;
		while (yaw <= -180) yaw += 360;
		return yaw;
	}

	// The spots a tier places or replaces a piece on.
	static bool SpotChangesAt(int spot, int tier)
	{
		if (spot < POST_COUNT) return tier == 0 || tier == 2;
		return tier == 1;
	}

	static ResourceName PrefabFor(int spot, string factionKey, int tier)
	{
		bool isEast = false;
		if (factionKey == "USSR" || factionKey == "RHS_AFRF") isEast = true;
		bool isFia = false;
		if (factionKey == "FIA") isFia = true;
		if (spot >= POST_COUNT)
		{
			if (tier < 1) return "";
			if (isEast) return NEST_USSR;
			if (isFia) return NEST_FIA;
			return NEST_US;
		}
		if (tier >= 2)
		{
			// Odd posts get a guard tower; FIA has none, so all four are bunkers.
			if (spot % 2 == 1 && !isFia)
			{
				if (isEast) return TOWER_USSR;
				return TOWER_US;
			}
			if (isEast) return BUNKER_USSR;
			if (isFia) return BUNKER_FIA;
			return BUNKER_US;
		}
		if (isEast) return SANDBAGS_USSR;
		if (isFia) return SANDBAGS_FIA;
		return SANDBAGS_US;
	}

	static string TierName(int tier)
	{
		switch (tier)
		{
			case 0: return "Defences I (sandbag positions)";
			case 1: return "Defences II (MG nests)";
			case 2: return "Defences III (bunkers and towers)";
		}
		return "Defences";
	}

	// Vanilla composition prices: 4 x 100, 3 x 85, 2 x 100 + 2 x 240.
	static int Cost(int tier)
	{
		switch (tier)
		{
			case 0: return 400;
			case 1: return 255;
			case 2: return 680;
		}
		return 400;
	}

	static float Duration(int tier)
	{
		if (tier >= 2) return 60.0;
		return 40.0;
	}

	static ACR_FobSlot Create()
	{
		ACR_FobSlot slot = new ACR_FobSlot();
		slot.m_iSlotIndex = ACR_FobSlots.SLOT_DEFENCES;
		slot.m_eKind = ACR_EStructure.DEFENCES;
		slot.m_eState = ACR_EFobSlotState.EMPTY;
		slot.m_sName = TierName(0);
		slot.m_iCost = Cost(0);
		slot.m_fDuration = Duration(0);
		slot.m_aPieces = {};
		for (int i = 0; i < SPOT_COUNT; i++) slot.m_aPieces.Insert(null);
		return slot;
	}

	// What a spot check ignores: the HQ and every piece already standing.
	static void IgnoreFor(ACR_Base base, ACR_FobSlot slot, notnull array<IEntity> ignore)
	{
		ACR_FobSlots.IgnoreFor(base, null, ignore);
		if (!slot || !slot.m_aPieces) return;
		foreach (IEntity piece : slot.m_aPieces)
			if (piece) ignore.Insert(piece);
	}

	static bool SpotClear(ACR_Base base, ACR_FobSlot slot, int spot, out string reason)
	{
		vector pos;
		float yaw;
		return FindSpot(base, slot, spot, pos, yaw, reason);
	}

	// Where a spot's piece goes: where its current piece stands, else the spot itself or the
	// first clear point beside it (along the ring, then a little further out; never inwards,
	// where the building footprints are). Bought defences go in even where they clip trees
	// and bushes, and tolerate a steeper rise than buildings (Jacob, 20 Oct).
	static bool FindSpot(ACR_Base base, ACR_FobSlot slot, int spot, out vector pos, out float yaw, out string reason)
	{
		reason = "";
		vector center = base.GetOrigin();
		IEntity old;
		if (slot && slot.m_aPieces && spot < slot.m_aPieces.Count()) old = slot.m_aPieces[spot];
		if (old)
		{
			pos = old.GetOrigin();
			vector dir = pos - center;
			yaw = BearingYaw(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG);
			return true;
		}
		array<IEntity> ignore = {};
		IgnoreFor(base, slot, ignore);
		array<float> rings = {RING, RING + 7};
		array<float> shifts = {0, 8, -8, 16, -16};
		foreach (float ring : rings)
		{
			foreach (float shift : shifts)
			{
				float bearing = SpotBearing(spot) + shift;
				float rad = bearing * Math.DEG2RAD;
				vector candidate = center + Vector(Math.Sin(rad) * ring, 0, Math.Cos(rad) * ring);
				candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
				string why;
				if (!ACR_FobSlots.s_bSkipSpotChecksForTest && !ACR_FobBuild.FootprintClear(candidate, PIECE_RADIUS, ignore, why, true, MAX_RISE))
				{
					if (reason.IsEmpty()) reason = why;
					continue;
				}
				pos = candidate;
				yaw = BearingYaw(bearing);
				reason = "";
				return true;
			}
		}
		return false;
	}

	// Checks if a candidate FOB center has room for defence spot on its 50 m ring.
	static bool DefenceSpotClear(vector center, int spot, out string reason)
	{
		reason = "";
		if (ACR_FobSlots.s_bSkipSpotChecksForTest) return true;
		array<float> rings = {RING, RING + 7};
		array<float> shifts = {0, 8, -8, 16, -16};
		foreach (float ring : rings)
		{
			foreach (float shift : shifts)
			{
				float bearing = SpotBearing(spot) + shift;
				float rad = bearing * Math.DEG2RAD;
				vector candidate = center + Vector(Math.Sin(rad) * ring, 0, Math.Cos(rad) * ring);
				candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
				string why;
				if (!ACR_FobBuild.FootprintClear(candidate, PIECE_RADIUS, null, why, true, MAX_RISE))
				{
					if (reason.IsEmpty()) reason = why;
					continue;
				}
				reason = "";
				return true;
			}
		}
		return false;
	}

	// A tier can be built when at least one of its spots is clear.
	static bool CheckTier(ACR_Base base, ACR_FobSlot slot, int tier, out string reason)
	{
		reason = "";
		string firstWhy = "";
		for (int spot = 0; spot < SPOT_COUNT; spot++)
		{
			if (!SpotChangesAt(spot, tier)) continue;
			string why;
			if (SpotClear(base, slot, spot, why)) return true;
			if (firstWhy.IsEmpty()) firstWhy = why;
		}
		reason = "every spot blocked (" + firstWhy + ")";
		return false;
	}

	static bool VehicleOnSpots(ACR_Base base, int tier)
	{
		for (int spot = 0; spot < SPOT_COUNT; spot++)
			if (SpotChangesAt(spot, tier) && ACR_FobSlots.VehicleOnSpot(SpotPosition(base.GetOrigin(), spot), PIECE_RADIUS)) return true;
		return false;
	}

	static int Standing(ACR_FobSlot slot)
	{
		int count;
		if (!slot || !slot.m_aPieces) return 0;
		foreach (IEntity piece : slot.m_aPieces)
			if (piece) count++;
		return count;
	}

	// Drop pieces that were dismantled, wrecked or destroyed.
	static void Prune(ACR_FobSlot slot)
	{
		if (!slot || !slot.m_aPieces) return;
		for (int i = 0; i < slot.m_aPieces.Count(); i++)
		{
			IEntity piece = slot.m_aPieces[i];
			if (!piece || ACR_Logistics.Health(piece) > 0) continue;
			RemovePiece(slot, i);
		}
	}

	protected static void RemovePiece(ACR_FobSlot slot, int spot)
	{
		IEntity piece = slot.m_aPieces[spot];
		slot.m_aPieces[spot] = null;
		if (!piece) return;
		SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(piece.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (comp) ACR_Structures.s_Registered.RemoveItem(comp);
		ACR_Navmesh.DeleteStructure(piece);
	}

	static void Clear(ACR_FobSlot slot)
	{
		if (!slot) return;
		if (!slot.m_aPieces) slot.m_aPieces = {};
		for (int i = 0; i < slot.m_aPieces.Count(); i++) RemovePiece(slot, i);
		while (slot.m_aPieces.Count() < SPOT_COUNT) slot.m_aPieces.Insert(null);
	}

	// Place (or replace) the pieces of the slot's new tier on every clear spot.
	static void Complete(ACR_Base base, ACR_FobSlot slot)
	{
		if (!slot.m_aPieces) Clear(slot);
		int placed;
		int skipped;
		for (int spot = 0; spot < SPOT_COUNT; spot++)
		{
			if (!SpotChangesAt(spot, slot.m_iTier)) continue;
			string why;
			vector pos;
			float yaw;
			if (!FindSpot(base, slot, spot, pos, yaw, why))
			{
				skipped++;
				PrintFormat("[ACR] FOB %1: defence spot %2 skipped (%3)", base.m_sName, spot, why);
				continue;
			}
			RemovePiece(slot, spot);
			ACR_FobBuild.FellTrees(pos, PIECE_RADIUS);
			IEntity piece = ACR_FobSlots.SpawnComposition(PrefabFor(spot, base.m_sOwner, slot.m_iTier), pos, yaw, base, slot.m_iTier);
			slot.m_aPieces[spot] = piece;
			if (piece) placed++;
		}
		PrintFormat("[ACR] FOB %1: %2 placed %3 pieces (%4 spots blocked, %5 standing)", base.m_sName, slot.m_sName, placed, skipped, Standing(slot));
	}

	static void TransferOwner(ACR_FobSlot slot, string faction)
	{
		if (!slot || !slot.m_aPieces) return;
		ACR_Manager manager = ACR_Manager.GetInstance();
		foreach (IEntity piece : slot.m_aPieces)
		{
			if (!piece) continue;
			SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(piece.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (comp) comp.ACR_SetFaction(faction);
			if (!manager || !manager.m_Structures) continue;
			ACR_Structure structure = manager.m_Structures.Find(piece);
			if (structure) structure.m_sFaction = faction;
		}
	}
}
