// Server -> owner snapshots for the command map, once a second, so markers do
// not depend on distant soldiers being streamed to the client. Every action a
// client takes from these markers is re-validated by the server.
//
// Each side's picture is built once per second (ACR_FeedFrame), not once per
// player. A section is only sent to a player when it changed since the last
// one he got (positions of soldiers and squad leaders always are), plus a full
// resend every FULL_EVERY_MS. Ages and countdowns go out as "seconds at send
// time" and the client counts on from there, so they don't count as changes.
class ACR_FeedFrame : Managed
{
 string m_sKey;
 Faction m_Faction;
 // roster
 ref array<RplId> m_RosterIds = {};
 ref array<vector> m_RosterPos = {};
 ref array<int> m_RosterStates = {};
 ref array<string> m_RosterLabels = {};
 ref array<RplId> m_RosterGroups = {};
 // group markers
 ref array<RplId> m_Leaders = {};
 ref array<vector> m_LeaderPos = {};
 ref array<string> m_GroupNames = {};
 ref array<int> m_Symbols = {};
 ref array<int> m_LeaderStates = {};
 ref array<int> m_Commandable = {};
 ref array<RplId> m_GroupIds = {};
 ref array<string> m_Details = {};
 ref array<int> m_Status = {};       // per group: hp, ammo, fuel, stock, ordered by, order age (s)
 string m_sStatusSig;
 // orders
 ref array<RplId> m_OrderGroups = {};
 ref array<vector> m_OrderTargets = {};
 ref array<int> m_OrderKinds = {};
 ref array<int> m_OrderIssuers = {};
 string m_sOrdersSig;
 // bases, requisitions, supports, zones
 ref array<vector> m_BasePos = {};
 ref array<string> m_BaseNames = {};
 ref array<string> m_BaseOwners = {};
 ref array<string> m_BaseDetails = {};
 ref array<float> m_BaseRadii = {};
 ref array<int> m_BaseContested = {};
 ref array<int> m_BaseSupply = {}; // stock, capacity, delivery seconds; -1 stock = no friendly FOB
 string m_sBasesSig;
 ref array<string> m_ReqNames = {};
 ref array<int> m_ReqCosts = {};
 ref array<int> m_ReqRemaining = {};
 string m_sReqSig;
 ref array<string> m_SupNames = {};
 ref array<int> m_SupCosts = {};
 ref array<int> m_SupCooldowns = {};
 string m_sSupSig;
 ref array<vector> m_ZonePos = {};
 ref array<float> m_ZoneRadii = {};
 ref array<string> m_ZoneLabels = {};
 string m_sZonesSig;
 // intel and our empty vehicles
 ref array<vector> m_ContactPos = {};
 ref array<int> m_ContactKinds = {};
 ref array<int> m_ContactCounts = {};
 ref array<int> m_ContactAges = {};
 ref array<string> m_Feed = {};
 ref array<int> m_FeedAges = {};
 string m_sIntelSig;
 ref array<vector> m_VehiclePos = {};
 ref array<string> m_VehicleNames = {};
 ref array<int> m_VehicleAges = {};
 string m_sVehiclesSig;
 // shared command
 ref array<int> m_CmdIds = {};
 ref array<string> m_CmdTags = {};
 ref array<string> m_CmdActions = {};
 ref array<int> m_CmdActionAges = {};
 ref array<RplId> m_SelGroups = {};
 ref array<int> m_SelOwners = {};
 string m_sCmdSig;
 // pings
 ref array<int> m_PingIds = {};
 ref array<int> m_PingKinds = {};
 ref array<vector> m_PingPos = {};
 ref array<int> m_PingOwners = {};
 ref array<string> m_PingTags = {};
 ref array<int> m_PingAges = {};
 ref array<string> m_PingAnswers = {};
 string m_sPingsSig;
 // estimated payload sizes (bytes), for the perf log
 ref map<string, int> m_Bytes = new map<string, int>();
}

class ACR_MapFeed
{
 static const float FULL_EVERY_MS = 10000;
 protected static ref map<int, ref map<string, string>> s_Sent = new map<int, ref map<string, string>>();
 protected static ref map<int, float> s_LastFull = new map<int, float>();

 static void PublishAll(ACR_Manager manager)
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  // Forget players who left.
  array<int> gone = {};
  foreach (int known, map<string, string> unused : s_Sent) if (!players.Contains(known)) gone.Insert(known);
  foreach (int left : gone) { s_Sent.Remove(left); s_LastFull.Remove(left); }

