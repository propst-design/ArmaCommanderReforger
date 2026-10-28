// Wear every approved helmet and boot candidate on a real UA character, checking the actual clothing slot.
class ACR_TestUAHelmets : BBB_AimTest
{
	static ref ACR_TestUAHelmets s_Helmets;
	protected ref array<ResourceName> m_Helmets = {};
	protected ref BBB_AimLane m_Lane;
	protected IEntity m_Soldier;
	protected SCR_InventoryStorageManagerComponent m_Inventory;
	protected SCR_CharacterInventoryStorageComponent m_Loadout;
	protected int m_Index;
	protected int m_Failed;
	protected int m_Passed;
	protected bool m_Checking;
	protected bool m_Boots;
	protected int m_Expected;
	protected float m_At;

	void ACR_TestUAHelmets() { m_aCheckDistances = {100}; }

	static void MaybeStartHelmets()
	{
		if (s_Helmets || !Replication.IsServer() || !System.IsCLIParam("acrUAHelmetTest")) return;
		s_Helmets = new ACR_TestUAHelmets();
		GetGame().GetCallqueue().CallLater(s_Helmets.BeginHelmets, 10000);
	}

	protected void BeginHelmets()
	{
		// The helmets are the UA mod's: without it the run can't be valid, so it says why (#54).
		if (!ACR_FactionSelection.UkraineModLoaded()) { ACR_TestRegress.Abort("uahelmets", "needs the UA (UAAF) mod loaded; this run has none"); return; }
		if (!ACR_UALoadout.Active() || !FindSite()) { m_Failed++; FinishHelmets(); return; }
		LoadLane();
		ACR_UALoadout.HelmetPool(m_Helmets);
		m_Expected = m_Helmets.Count();
		m_Lane = new BBB_AimLane();
		m_Lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(TEAM_US, m_aSiteFrom[0], m_aSiteDir[0]));
		if (!m_Lane.m_A.m_Group) { m_Failed++; FinishHelmets(); return; }
		m_Lane.m_aSpawned.Insert(m_Lane.m_A.m_Group);
		m_At = Now() + 4;
		GetGame().GetCallqueue().CallLater(TickHelmets, 500, true);
	}

	protected void TickHelmets()
	{
		if (Now() < m_At) return;
		if (!m_Soldier)
		{
			Hook(m_Lane.m_A, false);
			if (m_Lane.m_A.m_aMen.IsEmpty()) { m_Failed++; FinishHelmets(); return; }
			m_Soldier = m_Lane.m_A.m_aMen[0];
			ACR_UALoadout.Check(m_Soldier);
			m_Inventory = SCR_InventoryStorageManagerComponent.Cast(m_Soldier.FindComponent(SCR_InventoryStorageManagerComponent));
			m_Loadout = m_Inventory.GetCharacterStorage();
			m_At = Now() + 4;
			return;
		}
		typename area = LoadoutHeadCoverArea;
		if (m_Boots) area = LoadoutBootsArea;
		IEntity head = m_Loadout.GetClothFromArea(area);
		if (m_Checking)
		{
			bool ok = head && head.GetPrefabData() && head.GetPrefabData().GetPrefabName() == m_Helmets[m_Index];
			string result = "FAIL";
			if (ok) { m_Passed++; result = "PASS"; }
			else m_Failed++;
			string kind = "HELMET";
			if (m_Boots) kind = "BOOTS";
			PrintFormat("[ACR] UA %1 %2 %3 actual=%4", kind, result, m_Helmets[m_Index], ACR_UALoadout.Leaf(head));
			m_Index++;
			m_Checking = false;
		}
		if (head)
		{
			m_Inventory.TryDeleteItem(head);
			m_At = Now() + 0.5;
			return;
		}
		if (m_Index >= m_Helmets.Count())
		{
			if (m_Boots) { FinishHelmets(); return; }
			m_Boots = true;
			m_Index = 0;
			ACR_UALoadout.BootPool(m_Helmets);
			m_Expected += m_Helmets.Count();
			if (m_Helmets.IsEmpty()) { m_Failed++; FinishHelmets(); return; }
			return;
		}
		bool admitted = m_Inventory.TrySpawnPrefabToStorage(m_Helmets[m_Index], m_Loadout);
		if (!admitted)
		{
			m_Failed++;
			PrintFormat("[ACR] UA HELMET FAIL insert %1", m_Helmets[m_Index]);
			m_Index++;
			return;
		}
		m_Checking = true;
		m_At = Now() + 0.5;
	}

	protected void FinishHelmets()
	{
		GetGame().GetCallqueue().Remove(TickHelmets);
		if (m_Lane) Clear(m_Lane);
		m_Lane = null;
		string result = "PASS";
		if (m_Failed > 0 || m_Helmets.IsEmpty() || m_Passed != m_Expected) result = "FAIL";
		PrintFormat("[ACR] VERDICT %1 test=uahelmets passed=%2 failed=%3 expected=%4", result, m_Passed, m_Failed, m_Expected);
		Print("[ACR] TEST COMPLETE");
	}
}

modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		ACR_TestUAHelmets.MaybeStartHelmets();
	}
}
