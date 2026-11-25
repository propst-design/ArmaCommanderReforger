// Insurgent AI commander test (0.8.0 #97 item 11, -acrGuerrillaAITest 1):
// verifies Guerrilla AI runs when -acrCoinInsurgency ai is set, conventional ACR_AICommander
// never runs on the insurgent side, and the AI places network pieces, orders sabotage raids,
// musters and orders guerrilla bands (without land-grabbing), and packs up hot hideouts.
class ACR_TestGuerrillaAI
{
	protected static bool s_bDone;
	protected static bool s_bPhase1;

	static void Check(string name, bool ok, string detail = "")
	{
		ACR_TestRegress.Check(name, ok, detail);
	}

	static void Run(int second)
	{
		if (s_bDone) return;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases) return;

		string key = ACR_InsurgentSide.Key();
		string army = ACR_Coin.Army();

		if (second == 6 && !s_bPhase1)
		{
			s_bPhase1 = true;

			// 1. Guerrilla AI active; conventional ACR_AICommander never runs for the insurgent side.
			bool convNone = (manager.AICommanderFor(key) == null);
			Check("guerrilla_ai_active", ACR_GuerrillaAI.Active() && convNone, string.Format("active=%1 convAI=%2 key=%3", ACR_GuerrillaAI.Active(), convNone, key));

			// Give insurgent side RP for testing network, sabotage, and muster decisions.
			manager.m_Bases.Refund(key, 120);

			// 2. Network spending: AI places a cell or IED on a road and spends RP.
			int cellsBefore = ACR_FIAGrowth.Cells();
			int iedsBefore = ACR_IED.Live();
			int rpBefore = manager.m_Bases.GetRP(key);
			ACR_GuerrillaAI.RunTickForTest(manager);
			int cellsAfter = ACR_FIAGrowth.Cells();
			int iedsAfter = ACR_IED.Live();
			int rpAfter = manager.m_Bases.GetRP(key);
			bool placedNet = (cellsAfter > cellsBefore || iedsAfter > iedsBefore) && rpAfter < rpBefore;
			Check("guerrilla_ai_network", placedNet, string.Format("cells %1->%2 ieds %3->%4 rp %5->%6", cellsBefore, cellsAfter, iedsBefore, iedsAfter, rpBefore, rpAfter));

			// 3. Sabotage raid: add an unmanned army roadblock within 1500 m of a hideout and
			// run ticks until the AI launches a sabotage raid.
			array<ACR_DefenderPost> hideouts = {};
			ACR_Guerrillas.Hideouts(hideouts);
			ref ACR_Roadblock testRb;
			if (!hideouts.IsEmpty())
			{
				vector rbPos = ACR_Manager.Ground(hideouts[0].m_vPos + Vector(450, 0, 0));
				testRb = new ACR_Roadblock();
				testRb.m_Entity = GetGame().GetGameMode(); // non-null anchor entity for PickTarget/OrderSabotage
				testRb.m_sFaction = army;
				testRb.m_vPos = rbPos;
				ACR_Roadblocks.s_aRoadblocks.Insert(testRb);
			}
			for (int t = 0; t < 12 && !ACR_Raid.Running(); t++)
			{
				if (manager.m_Bases.GetRP(key) < 40) manager.m_Bases.Refund(key, 60);
				ACR_GuerrillaAI.RunTickForTest(manager);
			}
			bool raidSent = (ACR_Raid.Running() != null);
			Check("guerrilla_ai_sabotage", raidSent, string.Format("raidRunning=%1", raidSent));

			// 4. Band muster: AI starts a muster job and fast-forwarding stands up the band.
			bool hadJob = ACR_Muster.s_Jobs && !ACR_Muster.s_Jobs.IsEmpty();
			if (!hadJob)
			{
				ACR_GuerrillaAI.RunTickForTest(manager);
				hadJob = ACR_Muster.s_Jobs && !ACR_Muster.s_Jobs.IsEmpty();
			}
			if (hadJob) ACR_Muster.AdvanceForTest(3000);
			Check("guerrilla_ai_muster", hadJob, string.Format("musterStarted=%1", hadJob));
		}

		if (second == 12)
		{
			s_bDone = true;

			// 5. Band receives a guerrilla order (town work or road ambush), never a conventional attack.
			ACR_GuerrillaAI.RunTickForTest(manager);
			string assignedRole = "";
			bool noConventionalTask = true;
			foreach (ACR_Squad sq : manager.m_Squads)
			{
				if (!sq || !sq.GetFaction() || sq.GetFaction().GetFactionKey() != key || sq.CountAlive() <= 0) continue;
				string r = ACR_GuerrillaAI.BandRole(sq);
				if (!r.IsEmpty()) assignedRole = r;
				if (sq.m_iAITask == ACR_EAITask.ATTACK || sq.m_iAITask == ACR_EAITask.BUILD_FOB) noConventionalTask = false;
			}
			Check("guerrilla_ai_band_order", !assignedRole.IsEmpty() && noConventionalTask, string.Format("role=%1 noConvTask=%2", assignedRole, noConventionalTask));

			// 6. Hot hideout goes to ground when another hideout stands.
			array<ACR_DefenderPost> hideouts = {};
			ACR_Guerrillas.Hideouts(hideouts);
			bool wentToGround = false;
			string hotDetail = "need 2+ hideouts";
			if (hideouts.Count() >= 2)
			{
				ACR_DefenderPost target = hideouts[0];
				ACR_FIAIntel.Lead(army, target, "test1");
				ACR_FIAIntel.Lead(army, target, "test2");
				ACR_FIAIntel.Lead(army, target, "test3");
				string heatBefore = ACR_FIAIntel.Heat(target, army);
				int rpBeforeGtg = manager.m_Bases.GetRP(key);
				ACR_GuerrillaAI.RunTickForTest(manager);
				int rpAfterGtg = manager.m_Bases.GetRP(key);
				bool leadCleared = (ACR_FIAIntel.LeadOf(army, target) == null);
				wentToGround = target.m_bGone && leadCleared;
				hotDetail = string.Format("heat=%1 gone=%2 leadCleared=%3 rp %4->%5", heatBefore, target.m_bGone, leadCleared, rpBeforeGtg, rpAfterGtg);
			}
			Check("guerrilla_ai_hot_relocate", wentToGround, hotDetail);

			ACR_TestRegress.Complete("guerrillaai", 6);
		}
	}
}