  map<string, ref ACR_FeedFrame> frames = new map<string, ref ACR_FeedFrame>();
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc || !pc.GetControlledEntity()) continue;
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!faction) continue;
   string key = faction.GetFactionKey();
   ACR_FeedFrame frame;
   if (!frames.Find(key, frame))
   {
    frame = Build(manager, faction);
    frames.Insert(key, frame);
   }
   bool full = now - s_LastFull.Get(id) >= FULL_EVERY_MS;
   if (full) s_LastFull.Set(id, now);
   SendFrame(manager, pc, id, frame, full);
  }
  // The perf test has no players: build each side's frame anyway, to time it.
  if (ACR_Perf.s_bOn && players.IsEmpty())
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
   {
    Faction perfFaction = GetGame().GetFactionManager().GetFactionByKey(settings.m_sFaction);
    if (!perfFaction) continue;
    ACR_FeedFrame dry = Build(manager, perfFaction);
    foreach (string section, int bytes : dry.m_Bytes) ACR_Perf.s_iFeedBytes += bytes;
   }
 }

 // ---------------------------------------------------------------- sending
 protected static bool Changed(int playerId, string section, string signature, bool full)
 {
  map<string, string> sent;
  if (!s_Sent.Find(playerId, sent))
  {
   sent = new map<string, string>();
   s_Sent.Insert(playerId, sent);
  }
  string last;
  if (!full && sent.Find(section, last) && last == signature)
  {
   ACR_Perf.s_iFeedSkipped++;
   return false;
  }
  sent.Set(section, signature);
  return true;
 }

 protected static void Count(ACR_FeedFrame frame, string section)
 {
  ACR_Perf.s_iFeedSends++;
  ACR_Perf.s_iFeedBytes += frame.m_Bytes.Get(section);
 }

 protected static void SendFrame(ACR_Manager manager, SCR_PlayerController pc, int id, ACR_FeedFrame frame, bool full)
 {
  // Always: soldier and leader positions.
  IEntity original = pc.GetMainEntity();
  vector originalPos;
  if (original) originalPos = original.GetOrigin();
  pc.ACR_SendRoster(frame.m_RosterIds, frame.m_RosterPos, frame.m_RosterStates, frame.m_RosterLabels, frame.m_RosterGroups, originalPos);
  Count(frame, "roster");
  pc.ACR_SendGroupMarkers(frame.m_Leaders, frame.m_LeaderPos, frame.m_GroupNames, frame.m_Symbols, frame.m_LeaderStates, frame.m_Commandable, frame.m_GroupIds, frame.m_Details);
  Count(frame, "groups");
  if (Changed(id, "status", frame.m_sStatusSig, full)) { pc.ACR_SendGroupStatus(frame.m_GroupIds, frame.m_Status); Count(frame, "status"); }
  // Per player: command status (the countdown is counted on by the client).
  string key = frame.m_sKey;
  bool commander = manager.IsCommander(id);
  string text = manager.CommandStatus(id);
  int rp = manager.m_Bases.GetRP(key);
  int income = manager.m_Bases.GetIncome(key);
  int seconds = manager.m_Bases.SecondsToIncome();
  float nextIncome = GetGame().GetWorld().GetWorldTime() / 1000 + seconds;
  string statusSig = string.Format("%1|%2|%3|%4|%5|%6", commander, text, manager.m_SharedCommand, rp, income, (int)nextIncome);
  if (Changed(id, "command", statusSig, full)) pc.ACR_SendCommandStatus(commander, text, manager.m_SharedCommand, rp, income, seconds);
  bool debugAllowed = ACR_Manager.IsHostOrAdmin(id);
  string debugText;
  if (debugAllowed) debugText = manager.m_Debug.Status(manager, key);
  if (Changed(id, "debug", string.Format("%1|%2", debugAllowed, debugText), full)) pc.ACR_SendDebug(debugAllowed, debugText);
  string report = ACR_Report.Text(key);
  if (Changed(id, "report", report, full)) pc.ACR_SendReport(report);
  if (Changed(id, "bases", frame.m_sBasesSig, full)) { pc.ACR_SendBases(frame.m_BasePos, frame.m_BaseNames, frame.m_BaseOwners, frame.m_BaseDetails, frame.m_BaseRadii, frame.m_BaseContested, frame.m_BaseSupply); Count(frame, "bases"); }
  if (Changed(id, "reqs", frame.m_sReqSig, full)) { pc.ACR_SendRequisitions(frame.m_ReqNames, frame.m_ReqCosts, frame.m_ReqRemaining); Count(frame, "reqs"); }
  if (Changed(id, "sups", frame.m_sSupSig, full)) { pc.ACR_SendSupports(frame.m_SupNames, frame.m_SupCosts, frame.m_SupCooldowns); Count(frame, "sups"); }
  if (Changed(id, "zones", frame.m_sZonesSig, full)) { pc.ACR_SendZones(frame.m_ZonePos, frame.m_ZoneRadii, frame.m_ZoneLabels); Count(frame, "zones"); }
  if (Changed(id, "orders", frame.m_sOrdersSig, full)) { pc.ACR_SendOrders(frame.m_OrderGroups, frame.m_OrderTargets, frame.m_OrderKinds, frame.m_OrderIssuers); Count(frame, "orders"); }
  PublishCapture(pc, key);
  if (Changed(id, "intel", frame.m_sIntelSig, full)) { pc.ACR_SendIntel(frame.m_ContactPos, frame.m_ContactKinds, frame.m_ContactCounts, frame.m_ContactAges, frame.m_Feed, frame.m_FeedAges); Count(frame, "intel"); }
  if (Changed(id, "vehicles", frame.m_sVehiclesSig, full)) { pc.ACR_SendVehicles(frame.m_VehiclePos, frame.m_VehicleNames, frame.m_VehicleAges); Count(frame, "vehicles"); }
  if (Changed(id, "cmd", frame.m_sCmdSig, full)) { pc.ACR_SendCommanders(frame.m_CmdIds, frame.m_CmdTags, frame.m_CmdActions, frame.m_CmdActionAges, frame.m_SelGroups, frame.m_SelOwners); Count(frame, "cmd"); }
  if (Changed(id, "pings", frame.m_sPingsSig, full)) { pc.ACR_SendPings(frame.m_PingIds, frame.m_PingKinds, frame.m_PingPos, frame.m_PingOwners, frame.m_PingTags, frame.m_PingAges, frame.m_PingAnswers); Count(frame, "pings"); }
 }

 // ---------------------------------------------------------------- building
 protected static string P(vector v)
 {
  int x = v[0];
  int z = v[2];
  return x.ToString() + "," + z.ToString() + ";";
 }

 static ACR_FeedFrame Build(ACR_Manager manager, Faction faction)
 {
  ACR_FeedFrame frame = new ACR_FeedFrame();
  frame.m_Faction = faction;
  frame.m_sKey = faction.GetFactionKey();
  BuildRoster(manager, frame);
  BuildGroups(manager, frame);
  BuildOrders(manager, frame);
  BuildBases(manager, frame);
  BuildRequisitions(manager, frame);
  BuildSupports(manager, frame);
  BuildZones(manager, frame);
  BuildIntel(manager, frame);
  BuildCommanders(manager, frame);
  BuildPings(manager, frame);
  return frame;
 }

 // Every soldier of every friendly squad.
 protected static void BuildRoster(ACR_Manager manager, ACR_FeedFrame frame)
 {
  array<AIAgent> agents = {};
  int bytes;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad.GetFaction() != frame.m_Faction) continue;
   RplId groupId = SCR_EntityHelper.EntityToRplId(squad.m_Group);
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!soldier || soldier.GetCharacterController().IsDead()) continue;
    frame.m_RosterIds.Insert(SCR_EntityHelper.EntityToRplId(soldier));
    frame.m_RosterPos.Insert(soldier.GetOrigin());
    string label = string.Format("%1-%2", squad.GetName(), squad.NumberOf(soldier));
    frame.m_RosterLabels.Insert(label);
    frame.m_RosterGroups.Insert(groupId);
    int state = 0;
    if (ACR_Squad.FindController(soldier) != 0) state = 1;
    frame.m_RosterStates.Insert(state);
    bytes += 4 + 12 + 4 + label.Length() + 4 + 4;
   }
  }
  frame.m_Bytes.Set("roster", bytes + 12);
 }

 // A named, role-symbol marker at the leader of every friendly group: our
 // squads (commandable) and any other group such as player groups.
 protected static void BuildGroups(ACR_Manager manager, ACR_FeedFrame frame)
 {
  Faction faction = frame.m_Faction;
  array<SCR_AIGroup> groups = {};
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad.m_Group && squad.GetFaction() == faction) groups.Insert(squad.m_Group);
   // Private playable groups are omitted by the groups manager. Add the active
   // command group explicitly so fellow commanders keep a marker while mounted.
   if (squad.m_CommandGroup && squad.m_Commander > 0 && squad.GetFaction() == faction && !groups.Contains(squad.m_CommandGroup)) groups.Insert(squad.m_CommandGroup);
  }
  SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
  if (groupsManager)
  {
   array<SCR_AIGroup> playable = groupsManager.GetPlayableGroupsByFaction(faction);
   if (playable)
    foreach (SCR_AIGroup group : playable)
     if (group && !groups.Contains(group)) groups.Insert(group);
  }
  int bytes;
  string statusSig = "";
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (SCR_AIGroup group : groups)
  {
   if (!group || group.GetFaction() != faction) continue;
   ACR_Squad squad = manager.FindSquadByGroup(group);
   ACR_Squad commandSquad = manager.FindSquadByCommandGroup(group);
   if (!squad) squad = commandSquad;
   IEntity leader = group.GetLeaderEntity();
   if (squad) leader = squad.GetLeader();
   else if (!leader && group.GetLeaderID() > 0) leader = GetGame().GetPlayerManager().GetPlayerControlledEntity(group.GetLeaderID());
   ChimeraCharacter character = ChimeraCharacter.Cast(leader);
   if (!character || character.GetCharacterController().IsDead()) continue;
   RplId id = SCR_EntityHelper.EntityToRplId(leader);
   if (frame.m_Leaders.Contains(id)) continue;
   SCR_EditableGroupComponent editable = SCR_EditableGroupComponent.Cast(group.FindComponent(SCR_EditableGroupComponent));
   SCR_EditableGroupUIInfo info;
   if (editable) info = SCR_EditableGroupUIInfo.Cast(editable.GetInfo());
   SCR_MilitarySymbol symbol;
   if (info) symbol = info.GetMilitarySymbol();
   string name;
   if (squad) name = squad.GetName();
   if (name.IsEmpty()) name = group.GetCustomName();
   if (name.IsEmpty()) name = SCR_GroupHelperUI.GetTranslatedGroupName(group);
   if (name.IsEmpty() && info) name = info.GetName();
   if (name.IsEmpty()) name = "Friendly group";
   int icons = EMilitarySymbolIcon.INFANTRY;
   int dimension = EMilitarySymbolDimension.LAND;
   int amplifier = EMilitarySymbolAmplifier.SQUAD;
   if (symbol)
   {
    icons = symbol.GetIcons();
    dimension = symbol.GetDimension();
    amplifier = symbol.GetAmplifier();
   }
   // Vehicle crews show their vehicle's symbol, mounted or not; the label
   // says when they are out of it.
   IEntity crewVehicle;
   if (squad) crewVehicle = squad.MarkVehicle();
   if (crewVehicle)
   {
    int vehicleKind = ACR_Intel.VehicleKind(crewVehicle);
    icons = ACR_Intel.IconOf(vehicleKind);
    dimension = ACR_Intel.DimensionOf(vehicleKind);
    if (squad.m_OwnVehicle == crewVehicle && !squad.InOwnVehicle()) name += " (dismounted)";
   }
   frame.m_Leaders.Insert(id);
   IEntity markerAnchor = leader;
   IEntity ridden = ACR_Transport.VehicleOf(leader);
   if (ridden) markerAnchor = ridden;
   frame.m_LeaderPos.Insert(markerAnchor.GetOrigin());
   frame.m_GroupNames.Insert(name);
   frame.m_Symbols.Insert(icons);
   frame.m_Symbols.Insert(dimension);
   frame.m_Symbols.Insert(amplifier);
   int mode = 0;
   if (squad) mode = squad.m_Mode;
   if (squad && ACR_GunnerDrive.SquadGunner(squad)) mode |= 16; // + flag: has an AI-manned turret
   if (squad && squad.CanEstablishFob()) mode |= 32; // + flag: can establish FOB
   if (squad && !squad.IsVehicle() && squad.CountAlive() > 0) mode |= 64; // + flag: can build roadblock
   if (ACR_Minefields.CanLay(squad)) mode |= 128; // + flag: can lay / disarm mines
   frame.m_Symbols.Insert(mode); // 4 ints per group: icons, dimension, amplifier, move mode (+16 manned turret, +32 can establish FOB, +64 roadblock, +128 minefields)
   int state = 0;
   if (ACR_Squad.FindController(leader) > 0) state = 1;
   frame.m_LeaderStates.Insert(state);
   int canCommand = 0;
   if (squad) canCommand = 1;
   frame.m_Commandable.Insert(canCommand);
   RplId groupId = SCR_EntityHelper.EntityToRplId(group);
   frame.m_GroupIds.Insert(groupId);
   string detail;
   if (squad) detail = squad.Describe();
   frame.m_Details.Insert(detail);
   bytes += 4 + 12 + name.Length() + 4 + 16 + 4 + 4 + 4 + detail.Length() + 4;
   // Status bars and who gave the order: 6 ints per group.
   if (!squad)
   {
    for (int k = 0; k < 6; k++) frame.m_Status.Insert(-1);
    statusSig += "-;";
    continue;
   }
   int age = -1;
   if (squad.m_fOrderedAt > 0) age = (now - squad.m_fOrderedAt) / 1000;
   frame.m_Status.Insert(squad.m_iHealthPct);
   frame.m_Status.Insert(squad.m_iAmmoPct);
   frame.m_Status.Insert(squad.m_iFuelPct);
   frame.m_Status.Insert(squad.m_iStockPct);
   frame.m_Status.Insert(squad.m_iOrderedBy);
   frame.m_Status.Insert(age);
   int orderedAt = squad.m_fOrderedAt / 1000;
   statusSig += string.Format("%1,%2,%3,%4,%5,%6;", squad.m_iHealthPct, squad.m_iAmmoPct, squad.m_iFuelPct, squad.m_iStockPct, squad.m_iOrderedBy, orderedAt);
  }
  // Commanders are not always members of a registered squad or playable AI
  // group. Keep one server-authored marker for every living commander so it
  // remains visible to teammates while the player is mounted or detached.
  array<int> commanders = manager.GetCommanders(faction);
  if (commanders)
  {
   foreach (int playerId : commanders)
   {
    SCR_PlayerController commanderController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
    if (!commanderController) continue;
    IEntity unit = commanderController.GetMainEntity();
    ChimeraCharacter character = ChimeraCharacter.Cast(unit);
    if (!character || character.GetCharacterController().IsDead()) continue;
    RplId id = SCR_EntityHelper.EntityToRplId(unit);
    if (!id.IsValid()) continue;
    IEntity anchor = ACR_Transport.VehicleOf(unit);
    if (!anchor) anchor = unit;
    string label = "Command unit: " + GetGame().GetPlayerManager().GetPlayerName(playerId);
    int existing = frame.m_Leaders.Find(id);
    if (existing >= 0)
    {
     frame.m_LeaderPos[existing] = anchor.GetOrigin();
     frame.m_GroupNames[existing] = label;
     frame.m_Symbols[existing * 4 + 3] = 32;
     frame.m_Commandable[existing] = 0;
     continue;
    }
    frame.m_Leaders.Insert(id);
    frame.m_LeaderPos.Insert(anchor.GetOrigin());
    frame.m_GroupNames.Insert(label);
    frame.m_Symbols.Insert(EMilitarySymbolIcon.INFANTRY);
    frame.m_Symbols.Insert(EMilitarySymbolDimension.LAND);
    frame.m_Symbols.Insert(EMilitarySymbolAmplifier.SQUAD);
    frame.m_Symbols.Insert(32); // persistent commander marker; excluded from F7 possession
    int commandState;
    if (ACR_Squad.FindController(unit) > 0) commandState = 1;
    frame.m_LeaderStates.Insert(commandState);
    frame.m_Commandable.Insert(0);
    frame.m_GroupIds.Insert(RplId.Invalid());
    frame.m_Details.Insert("");
    for (int k = 0; k < 6; k++) frame.m_Status.Insert(-1);
    bytes += 4 + 12 + label.Length() + 4 + 16 + 4 + 4 + 4 + 4;
    statusSig += "-;" + id.AsString() + ";";
   }
  }
  frame.m_Bytes.Set("groups", bytes + 32);
  frame.m_Bytes.Set("status", frame.m_GroupIds.Count() * 28 + 8);
  // The status list is keyed by the group list, which can change on its own.
  foreach (RplId g : frame.m_GroupIds) statusSig += g.AsString() + ";";
  frame.m_sStatusSig = statusSig;
 }

 // Current destination of every friendly squad, then the rest of its plan.
 protected static void BuildOrders(ACR_Manager manager, ACR_FeedFrame frame)
 {
  string sig = "";
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad.GetFaction() != frame.m_Faction) continue;
   vector target;
   int kind;
   if (!squad.GetOrderTarget(target, kind)) continue;
   RplId groupId = SCR_EntityHelper.EntityToRplId(squad.m_Group);
   frame.m_OrderGroups.Insert(groupId);
   frame.m_OrderTargets.Insert(target);
   frame.m_OrderKinds.Insert(kind);
   frame.m_OrderIssuers.Insert(squad.m_iOrderedBy);
   sig += groupId.AsString() + ":" + P(target) + kind.ToString() + "," + squad.m_iOrderedBy.ToString() + ";";
   array<vector> planned = {};
   array<int> plannedKinds = {};
   squad.GetPlannedPoints(planned, plannedKinds);
   foreach (int p, vector point : planned)
   {
    frame.m_OrderGroups.Insert(groupId);
    frame.m_OrderTargets.Insert(point);
    frame.m_OrderKinds.Insert(plannedKinds[p]);
    frame.m_OrderIssuers.Insert(squad.m_iOrderedBy);
    sig += P(point) + plannedKinds[p].ToString() + ";";
   }
  }
  frame.m_sOrdersSig = sig;
  frame.m_Bytes.Set("orders", frame.m_OrderGroups.Count() * 24 + 16);
 }

 // Bases this faction can see (hidden enemy bases only once discovered).
 protected static void BuildBases(ACR_Manager manager, ACR_FeedFrame frame)
 {
  string sig = "";
  int bytes;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.IsVisibleTo(frame.m_sKey)) continue;
   string detail = manager.m_Bases.Describe(base);
   if (base.m_bBuilt && base.m_sOwner == frame.m_sKey)
   {
    detail += ACR_FobSupply.Describe(base);
    detail += ACR_FobSlots.Describe(base);
   }
   frame.m_BasePos.Insert(base.GetOrigin());
   frame.m_BaseNames.Insert(base.GetLabel());
   frame.m_BaseOwners.Insert(base.m_sOwner);
   frame.m_BaseDetails.Insert(detail);
   frame.m_BaseRadii.Insert(base.m_fRadius);
   int flag = 0;
   if (base.m_bContested) flag = 1;
   frame.m_BaseContested.Insert(flag);
   int stock = -1;
   if (base.m_bBuilt && base.m_sOwner == frame.m_sKey && base.m_FobSupply) stock = ACR_FobSupply.Stock(base.m_FobSupply);
   frame.m_BaseSupply.Insert(stock);
   frame.m_BaseSupply.Insert(ACR_FobSupply.CapacityOf(base));
   int delivery = 0;
   if (stock >= 0 && base.m_iSupplyDelivery > 0) delivery = Math.Max(1, Math.Ceil(base.m_fSupplyDeliveryLeft / 1000));
   if (stock >= 0 && base.m_bHqLost) delivery = -1; // HQ destroyed: the panel offers nothing
   frame.m_BaseSupply.Insert(delivery);
   string slotSig = "";
   for (int s = 0; s < ACR_FobSlots.UPGRADE_COUNT; s++)
   {
    int slotState = -1;
    if (stock >= 0) slotState = ACR_FobSlots.FeedState(base, s);
    frame.m_BaseSupply.Insert(slotState);
    slotSig += slotState.ToString() + ",";
   }
   sig += base.GetLabel() + "|" + base.m_sOwner + "|" + detail + "|" + flag.ToString() + "|" + slotSig + ";";
   bytes += 12 + base.GetLabel().Length() + base.m_sOwner.Length() + detail.Length() + 4 * (8 + ACR_FobSlots.UPGRADE_COUNT);
  }
  frame.m_sBasesSig = sig;
  frame.m_Bytes.Set("bases", bytes + 28);
 }

 protected static void BuildRequisitions(ACR_Manager manager, ACR_FeedFrame frame)
 {
  string sig = "";
  int bytes;
  foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction(frame.m_sKey).m_aRequisitions)
  {
   frame.m_ReqNames.Insert(entry.m_sName);
   frame.m_ReqCosts.Insert(entry.m_iCost);
   frame.m_ReqRemaining.Insert(entry.m_iRemaining);
   sig += entry.m_sName + "," + entry.m_iCost.ToString() + "," + entry.m_iRemaining.ToString() + ";";
   bytes += entry.m_sName.Length() + 12;
  }
  frame.m_sReqSig = sig;
  frame.m_Bytes.Set("reqs", bytes + 12);
 }

 protected static void BuildSupports(ACR_Manager manager, ACR_FeedFrame frame)
 {
  string sig = "";
  int bytes;
  float now = GetGame().GetWorld().GetWorldTime();
  string mortars = manager.Mortars().StatusFor(frame.m_sKey);
  foreach (ACR_SupportEntry entry : manager.m_Bases.GetFaction(frame.m_sKey).m_aSupports)
  {
   // Mortar supports say whether a team can fire them.
   // Helicopter supports (worlds with helipads) say whether a stationed helicopter is ready.
   string heli = "";
   if (ACR_HeliStation.IsHeliType(entry.m_eType) && ACR_HeliStation.UsesPads(frame.m_sKey) && manager.m_Supports)
    heli = manager.m_Supports.Station().StatusFor(frame.m_sKey, ACR_HeliStation.NeedsGunship(entry.m_eType));
   if (ACR_Mortars.IsMortarType(entry.m_eType)) frame.m_SupNames.Insert(entry.m_sName + "  [" + mortars + "]");
   else if (!heli.IsEmpty()) frame.m_SupNames.Insert(entry.m_sName + "  [" + heli + "]");
   else frame.m_SupNames.Insert(entry.m_sName);
   frame.m_SupCosts.Insert(entry.m_iCost);
   if (ACR_Mortars.IsMortarType(entry.m_eType)) sig += mortars;
   sig += heli;
   int left = Math.Ceil((entry.m_fReadyAt - now) / 1000);
   if (left < 0) left = 0;
   frame.m_SupCooldowns.Insert(left);
   int readyAt = 0;
   if (left > 0) readyAt = entry.m_fReadyAt / 1000;
   sig += entry.m_sName + "," + entry.m_iCost.ToString() + "," + readyAt.ToString() + ";";
   bytes += entry.m_sName.Length() + 12;
  }
  frame.m_sSupSig = sig;
  frame.m_Bytes.Set("sups", bytes + 12);
 }

 protected static void BuildZones(ACR_Manager manager, ACR_FeedFrame frame)
 {
  string sig = "";
  if (manager.m_Supports)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   foreach (ACR_SupportZone zone : manager.m_Supports.m_Zones)
   {
    if (zone.m_sFaction != frame.m_sKey) continue;
    frame.m_ZonePos.Insert(zone.m_Pos);
    frame.m_ZoneRadii.Insert(zone.m_fRadius);
    int left = Math.Ceil((zone.m_fImpactAt - now) / 1000);
    string label = zone.m_sName + " - ACTIVE";
    if (left > 0) label = string.Format("%1 - %2 s", zone.m_sName, left);
    frame.m_ZoneLabels.Insert(label);
    sig += P(zone.m_Pos) + label + ";";
   }
  }
  // Set-up mortar teams' reach ("~" label: drawn as a range ring, not a danger area).
  manager.Mortars().Ranges(frame.m_sKey, frame.m_ZonePos, frame.m_ZoneRadii, frame.m_ZoneLabels);
  // The side's own minefields (danger areas, labelled with the mines left).
  ACR_Minefields.Zones(frame.m_sKey, frame.m_ZonePos, frame.m_ZoneRadii, frame.m_ZoneLabels);
  foreach (int r, vector rangePos : frame.m_ZonePos) sig += P(rangePos) + frame.m_ZoneLabels[r] + ";";
  frame.m_sZonesSig = sig;
  frame.m_Bytes.Set("zones", sig.Length() + frame.m_ZonePos.Count() * 16 + 12);
 }

 // Spotted enemies (hostile markers), the side's feed, our known empty vehicles.
 protected static void BuildIntel(ACR_Manager manager, ACR_FeedFrame frame)
 {
  ACR_Intel intel = manager.IntelFor(frame.m_sKey);
  if (!intel) return;
  float now = GetGame().GetWorld().GetWorldTime();
  string sig = "";
  foreach (ACR_Contact contact : intel.m_Contacts)
  {
   frame.m_ContactPos.Insert(contact.m_Pos);
   frame.m_ContactKinds.Insert(contact.m_iKind);
   frame.m_ContactCounts.Insert(contact.m_iCount);
   int age = (now - contact.m_fSeen) / 1000;
   frame.m_ContactAges.Insert(age);
   int seen = contact.m_fSeen / 1000;
   sig += P(contact.m_Pos) + contact.m_iKind.ToString() + "," + contact.m_iCount.ToString() + "," + seen.ToString() + ";";
  }
  int feedBytes;
  foreach (int i, string line : intel.m_Feed)
  {
   int seconds = (now - intel.m_FeedTime[i]) / 1000;
   if (seconds > 600) break; // older than 10 min: not news any more
   frame.m_Feed.Insert(line);
   frame.m_FeedAges.Insert(seconds);
   int at = intel.m_FeedTime[i] / 1000;
   sig += line + at.ToString() + ";";
   feedBytes += line.Length() + 8;
  }
  frame.m_sIntelSig = sig;
  frame.m_Bytes.Set("intel", frame.m_ContactPos.Count() * 24 + feedBytes + 24);
  string vehicleSig = "";
  int vehicleBytes;
  foreach (IEntity vehicle, float seenAt : intel.m_VehicleSeen)
  {
   if (!vehicle) continue;
   vector pos = intel.m_VehicleAt.Get(vehicle);
   frame.m_VehiclePos.Insert(pos);
   string vehicleName = ACR_Vehicles.NameOf(vehicle);
   foreach (ACR_Squad crew : manager.m_Squads)
    if (crew.m_OwnVehicle == vehicle) { vehicleName += ", crew: " + crew.GetName(); break; }
   frame.m_VehicleNames.Insert(vehicleName);
   int vehicleAge = (now - seenAt) / 1000;
   frame.m_VehicleAges.Insert(vehicleAge);
   int seenSeconds = seenAt / 1000;
   // Seen continuously: the age stays near 0, so only the spot matters.
   if (vehicleAge <= 2) seenSeconds = 0;
   vehicleSig += P(pos) + vehicleName + "," + seenSeconds.ToString() + ";";
   vehicleBytes += 12 + vehicleName.Length() + 8;
  }
  frame.m_sVehiclesSig = vehicleSig;
  frame.m_Bytes.Set("vehicles", vehicleBytes + 12);
 }

 // Shared command: the side's commanders, their tags, last actions and selections.
 protected static void BuildCommanders(ACR_Manager manager, ACR_FeedFrame frame)
 {
  ACR_Command command = manager.Command();
  command.TagsOf(frame.m_sKey, frame.m_CmdIds, frame.m_CmdTags);
  float now = GetGame().GetWorld().GetWorldTime();
  string sig = "";
  foreach (int i, int id : frame.m_CmdIds)
  {
   string action = command.LastAction(id);
   float at = command.LastActionAt(id);
   int age = -1;
   if (at > 0) age = (now - at) / 1000;
   frame.m_CmdActions.Insert(action);
   frame.m_CmdActionAges.Insert(age);
   int atSeconds = at / 1000;
   sig += id.ToString() + "," + frame.m_CmdTags[i] + "," + action + "," + atSeconds.ToString() + ";";
  }
  command.SelectionsOf(frame.m_sKey, frame.m_SelGroups, frame.m_SelOwners);
  foreach (int s, RplId group : frame.m_SelGroups) sig += group.AsString() + ">" + frame.m_SelOwners[s].ToString() + ";";
  frame.m_sCmdSig = sig;
  frame.m_Bytes.Set("cmd", sig.Length() + 32);
 }

 protected static void BuildPings(ACR_Manager manager, ACR_FeedFrame frame)
 {
  array<ACR_Ping> pings = {};
  manager.Pings().Of(frame.m_sKey, pings);
  float now = GetGame().GetWorld().GetWorldTime();
  string sig = manager.Pings().m_iSerial.ToString() + ";";
  int bytes;
  foreach (ACR_Ping ping : pings)
  {
   frame.m_PingIds.Insert(ping.m_iId);
   frame.m_PingKinds.Insert(ping.m_iKind);
   frame.m_PingPos.Insert(ping.m_Pos);
   frame.m_PingOwners.Insert(ping.m_iOwner);
   frame.m_PingTags.Insert(ping.m_sTag);
   int age = (now - ping.m_fPlaced) / 1000;
   frame.m_PingAges.Insert(age);
   frame.m_PingAnswers.Insert(ping.m_sAnswer);
   bytes += 4 + 4 + 12 + 4 + ping.m_sTag.Length() + 4 + 4 + ping.m_sAnswer.Length() + 4;
  }
  frame.m_sPingsSig = sig;
  frame.m_Bytes.Set("pings", bytes + 28);
 }

 // Capture bar for a player standing in a base ring (empty label = hide).
 //  tone: 0 ours / neutral progress, 1 we are taking it, 2 the enemy is taking it
 static void PublishCapture(SCR_PlayerController pc, string key)
 {
  IEntity body = pc.GetControlledEntity();
  string label;
  float progress;
  int tone;
  if (body)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (vector.DistanceXZ(body.GetOrigin(), base.GetOrigin()) > base.m_fRadius || !base.IsVisibleTo(key)) continue;
    if (base.m_sOwner == key)
    {
     if (!base.m_bContested && base.m_fScore >= 100) break;
     label = base.m_sName + ": holding";
     if (base.m_bContested) label = base.m_sName + ": under attack";
     progress = (base.m_fScore + 100) / 200;
     tone = 2;
     if (!base.m_bContested) tone = 0;
    }
    else if (!base.m_sOwner.IsEmpty())
    {
     label = "Capturing " + base.m_sName;
     progress = (100 - base.m_fScore) / 200;
     tone = 1;
    }
    else if (base.m_sCapturer == key)
    {
     label = "Capturing " + base.m_sName;
     progress = base.m_fScore / 100;
     tone = 1;
    }
    else if (!base.m_sCapturer.IsEmpty())
    {
     label = base.m_sName + ": enemy capturing";
     progress = base.m_fScore / 100;
     tone = 2;
    }
    else
    {
     label = base.m_sName + ": neutral";
     progress = 0;
     tone = 0;
    }
    if (base.m_bContested && tone == 1) label += " (contested)";
    break;
   }
  }
  pc.ACR_SendCapture(label, progress, tone);
 }
}
