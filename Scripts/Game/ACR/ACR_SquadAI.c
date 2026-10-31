// Hand a squad to the AI (squad panel, "AI commands this squad"): while a player commands the
// side, the AI commander runs its usual squad logic for the squads handed to it, and only for
// them (ACR_AICommander.TickHanded: attacks, base defence, the reserve and posting, roadblocks
// and minefields). It buys nothing, runs no logistics and builds no FOB for the side; the player
// keeps every other squad. An order of the player's to a handed squad takes it back first and
// then goes out as usual; the toggle takes it back halted, with no AI task. With the AI in
// command of the side it runs every squad as before. Built on the FOB handover (ACR_FobAI): a
// side without an AI commander gets the same AI-only instance (ACR_Manager.FobAICommanderFor).
// The always-on second-in-command (#14, -acr2IC 1, off for now): while no AI commander is in
// command of a side, every squad of it nobody has claimed is the AI's (Adopt). A player claims a
// squad by leading it or giving it an order, until he hands it back with the panel's toggle.
class ACR_SquadAI
{
 static bool TwoIC() { return ACR_Options.Flag("acr2IC"); }

 // The 2IC runs a side a player commands, or one with no AI commander at all (its squads would
 // stand idle). A side the AI commander leads never has one, its start-up hold included: pn10
 // (26 Oct) had the 2IC take on Red's squads in the 45 s before Red's AI commander took command.
 static bool Runs(ACR_Manager manager, string faction)
 {
  if (!manager) return false;
  Faction side = GetGame().GetFactionManager().GetFactionByKey(faction);
  array<int> commanders = null;
  if (side) commanders = manager.GetCommanders(side);
  if (commanders && !commanders.IsEmpty()) return true;
  ACR_FactionSettings settings = ACR_FactionSettings.Find(faction);
  return !settings || !settings.m_bAICommander;
 }

