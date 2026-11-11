// ACR_TestFobSlots: Regression tests for Stage 2 FOB preset upgrade slots & panel.
// Checks: 8 preset slot configurations (cost, duration, layout), OrderUpgrade validation,
// local supply deduction, contest pausing, established work crew 1x speed, engineer 2x speedup,
// completion spawning composition, registration & activation in ACR_Structures,
// already-built refusal, sequential building, destruction reset, and FOB cleanup.

class ACR_TestFobSlots
{
	static const int CHECKS = 23;

	static void Run(ACR_Manager manager, ACR_Base fob, ACR_Squad squad, IEntity truck)
	{
		if (!manager || !fob || !squad || !truck)
		{
			ACR_TestRegress.Abort("structures", "FOB slots test fixture missing");
			return;
		}

		string why;
		vector truckPos = truck.GetOrigin();

		// 1. Verify 8 preset slots configuration
		bool slotsConfigOk = (fob.m_aFobSlots && fob.m_aFobSlots.Count() == ACR_FobSlots.SLOT_COUNT);
		if (slotsConfigOk)
		{
			if (ACR_FobSlots.SlotCost(0) != 250 || ACR_FobSlots.SlotDuration(0) != 40.0) slotsConfigOk = false;
			if (ACR_FobSlots.SlotCost(1, 0) != 150 || ACR_FobSlots.SlotCost(1, 1) != 150) slotsConfigOk = false;
			if (ACR_FobSlots.SlotCost(2) != 325 || ACR_FobSlots.SlotDuration(2) != 30.0) slotsConfigOk = false;
			if (ACR_FobSlots.SlotCost(3) != 50 || ACR_FobSlots.SlotDuration(3) != 30.0) slotsConfigOk = false;
			if (ACR_FobSlots.SlotCost(4) != 750 || ACR_FobSlots.SlotDuration(4) != 45.0) slotsConfigOk = false;
			if (ACR_FobSlots.SlotCost(5) != 250 || ACR_FobSlots.SlotDuration(5) != 30.0) slotsConfigOk = false;
			if (ACR_FobSlots.SlotCost(6, 0) != 275 || ACR_FobSlots.SlotCost(6, 1) != 200) slotsConfigOk = false;
			if (ACR_FobSlots.SlotCost(7) != 175 || ACR_FobSlots.SlotDuration(7) != 30.0) slotsConfigOk = false;
		}
		ACR_TestRegress.Check("fobslots: 8 preset slots initialized with correct costs and durations", slotsConfigOk, "");

		// 2. OrderUpgrade refuses non-FOB base
		ACR_Base ordinaryBase = ACR_Base.s_All[0];
		ACR_TestRegress.Check("fobslots: OrderUpgrade refuses non-FOB base", ACR_FobSlots.OrderUpgradeBase(manager, "US", ordinaryBase, 0, why) == "" && why.Contains("Invalid FOB"), why);

		// 3. OrderUpgrade refuses enemy FOB
		ACR_TestRegress.Check("fobslots: OrderUpgrade refuses enemy FOB", ACR_FobSlots.OrderUpgradeBase(manager, "USSR", fob, 0, why) == "" && why.Contains("another faction"), why);

		// 4. OrderUpgrade refuses contested FOB
		fob.m_bContested = true;
		ACR_TestRegress.Check("fobslots: OrderUpgrade refuses contested FOB", ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, 0, why) == "" && why.Contains("contested"), why);
		fob.m_bContested = false;

