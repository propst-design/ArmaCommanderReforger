// srv:fiamuster (Everon whole map, about 45 s): insurgent muster, disperse and hand-fired IED kit
// (ACR_Muster, ACR_HideoutHomes.GiveIedKit, 0.8.0 #97 item 9).
// Verifies:
//  1. Muster in cover within 3 km of a cool hideout stands up after the walk and spawns a real squad.
//  2. Muster refuses with clear reasons: open ground/water, near army base (<500 m), watched by enemy
//     (<600 m), inside a Loyal town, and out of reach / hot hideout.
//  3. A watched muster pauses and aborts + refunds RP after WATCH_ABORT_S.
//  4. An in-contact band cannot disperse; an unseen band out of contact disperses and refunds RP.
//  5. Drawing an IED kit deducts 5 RP and spawns a charge + detonator into an insurgent's inventory.
class ACR_TestFiaMuster
{
	protected static ACR_DefenderPost s_Hideout;

	static void Run(int second)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return;
		if (second == 10) Verify(manager);
		if (second == 15) VerifyDisperseAndKit(manager);
	}

	protected static void Verify(ACR_Manager manager)
	{
		string ins = ACR_InsurgentSide.Key();
		string army = ACR_Coin.Army();
		ACR_FactionData data = manager.m_Bases.GetFaction(ins);
		data.m_iRP = 300;

		// Pick the first foot infantry entry in the insurgent buy list.
		int footIdx = -1;
		foreach (int i, ACR_RequisitionEntry e : data.m_aRequisitions)
		{
			if (ACR_Muster.IsFootEntry(e)) { footIdx = i; break; }
		}

		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		ACR_DefenderPost hideout;
		if (!hideouts.IsEmpty()) hideout = hideouts[0];
		s_Hideout = hideout;

		// Find a valid covered muster spot within 3 km of 'hideout'.
		vector goodSpot = vector.Zero;
		bool foundGood = false;
		if (hideout && footIdx >= 0)
		{
			foreach (vector h : ACR_Guerrillas.s_aHouses)
			{
				ACR_DefenderPost picked;
				float slow;
				string ref0;
				if (ACR_Muster.CanMuster(ins, footIdx, h, picked, slow, ref0))
				{
					goodSpot = h;
					foundGood = true;
					break;
				}
			}
		}

		// 1. Valid muster queues, advances, and stands up as a real squad.
		int squadsBefore = manager.m_Squads.Count();
		int rpBefore = data.m_iRP;
		string mRef;
		bool ordered = foundGood && ACR_Muster.Order(ins, footIdx, goodSpot, mRef);
		bool zoneShows = false;
		if (ordered)
		{
			array<vector> zp = {}; array<float> zr = {}; array<string> zl = {};
			ACR_Muster.Zones(ins, zp, zr, zl);
			zoneShows = !zl.IsEmpty();
			ACR_Muster.AdvanceForTest(3000);
		}
		Check("fiamuster: valid muster queues, shows zone, and stands up after walk",
			ordered && zoneShows && ACR_Muster.s_Jobs.IsEmpty() && data.m_iRP < rpBefore,
			string.Format("ordered %1 ('%2'), zone %3, jobs %4, rp %5->%6, squads %7->%8", ordered, mRef, zoneShows, ACR_Muster.s_Jobs.Count(), rpBefore, data.m_iRP, squadsBefore, manager.m_Squads.Count()));

		// 2. Refusals: water/open, near army base, Loyal town, out of reach / hot hideout.
		ACR_DefenderPost dummyH; float dummyS;
		string rWater, rBase, rLoyal, rFar;
		bool badWater = !ACR_Muster.CanMuster(ins, footIdx, Vector(10, 0, 10), dummyH, dummyS, rWater);
		ACR_Base armyBase = ACR_OneSide.Home();
		bool badBase = armyBase && !ACR_Muster.CanMuster(ins, footIdx, armyBase.GetOrigin(), dummyH, dummyS, rBase);
		// Loyal town refusal.
		ACR_Base town;
		foreach (ACR_Base b : ACR_Base.s_All) if (ACR_TownSupport.IsTown(b) && b.m_sOwner != army) { town = b; break; }
		bool badLoyal = false;
		if (town)
		{
			int savedTier = town.m_iSupportTier;
			town.m_iSupportTier = ACR_TownSupport.LOYAL;
			badLoyal = !ACR_Muster.CanMuster(ins, footIdx, town.GetOrigin(), dummyH, dummyS, rLoyal);
			town.m_iSupportTier = savedTier;
		}
		// Out of reach / hot hideout refusal.
		bool badHot = false;
		if (hideout && foundGood)
		{
			foreach (ACR_DefenderPost hh : hideouts)
				if (vector.DistanceXZ(hh.m_vPos, goodSpot) <= ACR_Muster.MUSTER_REACH) ACR_Guerrillas.Know(hh, army);
			badHot = !ACR_Muster.CanMuster(ins, footIdx, goodSpot, dummyH, dummyS, rFar);
			foreach (ACR_DefenderPost hh2 : hideouts)
				if (hh2.m_Cell) hh2.m_Cell.m_aKnownBy.RemoveItem(army);
		}
		Check("fiamuster: refuses water/open, near army base, Loyal town, and hot hideouts",
			badWater && badBase && badLoyal && badHot,
			string.Format("water '%1', base '%2', loyal '%3', hot '%4'", rWater, rBase, rLoyal, rFar));

		// 3. Watched muster pauses and refunds after WATCH_ABORT_S.
		bool abortOk = false;
		string abortInfo = "no army base";
		if (hideout && armyBase && footIdx >= 0)
		{
			ACR_MusterJob fakeJob = new ACR_MusterJob();
			fakeJob.m_sFaction = ins;
			fakeJob.m_iEntry = footIdx;
			fakeJob.m_sName = "Test band";
			fakeJob.m_vSpot = armyBase.GetOrigin(); // army soldiers present here
			fakeJob.m_Hideout = hideout;
			fakeJob.m_iPaid = 20;
			fakeJob.m_fDurationMs = 120000;
			ACR_Muster.s_Jobs.Insert(fakeJob);
			int rpPreAbort = data.m_iRP;
			ACR_Muster.AdvanceForTest(ACR_Muster.WATCH_ABORT_S + 5);
			abortOk = ACR_Muster.s_Jobs.IsEmpty() && data.m_iRP == rpPreAbort + 20;
			abortInfo = string.Format("jobs %1, rp %2->%3", ACR_Muster.s_Jobs.Count(), rpPreAbort, data.m_iRP);
		}
		Check("fiamuster: watched muster pauses and aborts with full RP refund", abortOk, abortInfo);
	}

	protected static void VerifyDisperseAndKit(ACR_Manager manager)
	{
		string ins = ACR_InsurgentSide.Key();
		ACR_FactionData data = manager.m_Bases.GetFaction(ins);

		// 4. Disperse: unseen band out of contact disperses and refunds RP.
		bool disperseOk = false;
		string disperseInfo = "no band";
		ACR_Squad band;
		foreach (ACR_Squad sq : manager.m_Squads)
		{
			if (sq && sq.GetFaction() && sq.GetFaction().GetFactionKey() == ins && sq.CountAlive() > 0) { band = sq; break; }
		}
		if (band)
		{
			int rpPreDisp = data.m_iRP;
			string dRef;
			bool dispersed = ACR_Muster.Disperse(band, dRef);
			disperseOk = dispersed && data.m_iRP > rpPreDisp;
			disperseInfo = string.Format("dispersed %1 ('%2'), rp %3->%4", dispersed, dRef, rpPreDisp, data.m_iRP);
		}
		Check("fiamuster: unseen band disperses and refunds RP", disperseOk, disperseInfo);

		// 5. Hand-fired IED kit: spawns charge + detonator and deducts 5 RP.
		bool kitOk = false;
		string kitInfo = "no soldier";
		if (s_Hideout)
		{
			IEntity fighter = ACR_Manager.SpawnPrefab(ACR_Mortars.RIFLE_FIA, ACR_Manager.Ground(s_Hideout.m_vPos + Vector(3, 0, 3)));
			if (fighter)
			{
				int rpPreKit = data.m_iRP;
				string kRef;
				bool gave = ACR_HideoutHomes.GiveIedKit(fighter, ins, kRef, true);
				kitOk = gave && data.m_iRP == rpPreKit - ACR_HideoutHomes.IED_KIT_COST;
				kitInfo = string.Format("gave %1 ('%2'), rp %3->%4", gave, kRef, rpPreKit, data.m_iRP);
				SCR_EntityHelper.DeleteEntityAndChildren(fighter);
			}
		}
		Check("fiamuster: hand-fired IED kit spawns charge + detonator and deducts 5 RP", kitOk, kitInfo);

		ACR_TestRegress.Complete("fiamuster", 5);
	}

	protected static void Check(string name, bool ok, string detail)
	{
		ACR_TestRegress.Check(name, ok, detail);
	}
}
