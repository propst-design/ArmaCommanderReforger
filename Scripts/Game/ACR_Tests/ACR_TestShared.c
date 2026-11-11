// Shared command tests.
//  -acrTagsTest 1   (server-capable) short names, ping rules, map feed volume
//  -acrSharedTest 1 (needs the game) orders credited to their commander, Halt,
//                   override warning, pings (map, picker, HUD), the support link,
//                   selections, the spending log, with the command map open.
// Each check is an ACR_TestRegress check; VERDICT: tags 19, shared 17.
class ACR_TestShared
{
 static const int CHECKS_TAGS = 19;
 static const int CHECKS_SHARED = 17;

 static void Check(string name, bool ok, string detail = "")
 {
  ACR_TestRegress.Check(name, ok, detail);
 }

 static string Tags(array<string> names)
 {
  array<string> tags = {};
  ACR_Command.MakeTags(names, null, tags);
  string joined = "";
  foreach (string t : tags) joined += t + "|";
  return joined;
 }

 // ---------------------------------------------------------------- -acrTagsTest
 static void TestTags(ACR_TestHarness harness, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   ACR_Perf.s_bOn = true;
   string cyr = "Ёжиков";
   PrintFormat("[ACR] SHARED info: '%1' Length()=%2 chars=%3 ToAscii(0)=%4", cyr, cyr.Length(), ACR_Command.CharCount(cyr), cyr.ToAscii(0));
  }
  if (tick == 3)
  {
   Check("tag basic", Tags({"PoppiPoppins", "Hexxyz", "propst-design"}) == "Popp|Hexx|prop|", Tags({"PoppiPoppins", "Hexxyz", "propst-design"}));
   Check("tag clan and decoration", Tags({"[ACR] xX_Hexxyz_Xx"}) == "Hexx|", Tags({"[ACR] xX_Hexxyz_Xx"}));
   Check("tag two words", Tags({"Jacob Thornton"}) == "Jaco|", Tags({"Jacob Thornton"}));
   Check("tag clash grows", Tags({"PoppiPoppins", "Poppy"}) == "Poppi|Poppy|", Tags({"PoppiPoppins", "Poppy"}));
   Check("tag same name numbered", Tags({"Hexxyz", "[X] Hexxyz"}) == "Hexxyz1|Hexxyz2|", Tags({"Hexxyz", "[X] Hexxyz"}));
   Check("tag digits only", Tags({"12345", "Bob"}) == "Cmd1|Bob|", Tags({"12345", "Bob"}));
   Check("tag symbols only", Tags({"!!!", "###"}) == "Cmd1|Cmd2|", Tags({"!!!", "###"}));
   string cyrTag = Tags({"Ёжиков"});
   Check("tag cyrillic whole letters", cyrTag == "Ёжик|", cyrTag);
   Check("tag accents", Tags({"Zoë_Müller"}) == "ZoëM|", Tags({"Zoë_Müller"}));
  }
  if (tick == 4)
  {
   ACR_Pings pings = manager.Pings();
   vector at = ACR_Manager.Ground("1400 0 2832");
   for (int k = 0; k < 4; k++) pings.Add("US", 900, ACR_EPing.ATTACK, at + Vector(k * 10, 0, 0), "Popp");
   array<ACR_Ping> us = {};
   pings.Of("US", us);
   Check("ping cap per player", us.Count() == 3, us.Count().ToString());
   Check("ping oldest goes first", us.Count() == 3 && us[0].m_Pos[0] > at[0] + 5, "");
   array<ACR_Ping> ussr = {};
   pings.Of("USSR", ussr);
   Check("ping only for its side", ussr.IsEmpty(), "");
   string other = pings.Remove(901, us[0].m_iId);
   Check("ping not removable by others", !other.IsEmpty(), other);
   pings.Remove(900, us[0].m_iId);
   pings.Of("US", us);
   Check("ping removed by owner", us.Count() == 2, us.Count().ToString());
   ACR_Ping look = pings.Add("US", 902, ACR_EPing.LOOK, at, "Hexx");
   ACR_Ping support = pings.Add("US", 902, ACR_EPing.SUPPORT, at + Vector(0, 0, 300), "Hexx");
   pings.OnSupportCalled("US", at + Vector(40, 0, 300), "Jaco", "Mortar HE");
   Check("support ping answered", support.m_sAnswer == "Answered by Jaco: Mortar HE", support.m_sAnswer);
   pings.OnSupportCalled("US", at + Vector(1000, 0, 300), "Jaco", "Smoke");
   Check("far support call answers nothing new", support.m_sAnswer.Contains("Mortar"), "");
   ACR_Intel intel = manager.IntelFor("US");
   bool fed = false;
   foreach (string line : intel.m_Feed) if (line.Contains("Hexx: Look here")) fed = true;
   Check("ping in the side's feed", fed, "");
  }
  if (tick == 22)
  {
   // The Look ping (15 s) and the answered support ping (8 s) are gone; the attacks stay.
   array<ACR_Ping> left = {};
   manager.Pings().Of("US", left);
   int looks, supports, attacks;
   foreach (ACR_Ping p : left)
   {
    if (p.m_iKind == ACR_EPing.LOOK) looks++;
    if (p.m_iKind == ACR_EPing.SUPPORT) supports++;
    if (p.m_iKind == ACR_EPing.ATTACK) attacks++;
   }
   Check("pings expire", looks == 0 && supports == 0 && attacks == 2, string.Format("look %1 support %2 attack %3", looks, supports, attacks));
  }
  if (tick == 24)
  {
   // Feed frames are built for each side even without players (perf mode).
   ACR_FeedFrame frame = ACR_MapFeed.Build(manager, GetGame().GetFactionManager().GetFactionByKey("US"));
   int total;
   string parts = "";
   foreach (string section, int bytes : frame.m_Bytes) { total += bytes; parts += string.Format(" %1=%2", section, bytes); }
   Check("feed frame built", frame.m_PingIds.Count() == 2 && total > 0, string.Format("%1 bytes:%2", total, parts));
   ACR_TestRegress.Complete("tags", CHECKS_TAGS);
  }
 }

 // ---------------------------------------------------------------- -acrSharedTest
 static void TestShared(ACR_TestHarness harness, int tick)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager || !harness.m_Range) return;
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_Perf.s_bOn = true;
  }
  ACR_Squad alpha = harness.Squad();
  ACR_Squad bravo = manager.FindSquadByGroup(harness.m_Range.m_Bravo);
  if (!alpha || !bravo) return;
  RplId alphaId = SCR_EntityHelper.EntityToRplId(harness.m_Range.m_Group);
  RplId bravoId = SCR_EntityHelper.EntityToRplId(harness.m_Range.m_Bravo);
  int me = pc.GetPlayerId();
  ACR_MapData data = pc.ACR_TestData();
  IEntity body = pc.GetControlledEntity();
  // The map opens at t=8 (needs the launcher from 1 Oct, which ignores the
  // vanilla map tool exception on open); drawing runs every frame while open.
  if (tick < 20) return;
  tick -= 12;
  if (tick == 8)
  {
   Check("shared command on by default", manager.m_SharedCommand, "");
   Check("player is a commander", manager.IsCommander(me), manager.CommandStatus(me));
   GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  }
  if (tick == 10) Check("command map open", pc.ACR_TestMapOpen(), "");
  if (tick == 10)
  {
   // The game's own map controls (pan, zoom, select) live in the cursor module;
   // without it only our widgets respond to the mouse.
   SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
   SCR_MapCursorModule cursor;
   if (mapEntity) cursor = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
   Check("map cursor module active", cursor != null, "");
  }
  if (tick == 10) pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, harness.Ground("1420 0 2832"));
  if (tick == 13)
  {
   Check("order credited to its commander", alpha.m_iOrderedBy == me, alpha.m_iOrderedBy.ToString());
   string by = pc.ACR_TestOrderedBy(alphaId);
   Check("panel shows who ordered", by.Contains("Ordered by you"), by);
   pc.ACR_Request(ACR_ERequest.MOVE_TO, bravoId, harness.Ground("1420 0 2880"));
  }
  if (tick == 16) pc.ACR_Request(ACR_ERequest.ORDER_HALT, bravoId);
  if (tick == 18)
  {
   array<AIWaypoint> waypoints = {};
   bravo.m_Group.GetWaypoints(waypoints);
   Check("halt stops the squad", bravo.m_Order == "Halted" && waypoints.IsEmpty(), bravo.m_Order + " waypoints " + waypoints.Count().ToString());
   // Someone else (a fake player 999) ordered Alpha a moment ago.
   alpha.m_iOrderedBy = 999;
   alpha.m_fOrderedAt = GetGame().GetWorld().GetWorldTime();
   pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, harness.Ground("1430 0 2832"));
  }
  if (tick == 20)
  {
   bool retasked = false;
   foreach (string line : manager.IntelFor("US").m_Feed) if (line.Contains("re-tasked Alpha")) retasked = true;
   Check("re-tasking someone's order is in the feed", retasked, "");
   // Client: the override warning (fake commander 999 "Popp" ordered Alpha 3 s ago).
   data.m_CmdIds.Clear(); data.m_CmdIds.Insert(me); data.m_CmdIds.Insert(999);
   data.m_CmdTags.Clear(); data.m_CmdTags.Insert("Me"); data.m_CmdTags.Insert("Popp");
   int s = data.m_StatusGroups.Find(alphaId);
   if (s >= 0)
   {
    data.m_Status[s * 6 + 4] = 999;
    data.m_Status[s * 6 + 5] = 3;
    data.m_iStatusAt = System.GetTickCount();
   }
   array<RplId> picked = {alphaId};
   bool first = pc.ACR_TestConfirmOverride(picked);
   bool second = pc.ACR_TestConfirmOverride(picked);
   Check("override asks first, then goes ahead", s >= 0 && !first && second, string.Format("status %1 first %2 second %3", s, first, second));
  }
  if (tick == 21)
  {
   // Pings: Look 25 m ahead, Support 45 m ahead (seen from the map and the HUD).
   vector forward = body.GetTransformAxis(2);
   forward[1] = 0;
   forward.Normalize();
   pc.ACR_RequestPing(ACR_EPing.LOOK, body.GetOrigin() + forward * 25, true);
  }
  if (tick == 22)
  {
   vector ahead = body.GetTransformAxis(2);
   ahead[1] = 0;
   ahead.Normalize();
   pc.ACR_RequestPing(ACR_EPing.SUPPORT, body.GetOrigin() + ahead * 45 + Vector(8, 0, 0), true);
  }
  if (tick == 23)
  {
   bool picked = pc.ACR_TestPicker(harness.Ground("1450 0 2850"), ACR_EPing.ATTACK);
   Check("map ping picker places a ping", picked, "");
  }
  if (tick == 25)
  {
   Check("client has the pings", data.m_PingIds.Count() >= 3, data.m_PingIds.Count().ToString());
  }

  if (tick == 29)
  {
   // The map stays open (closing it from a test throws in vanilla map UI code):
   // the HUD layer must be hidden then. With the map closed it showed 3 (1 Oct run).
   Check("HUD pings hidden while the map is open", pc.ACR_TestHudPings() == 0, pc.ACR_TestHudPings().ToString());
  }
  if (tick == 33)
  {
   int supportId = -1;
   foreach (int i, int id : data.m_PingIds) if (data.m_PingKinds[i] == ACR_EPing.SUPPORT) supportId = id;
   pc.ACR_TestClickPing(supportId);
   Check("support ping arms a support call", pc.ACR_TestHasSupportTarget(), supportId.ToString());
  }
  if (tick == 34)
  {
   array<RplId> selection = {alphaId, bravoId};
   pc.ACR_TestSelect(selection);
  }
  if (tick == 37)
  {
   array<RplId> groups = {};
   array<int> owners = {};
   manager.Command().SelectionsOf("US", groups, owners);
   Check("selection shared with other commanders", groups.Contains(alphaId) && groups.Contains(bravoId) && owners.Contains(me), groups.Count().ToString());
   foreach (int r, ACR_RequisitionEntry e : manager.m_Bases.GetFaction("US").m_aRequisitions)
    if (e.m_sVehicle.IsEmpty()) { pc.ACR_TestPurchase(r, "Farm [1]"); break; }
  }
  if (tick == 39)
  {
   bool logged = false;
   foreach (string spend : manager.IntelFor("US").m_Feed) if (spend.Contains("requisitioned") && spend.Contains("RP)")) logged = true;
   Check("spending in the feed", logged, "");
  }
  if (tick == 38)
  {
   // Another commander's order (fake 999), drawn dashed with his tag.
   bravo.MoveTo(harness.Ground("1480 0 2900"));
   bravo.m_iOrderedBy = 999;
  }
  if (tick == 40)
  {
   data.m_CmdIds.Clear(); data.m_CmdIds.Insert(me); data.m_CmdIds.Insert(999);
   data.m_CmdTags.Clear(); data.m_CmdTags.Insert("Jaco"); data.m_CmdTags.Insert("Popp");
   int dashed;
   foreach (int o, int issuer : data.m_OrderIssuers) if (issuer == 999) dashed++;
   Check("other commander's order reaches the map", dashed >= 1, dashed.ToString());
  }
  if (tick == 62)
  {
   // HUD widgets live on the workspace: the mode's end must take them away.
   ACR_PingDisplay.ShowHoldHint(ACR_EPing.ATTACK);
   int before = ACR_Hud.s_iCleared;
   ACR_Hud.Clear();
   Check("HUD widgets removed when the mode stops", ACR_Hud.s_iCleared > before, string.Format("%1 removed", ACR_Hud.s_iCleared - before));
   ACR_PingDisplay.ShowHoldHint(ACR_EPing.LOOK);
   ACR_PingDisplay.ShowHoldHint(-1);
  }
  if (tick == 64) ACR_TestRegress.Complete("shared", CHECKS_SHARED);
 }
}