		// 5. OrderUpgrade refuses invalid slot index
		ACR_TestRegress.Check("fobslots: OrderUpgrade refuses invalid slot index", ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, 99, why) == "" && why.Contains("Invalid upgrade slot"), why);

		// 6. OrderUpgrade refuses insufficient supplies
		SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
		if (store) store.SetResourceValue(100);
		ACR_TestRegress.Check("fobslots: OrderUpgrade refuses insufficient supplies", ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, 0, why) == "" && why.Contains("Needs 250 supplies"), why);

		// 7. OrderUpgrade accepts valid order and deducts supplies
		if (store) store.SetResourceValue(500);
		string result = ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_RELAY, why);
		ACR_FobSlot slotRelay = fob.m_aFobSlots[ACR_FobSlots.SLOT_RELAY];
		bool orderOk = (result.Contains("Radio Relay") && store.GetResourceValue() == 250 && slotRelay.m_eState == ACR_EFobSlotState.BUILDING);
		ACR_TestRegress.Check("fobslots: OrderUpgrade accepts valid order and deducts supplies", orderOk, result);

		// 8. OrderUpgrade refuses second upgrade while one is building
		ACR_TestRegress.Check("fobslots: OrderUpgrade refuses second upgrade while one is building", ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_AMMO, why) == "" && why.Contains("Already building"), why);

		// 9. Contest pauses build progression
		fob.m_bContested = true;
		ACR_FobSlots.Advance(fob, 10.0);
		ACR_TestRegress.Check("fobslots: contest pauses build progression", slotRelay.m_fProgress == 0, slotRelay.m_fProgress.ToString());
		fob.m_bContested = false;

		// 10. Established work crew advances build progress at standard (1x) rate
		truck.SetOrigin(fob.GetOrigin() + Vector(300, 0, 0));
		ACR_FobSlots.Advance(fob, 10.0);
		ACR_TestRegress.Check("fobslots: established work crew builds upgrade at 1x speed", slotRelay.m_fProgress == 10.0, slotRelay.m_fProgress.ToString());

		// 11. Engineer present speeds up build progress at 2x speed
		truck.SetOrigin(fob.GetOrigin() + Vector(10, 0, 0));
		ACR_FobSlots.Advance(fob, 5.0);
		ACR_TestRegress.Check("fobslots: engineer present speeds up build progress at 2x speed", slotRelay.m_fProgress == 20.0, slotRelay.m_fProgress.ToString());

		// 12. Upgrade completion transitions to BUILT and spawns composition
		ACR_FobSlots.Advance(fob, 5.0); // 20 + 5*2 = 30s (full duration)
		bool completeOk = (slotRelay.m_eState == ACR_EFobSlotState.BUILT && slotRelay.m_Entity != null);
		ACR_TestRegress.Check("fobslots: upgrade completion transitions to BUILT and spawns composition", completeOk, "");

		// 13. Completed composition activates in ACR_Structures
		manager.m_Structures.ACR_TestForceTick();
		bool relayActive = manager.m_Structures.HasRelay(fob);
		ACR_TestRegress.Check("fobslots: completed composition activates in ACR_Structures", relayActive, "");

		// 14. OrderUpgrade refuses already built single-tier slot
		ACR_TestRegress.Check("fobslots: OrderUpgrade refuses already built slot", ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_RELAY, why) == "" && why.Contains("already built"), why);

		// 15. Work crew builds next upgrade once previous completes
		if (store) store.SetResourceValue(300);
		result = ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_DEPOT, why);
		ACR_FobSlot slotDepot = fob.m_aFobSlots[ACR_FobSlots.SLOT_DEPOT];
		bool depotOrdered = (result.Contains("Supply Depot") && slotDepot.m_eState == ACR_EFobSlotState.BUILDING);
		ACR_FobSlots.Advance(fob, 15.0); // 15*2 = 30s (full duration)
		manager.m_Structures.ACR_TestForceTick();
		bool depotActive = (slotDepot.m_eState == ACR_EFobSlotState.BUILT && ACR_FobSupply.CapacityOf(fob) == 3000);
		ACR_TestRegress.Check("fobslots: work crew builds next upgrade and depot expands capacity", depotOrdered && depotActive, "");

		// 16. Destroyed structure resets slot state to EMPTY
		if (slotDepot.m_Entity)
		{
			SCR_CampaignBuildingCompositionComponent oldComp = SCR_CampaignBuildingCompositionComponent.Cast(slotDepot.m_Entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (oldComp) ACR_Structures.s_Registered.RemoveItem(oldComp);
			SCR_EntityHelper.DeleteEntityAndChildren(slotDepot.m_Entity);
			slotDepot.m_Entity = null;
		}
		ACR_FobSlots.Advance(fob, 1.0);
		bool resetOk = (slotDepot.m_eState == ACR_EFobSlotState.EMPTY && slotDepot.m_fProgress == 0 && slotDepot.m_iTier == 0);
		ACR_TestRegress.Check("fobslots: destroyed structure resets slot state to EMPTY", resetOk, "");

		// 17. Tiered upgrade for maintenance: Light (150) -> Heavy (150)
		if (store) store.SetResourceValue(400);
		string maintT1 = ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_REPAIR, why);
		ACR_FobSlot slotRepair = fob.m_aFobSlots[ACR_FobSlots.SLOT_REPAIR];
		ACR_FobSlots.Advance(fob, 15.0); // 15*2 = 30s (Light duration)
		manager.m_Structures.ACR_TestForceTick();
		float repairRateT1 = ACR_Structures.RepairFraction(slotRepair.m_Entity);
		// Faces its slot; on a slope it follows the ground as a floored piece does (ACR_GroundTilt FLOOR,
		// at most 5 deg since 6 Nov), so pitch and roll are only held to that cap, not to level.
		float floorFull, floorShare, floorCap;
		ACR_GroundTilt.FollowCurve(ACR_GroundTilt.FLOOR, floorFull, floorShare, floorCap);
		bool yawOk = slotRepair.m_Entity && Math.AbsFloat(slotRepair.m_Entity.GetYawPitchRoll()[0] - ACR_FobSlots.SlotYaw(ACR_FobSlots.SLOT_REPAIR)) < 1.0 && Math.AbsFloat(slotRepair.m_Entity.GetYawPitchRoll()[1]) <= floorCap + 0.1 && Math.AbsFloat(slotRepair.m_Entity.GetYawPitchRoll()[2]) <= floorCap + 0.1;
		int maintT1State = slotRepair.m_eState;
		int maintT1Tier = slotRepair.m_iTier;
		vector maintYpr;
		if (slotRepair.m_Entity) maintYpr = slotRepair.m_Entity.GetYawPitchRoll();
		bool maintT1Ok = (slotRepair.m_eState == ACR_EFobSlotState.BUILT && slotRepair.m_iTier == 0 && repairRateT1 == 0.15 && yawOk);

		string maintT2 = ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_REPAIR, why);
		ACR_FobSlots.Advance(fob, 23.0); // 23*2 = 46s >= 45s (Heavy duration)
		manager.m_Structures.ACR_TestForceTick();
		float repairRateT2 = ACR_Structures.RepairFraction(slotRepair.m_Entity);
		bool maintT2Ok = (slotRepair.m_eState == ACR_EFobSlotState.BUILT && slotRepair.m_iTier == 1 && repairRateT2 == 0.21);
		bool maintMaxRefused = (ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_REPAIR, why) == "" && why.Contains("already built"));
		ACR_TestRegress.Check("fobslots: tiered maintenance builds light (15%) then upgrades to heavy (21%)", maintT1Ok && maintT2Ok && maintMaxRefused, string.Format("light: '%1' state %2 tier %3 rate %4 yaw ok, tilt within %7 deg: %5 (ypr %6); ", maintT1, maintT1State, maintT1Tier, repairRateT1, yawOk, maintYpr, floorCap) + string.Format("heavy: '%1' state %2 tier %3 rate %4; max refused %5 (%6)", maintT2, slotRepair.m_eState, slotRepair.m_iTier, repairRateT2, maintMaxRefused, why));

		// 18. Tiered upgrade for barracks: Small (275, +11) -> Large (200, +19)
		if (store) store.SetResourceValue(500);
		string barrT1 = ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_BARRACKS, why);
		ACR_FobSlot slotBarracks = fob.m_aFobSlots[ACR_FobSlots.SLOT_BARRACKS];
		ACR_FobSlots.Advance(fob, 18.0); // 18*2 = 36s >= 35s (Small duration)
		manager.m_Structures.ACR_TestForceTick();
		int defendersT1 = ACR_Structures.HousingDefenders(slotBarracks.m_Entity);
		bool barrT1Ok = (slotBarracks.m_eState == ACR_EFobSlotState.BUILT && slotBarracks.m_iTier == 0 && defendersT1 == 11);

		string barrT2 = ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_BARRACKS, why);
		ACR_FobSlots.Advance(fob, 23.0); // 23*2 = 46s >= 45s (Large duration)
		manager.m_Structures.ACR_TestForceTick();
		int defendersT2 = ACR_Structures.HousingDefenders(slotBarracks.m_Entity);
		bool barrT2Ok = (slotBarracks.m_eState == ACR_EFobSlotState.BUILT && slotBarracks.m_iTier == 1 && defendersT2 == 19);
		bool barrMaxRefused = (ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_BARRACKS, why) == "" && why.Contains("already built"));
		ACR_TestRegress.Check("fobslots: tiered barracks builds small (+11) then upgrades to large (+19)", barrT1Ok && barrT2Ok && barrMaxRefused, "");

		// 19. Description reflects built upgrades and tiers
		string desc = ACR_FobSlots.Describe(fob);
		bool descOk = desc.Contains("Heavy Maintenance (built)") && desc.Contains("Barracks (Large) (built)");
		ACR_TestRegress.Check("fobslots: description reflects built upgrades and tiers", descOk, desc);

		RplComponent relayReplication = RplComponent.Cast(slotRelay.m_Entity.FindComponent(RplComponent));
		ACR_TestRegress.Check("fobslots: completed upgrade has a registered replication node", relayReplication && relayReplication.Id().IsValid() && Replication.FindItem(relayReplication.Id()) == relayReplication, "");

		// An antenna can be destroyed while its composition root remains alive.
		IEntity oldRelayRoot = slotRelay.m_Entity;
		IEntity antenna = ACR_Structures.RelayAntenna(oldRelayRoot);
		if (antenna) SCR_EntityHelper.DeleteEntityAndChildren(antenna);
		ACR_FobSlots.Advance(fob, 1.0);
		ACR_TestRegress.Check("fobslots: antenna child loss removes root and reopens relay slot", oldRelayRoot == null && slotRelay.m_Entity == null && slotRelay.m_eState == ACR_EFobSlotState.EMPTY, "");
		store.SetResourceValue(500);
		string rebuiltRelay = ACR_FobSlots.OrderUpgradeBase(manager, "US", fob, ACR_FobSlots.SLOT_RELAY, why);
		ACR_FobSlots.Advance(fob, 30.0);
		manager.m_Structures.ACR_TestForceTick();
		ACR_TestRegress.Check("fobslots: destroyed antenna can be rebuilt through its slot", !rebuiltRelay.IsEmpty() && slotRelay.m_eState == ACR_EFobSlotState.BUILT && ACR_Structures.RelayAntenna(slotRelay.m_Entity) != null && manager.m_Structures.HasRelay(fob), why);

		// 20. FOB cleanup removes spawned entities and resets slots
		ACR_FobSlots.Cleanup(fob);
		bool cleanOk = (slotRelay.m_eState == ACR_EFobSlotState.EMPTY && slotRelay.m_Entity == null && slotRepair.m_Entity == null && slotBarracks.m_Entity == null && slotRepair.m_iTier == 0);
		ACR_TestRegress.Check("fobslots: FOB cleanup removes spawned entities and resets slots", cleanOk, "");

		// Restore truck position
		truck.SetOrigin(truckPos);
	}
}