 // The 2IC takes on every squad of the side nobody has claimed (as they are: their AI tasks
 // carry on); a squad a player leads is his. How many it took on.
 static int Adopt(string faction)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !TwoIC()) return 0;
  int adopted;
  array<ACR_Squad> taken = {};
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
   if (squad.HasPlayer() || squad.m_Commander != 0)
   {
    if (squad.m_bAICommanded) Release(squad, false);
    squad.m_bClaimed = true;
    continue;
   }
   string why;
   if (squad.m_bAICommanded || squad.m_bClaimed || !CanHand(squad, why)) continue;
   Hand(squad, true);
   adopted++;
   taken.Insert(squad);
  }
  if (adopted > 0) PrintFormat("[ACR] 2IC %1: %2 unclaimed squad(s) taken on", faction, adopted);
  if (adopted == 1) ACR_TwoIC.Report(faction, string.Format("Took on %1", taken[0].GetName()));
  else if (adopted > 1) ACR_TwoIC.Report(faction, string.Format("Took on %1 squads nobody leads", adopted));
  return adopted;
 }

 static bool CanHand(ACR_Squad squad, out string refusal)
 {
  refusal = "";
  if (squad.HasPlayer()) refusal = "a player is leading it";
  else if (squad.m_bMortar) refusal = "mortar teams stay with you (fire missions)";
  else if (squad.Aircraft()) refusal = "the AI can't fly helicopters";
  else if (ACR_AirLift.Busy(squad)) refusal = "it is on an air lift";
  return refusal.IsEmpty();
 }

 // The panel's toggle (server): the message for the player.
 static string Toggle(ACR_Squad squad)
 {
  if (squad.m_bAICommanded)
  {
   Release(squad, true);
   return squad.GetName() + ": back under your command";
  }
  string why;
  if (!CanHand(squad, why)) return squad.GetName() + ": " + why;
  Hand(squad);
  return squad.GetName() + ": the AI commands it now (your next order to it takes it back)";
 }

 // keepTask: the 2IC taking on a squad the AI already runs (Adopt) keeps its AI task.
 // A squad's own move mode when it was handed over, back on Release: the AI's attacks and rides run
 // Fast (a forced order), and a squad handed back still on Fast forced every order of the player's
 // after it (pn11, 27 Oct: a LAV's turret crewman got out at a gate on the player's move; BBB
 // keeps crews aboard only for orders below the player's priority level).
 protected static ref map<ACR_Squad, int> s_ModeBefore = new map<ACR_Squad, int>();
 static void Reset() { s_ModeBefore.Clear(); }

 static void Hand(ACR_Squad squad, bool keepTask = false)
 {
  if (!squad || squad.m_bAICommanded) return;
  s_ModeBefore.Set(squad, squad.m_Mode);
  squad.m_bAICommanded = true;
  squad.m_bClaimed = false;
  if (!keepTask) squad.m_iAITask = ACR_EAITask.FREE;
  squad.m_iOrderedBy = 0;
  PrintFormat("[ACR] SQUAD AI %1: handed to the AI", squad.GetName());
 }

 // Back to the player: out of every AI plan; halt: standing still (the toggle), else the
 // player's order that took it back replaces the AI's.
 static bool Release(ACR_Squad squad, bool halt)
 {
  if (!squad || !squad.m_bAICommanded) return false;
  squad.m_bAICommanded = false;
  squad.m_bClaimed = true;
  squad.m_iAITask = ACR_EAITask.FREE;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && squad.GetFaction())
  {
   ACR_AICommander ai = manager.FobAICommanderFor(squad.GetFaction().GetFactionKey());
   if (ai) ai.Forget(squad);
  }
  int before;
  if (s_ModeBefore.Find(squad, before))
  {
   s_ModeBefore.Remove(squad);
   if (squad.m_Mode != before) squad.SetMode(before);
  }
  if (halt) squad.Halt("Holding");
  PrintFormat("[ACR] SQUAD AI %1: back to the player (halt %2), mode %3", squad.GetName(), halt, typename.EnumToString(ACR_EMoveMode, squad.m_Mode));
  if (squad.GetFaction()) ACR_TwoIC.Report(squad.GetFaction().GetFactionKey(), string.Format("%1 is yours now", squad.GetName()));
  return true;
 }

 // An order (not a mode, the fire-at-will toggle or this toggle) takes a handed squad back.
 static bool TakesBack(int command)
 {
  return command != ACR_ERequest.ORDER_SQUAD_AI && command != ACR_ERequest.MODE_COMBAT && command != ACR_ERequest.MODE_FAST && command != ACR_ERequest.MODE_SILENT && command != ACR_ERequest.ORDER_FIRE_AT_WILL;
 }

 // A player's order to the squad: true when it took the squad back.
 static bool TakeBack(ACR_Squad squad, int command)
 {
  if (!squad || !TakesBack(command)) return false;
  if (TwoIC()) squad.m_bClaimed = true; // ordered by a player: the 2IC leaves it alone
  if (!squad.m_bAICommanded) return false;
  return Release(squad, false);
 }

 // After a player's order (ACR_Requests.OrderSquad): refused, the squad is as it was before it:
 // taken back from the AI, it is handed back (the AI runs it again); not handed, its claim is what
 // it was (TakeBack claims it for the 2IC's sake).
 static void AfterOrder(ACR_Squad squad, int command, bool accepted, bool tookBack, bool wasClaimed)
 {
  if (!squad || accepted || !TakesBack(command)) return;
  if (tookBack)
  {
   Hand(squad);
   PrintFormat("[ACR] SQUAD AI %1: the order was refused, so it stays the AI's", squad.GetName());
   return;
  }
  squad.m_bClaimed = wasClaimed;
 }

 // A squad that is gone (wiped out, dropped): nothing kept for it (ACR_Squad.Dispose).
 static void Forget(ACR_Squad squad)
 {
  if (squad) s_ModeBefore.Remove(squad);
 }

 static bool RemembersMode(ACR_Squad squad)
 {
  return squad && s_ModeBefore.Contains(squad);
 }

 // A squad of this side is handed to the AI.
 static bool AnyFor(string faction)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.m_bAICommanded && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction) return true;
  return false;
 }

 static int CountFor(string faction)
 {
  int count;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return 0;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.m_bAICommanded && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction) count++;
  return count;
 }
}
