// Command panel updates/actions, aim drag, and map right-click dispatch; extracted from ACR_PlayerController.c.
class ACR_PlayerPanel
{
 static void ACR_RefreshKeyHintsButton(SCR_PlayerController pc)
 {
  if (pc.m_ACR_Panel) pc.m_ACR_Panel.RefreshKeyHints();
 }

 static void ACR_UpdatePanel(SCR_PlayerController pc)
 {
  if (!pc.m_ACR_Panel) return;
  pc.m_ACR_Panel.TickDrag();
  string reinforceFeedback = "";
  if (GetGame().GetWorld().GetWorldTime() - pc.m_ACR_ReinforceFeedbackAt < 12000) reinforceFeedback = pc.m_ACR_ReinforceFeedback;
  pc.m_ACR_Panel.SetReinforceFeedback(reinforceFeedback);
  pc.m_ACR_Panel.SetDebug(pc.m_ACR_DebugAllowed, pc.m_ACR_DebugOpen, pc.m_ACR_DebugStatus, pc.m_ACR_SelectedBase, pc.m_ACR_DebugOwn, pc.m_ACR_DebugEnemy, pc.m_ACR_DebugPaused, pc.m_ACR_DebugReveal);
  bool canTake = !pc.m_ACR_IsCommander && (pc.m_ACR_SharedCommand || pc.m_ACR_CommandText == "Command: AI");
  pc.m_ACR_Panel.SetCommand(pc.m_ACR_CommandText, pc.m_ACR_IsCommander, canTake);
  int incomeIn = Math.Max(0, pc.m_ACR_IncomeSeconds - ACR_MapData.Since(pc.m_ACR_IncomeAt));
  pc.m_ACR_Panel.SetEconomy(string.Format("RP %1   +%2 in %3s", pc.m_ACR_RP, pc.m_ACR_Income, incomeIn));
  pc.m_ACR_Panel.SetRoster(pc.ACR_RosterText());
  if (!pc.m_ACR_IsCommander) pc.m_ACR_PendingEntry = -1;
  if (!pc.m_ACR_IsCommander) pc.m_ACR_PendingSupport = -1;
  if (!pc.m_ACR_IsCommander) pc.m_ACR_PendingNetwork = -1;
  string myFactionKey = "";
  Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
  if (localFaction) myFactionKey = localFaction.GetFactionKey();
  bool insurgentViewer = ACR_InsurgentSide.IsInsurgentColor(myFactionKey) && pc.m_ACR_SupNames.IsEmpty();
  pc.m_ACR_Panel.SetRequisitions(pc.m_ACR_IsCommander, pc.m_ACR_RequisitionOpen, pc.m_ACR_ReqNames, pc.m_ACR_ReqCosts, pc.m_ACR_ReqRemaining, pc.m_ACR_RP, pc.m_ACR_PendingEntry);
  array<int> cooldowns = {};
  pc.ACR_Cooldowns(cooldowns);
  pc.m_ACR_Panel.SetSupports(pc.m_ACR_IsCommander, pc.m_ACR_SupportOpen, pc.m_ACR_SupNames, pc.m_ACR_SupCosts, cooldowns, pc.m_ACR_RP, pc.m_ACR_PendingSupport);
  pc.m_ACR_Panel.SetNetwork(pc.m_ACR_IsCommander && insurgentViewer, pc.m_ACR_NetworkOpen, pc.m_ACR_RP, pc.m_ACR_PendingNetwork);
  pc.m_ACR_Panel.SetMusterMode(pc.m_ACR_MusterMode);
  pc.m_ACR_Panel.SetReport(pc.m_ACR_ReportOpen, pc.m_ACR_ReportText);
  if (pc.m_ACR_PendingNetwork >= 0)
  {
   pc.m_ACR_Panel.SetBase(ACR_InsurgentNetwork.NameOf(pc.m_ACR_PendingNetwork), ACR_InsurgentNetwork.HintOf(pc.m_ACR_PendingNetwork));
   return;
  }
  if (pc.m_ACR_HasSupportTarget && pc.m_ACR_SupportOpen)
  {
   pc.m_ACR_Panel.SetBase("Support for " + pc.m_ACR_SupportTargetFor, "Pick a support above: it fires on the ping. Click Supports to cancel.");
   return;
  }
  if (pc.m_ACR_PendingSupport >= 0 && pc.m_ACR_PendingSupport < pc.m_ACR_SupNames.Count())
  {
   string supportHint = "Right-click the target on the map. Click the entry again to cancel.";
   string supportName = pc.m_ACR_SupNames[pc.m_ACR_PendingSupport];
   supportName.ToLower();
   if (supportName.Contains("air lift")) supportHint = "Select the squad to fly, then right-click where it should land. Click the entry again to cancel.";
   pc.m_ACR_Panel.SetBase(pc.m_ACR_SupNames[pc.m_ACR_PendingSupport], supportHint);
   return;
  }
  if (!pc.m_ACR_SupportResult.IsEmpty() && System.GetTickCount() - pc.m_ACR_SupportResultAt < 10000)
  {
   pc.m_ACR_Panel.SetBase("Support call", pc.m_ACR_SupportResult);
   return;
  }
  if (pc.m_ACR_PendingEntry >= 0 && pc.m_ACR_PendingEntry < pc.m_ACR_ReqNames.Count())
  {
   string deployHint = "Click one of your bases to deploy. Right-click to cancel.";
   if (pc.m_ACR_MusterMode && insurgentViewer) deployHint = "Click any covered spot on the map to muster this band (+25% RP). Right-click to cancel.";
   pc.m_ACR_Panel.SetBase(pc.m_ACR_ReqNames[pc.m_ACR_PendingEntry], deployHint);
   return;
  }
  if (!pc.m_ACR_SelectedBase.IsEmpty())
  {
   int baseIndex = pc.m_ACR_Data.m_BaseNames.Find(pc.m_ACR_SelectedBase);
   if (baseIndex >= 0)
   {
    pc.m_ACR_Panel.SetBase(pc.m_ACR_Data.m_BaseNames[baseIndex], ACR_CommandPanel.FobDetail(pc.m_ACR_Data.m_BaseDetails[baseIndex]));
    bool isHideout = pc.m_ACR_Data.m_BaseDetails[baseIndex].StartsWith("Hideout |");
    pc.m_ACR_Panel.SetHideout(isHideout && pc.m_ACR_IsCommander, isHideout && pc.m_ACR_IsCommander && (pc.m_ACR_Data.m_BaseContested[baseIndex] & 1) == 0);
    int stride = 3 + ACR_FobSlots.UPGRADE_COUNT;
    int supplyOffset = baseIndex * stride;
    if (pc.m_ACR_Data.m_BaseSupply.Count() >= supplyOffset + stride)
    {
     int stock = pc.m_ACR_Data.m_BaseSupply[supplyOffset];
     int capacity = pc.m_ACR_Data.m_BaseSupply[supplyOffset + 1];
     int delivery = pc.m_ACR_Data.m_BaseSupply[supplyOffset + 2];
     // delivery -1: the FOB's HQ is destroyed and it is offline until rebuilt.
     bool ready = pc.m_ACR_IsCommander && (pc.m_ACR_Data.m_BaseContested[baseIndex] & 1) == 0 && delivery >= 0;
     string factionKey = "";
     Faction myFaction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
     if (myFaction) factionKey = myFaction.GetFactionKey();
     array<int> slotStates = {};
     for (int s = 0; s < ACR_FobSlots.UPGRADE_COUNT; s++)
     {
      slotStates.Insert(pc.m_ACR_Data.m_BaseSupply[supplyOffset + 3 + s]);
     }
     pc.m_ACR_Panel.SetFob(stock >= 0, ready && capacity - stock >= ACR_FobSupply.DELIVERY_AMOUNT && delivery == 0 && pc.m_ACR_RP >= ACR_FobSupply.DELIVERY_COST, ready && stock > 0, slotStates, stock, ready, factionKey);
     pc.m_ACR_Panel.SetFobSupplyRuns((pc.m_ACR_Data.m_BaseContested[baseIndex] & 4) != 0);
     pc.m_ACR_Panel.SetFobAI(pc.m_ACR_IsCommander && delivery >= 0, (pc.m_ACR_Data.m_BaseContested[baseIndex] & 2) != 0);
    }
    else if (pc.m_ACR_Data.m_BaseSupply.Count() > supplyOffset + 2)
    {
     int stock = pc.m_ACR_Data.m_BaseSupply[supplyOffset];
     int capacity = pc.m_ACR_Data.m_BaseSupply[supplyOffset + 1];
     int delivery = pc.m_ACR_Data.m_BaseSupply[supplyOffset + 2];
     bool ready = pc.m_ACR_IsCommander && (pc.m_ACR_Data.m_BaseContested[baseIndex] & 1) == 0;
     pc.m_ACR_Panel.SetFob(stock >= 0, ready && capacity - stock >= ACR_FobSupply.DELIVERY_AMOUNT && delivery == 0 && pc.m_ACR_RP >= ACR_FobSupply.DELIVERY_COST, ready && stock > 0);
    }
    return;
   }
   pc.m_ACR_SelectedBase = "";
  }
  int index = -1;
  if (pc.m_ACR_SelectedGroup.IsValid()) index = pc.m_ACR_Data.m_GroupIds.Find(pc.m_ACR_SelectedGroup);
  if (index < 0)
  {
   // Selection vanished (squad wiped out, or not in the latest snapshot yet).
   pc.m_ACR_Panel.SetGroup("", "", false, false, 0, 0, "");
   return;
  }
  bool friendly = index < pc.m_ACR_Data.m_Commandable.Count() && pc.m_ACR_Data.m_Commandable[index] == 1;
  int modeAndFlags = pc.m_ACR_GroupSymbols[index * 4 + 3];
  bool canEstablishFob = friendly && (modeAndFlags & 32) != 0 && pc.m_ACR_Data.m_Selected.Count() <= 1;
  if (!canEstablishFob && pc.m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB)
   pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  bool canSetupHideout = friendly && insurgentViewer && pc.m_ACR_Data.m_Selected.Count() <= 1;
  if (!canSetupHideout && pc.m_ACR_CursorMode == ACR_ECursorMode.SETUP_HIDEOUT)
   pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  pc.m_ACR_Panel.SetSetupHideout(canSetupHideout);
  bool canRoadblock = friendly && (modeAndFlags & 64) != 0 && pc.m_ACR_Data.m_Selected.Count() <= 1;
  int roadblock = ACR_CommandPanel.RoadblockOfMode(pc.m_ACR_CursorMode);
  if (!canRoadblock && roadblock >= 0)
  {
   pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
   roadblock = -1;
  }
  string roadblockFaction = "";
  Faction roadblockSide = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (roadblockSide) roadblockFaction = roadblockSide.GetFactionKey();
  bool canMines = friendly && (modeAndFlags & 128) != 0 && pc.m_ACR_Data.m_Selected.Count() <= 1;
  if (!canMines && pc.m_ACR_CursorMode >= ACR_ECursorMode.MINES_AP && pc.m_ACR_CursorMode <= ACR_ECursorMode.DISARM_MINES)
   pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  bool canEvac = friendly && (modeAndFlags & 4096) != 0 && pc.m_ACR_Data.m_Selected.Count() <= 1;
  if (!canEvac && pc.m_ACR_CursorMode == ACR_ECursorMode.EVAC)
   pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  bool canFortify = friendly && (modeAndFlags & 256) != 0 && pc.m_ACR_Data.m_Selected.Count() <= 1;
  if (!canFortify && pc.m_ACR_CursorMode == ACR_ECursorMode.FORTIFY)
   pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  string hint;
  if (!friendly) hint = "Player group: not commandable";
  else if (!pc.m_ACR_IsCommander) hint = "Take command to give orders";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.DEFEND) hint = "Right-click: defend there, on a road an ambush (click Defend again to cancel). Dragged: posts where you pressed facing where you let go, an ambush when you let go on a road.";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.GARRISON) hint = "Right-click a building: they hold its windows and doors, filling the next buildings if needed (click Garrison again to cancel)";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.GET_IN) hint = "Right-click near a vehicle: get in (click Get in again to cancel)";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.LZ) hint = "Right-click the landing zone: they jump out when their ride sets down there";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.RIDE) hint = "Right-click the destination: a free truck nearby picks them up and drops them there";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB) hint = "Right-click a clear site: establish forward base (click Establish FOB again to cancel)";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.SETUP_HIDEOUT) hint = "Right-click a building outside towns and bases: set up hideout, 25 RP (click again to cancel)";
  else if (roadblock >= 0) hint = ACR_Roadblocks.Hint(roadblock, roadblockFaction);
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.FORTIFY) hint = ACR_Fortify.Hint();
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.EVAC) hint = "Right-click within 600 m of a town holding out (Civilians ring): its people walk to that evac point, they go there; park a vehicle on it to load them, then drive them to a base (click Evac point again to cancel)";
  else if (pc.m_ACR_CursorMode == ACR_ECursorMode.QRF) hint = "Right-click its post: it stands by there, goes after enemies your side spots within 300 m and comes back (click QRF again to cancel)";
  else if (pc.m_ACR_CursorMode >= ACR_ECursorMode.MINES_AP && pc.m_ACR_CursorMode <= ACR_ECursorMode.DISARM_MINES) hint = ACR_Minefields.Hint(ACR_Minefields.KindOfCursor(pc.m_ACR_CursorMode)) + pc.ACR_MineShapeHint();
  else hint = "Right-click: move (enemy: hunt, vehicle: get in, own base: defend)";
  if (friendly && pc.m_ACR_IsCommander && ACR_Aimable(pc)) hint += " Right-click and drag to set the way it faces (a right-drag aims while this is armed, it doesn't pan the map).";
  string title = pc.m_ACR_Data.m_GroupNames[index];
  string detail = pc.m_ACR_GroupDetails[index];
  string orderedBy = pc.ACR_OrderedByText(pc.m_ACR_SelectedGroup);
  if (!orderedBy.IsEmpty()) detail += "\n" + orderedBy;
  if (pc.m_ACR_Data.m_Selected.Count() > 1)
  {
   title = string.Format("%1 squads selected", pc.m_ACR_Data.m_Selected.Count());
   string names;
   foreach (RplId member : pc.m_ACR_Data.m_Selected)
   {
    int memberIndex = pc.m_ACR_Data.m_GroupIds.Find(member);
    if (memberIndex < 0) continue;
    if (!names.IsEmpty()) names += ", ";
    names += pc.m_ACR_Data.m_GroupNames[memberIndex];
   }
   detail = names + " | orders go to all (switch: first)";
  }
  if (friendly && pc.m_ACR_IsCommander && pc.m_ACR_CursorMode == ACR_ECursorMode.MOVE) hint += "  |  Shift+right-click: add waypoint  |  Ctrl+drag: select squads";
  pc.m_ACR_Panel.SetGroup(title, detail, friendly, pc.m_ACR_IsCommander, modeAndFlags & 15, pc.m_ACR_CursorMode, hint);
  pc.m_ACR_Panel.SetGunner(friendly && (modeAndFlags & 16) != 0 && pc.m_ACR_Data.m_Selected.Count() <= 1);
  pc.m_ACR_Panel.SetSquadAI(friendly && pc.m_ACR_IsCommander && pc.m_ACR_Data.m_Selected.Count() <= 1, (modeAndFlags & 1024) != 0);
  pc.m_ACR_Panel.SetPatrol((modeAndFlags & 512) != 0);
  pc.m_ACR_Panel.SetEstablishFob(canEstablishFob);
  pc.m_ACR_Panel.SetEvac(canEvac);
  pc.m_ACR_Panel.SetRoadblock(canRoadblock, !ACR_Roadblocks.IsFia(roadblockFaction));
  pc.m_ACR_Panel.SetMines(canMines);
  pc.m_ACR_Panel.SetFortify(canFortify);
  // Also for a full squad low on ammo: inside a deploy base Reinforce restocks it
  // (the server says so when it isn't in one).
  string details = pc.m_ACR_GroupDetails[index];
  bool missing = details.Contains("missing");
  bool lowAmmo = details.Contains("| ammo ") && !details.Contains("| ammo 100%");
  pc.m_ACR_Panel.SetReinforce("Reinforce (3 RP/soldier; FOB instant service extra)", missing || lowAmmo);
 }

 static void ACR_OnPanelAction(SCR_PlayerController pc, int action)
 {
  pc.ACR_MarkUiClick();
  switch (action)
  {
   case ACR_EPanelAction.TOGGLE_COMMAND:
    if (pc.m_ACR_IsCommander) pc.ACR_Request(ACR_ERequest.LEAVE_COMMAND);
    else pc.ACR_Request(ACR_ERequest.TAKE_COMMAND);
    return;
   case ACR_EPanelAction.TOGGLE_RTS_CAMERA:
    pc.ACR_ToggleRTSCamera();
    return;
   case ACR_EPanelAction.SWITCH: pc.ACR_Request(ACR_ERequest.SWITCH_TO_GROUP, pc.m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.TAKE_GUNNER: pc.ACR_Request(ACR_ERequest.TAKE_GUNNER, pc.m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.MODE_COMBAT: pc.ACR_RequestSelected(ACR_ERequest.MODE_COMBAT); return;
   case ACR_EPanelAction.MODE_FAST: pc.ACR_RequestSelected(ACR_ERequest.MODE_FAST); return;
   case ACR_EPanelAction.MODE_SILENT: pc.ACR_RequestSelected(ACR_ERequest.MODE_SILENT); return;
   // Defend / Get in arm the next right-click; pressing the same button again
   // cancels back to a plain move.
   case ACR_EPanelAction.DEFEND:
    if (pc.m_ACR_CursorMode == ACR_ECursorMode.DEFEND) pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else pc.m_ACR_CursorMode = ACR_ECursorMode.DEFEND;
    return;
   case ACR_EPanelAction.GARRISON:
    if (pc.m_ACR_CursorMode == ACR_ECursorMode.GARRISON) pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else pc.m_ACR_CursorMode = ACR_ECursorMode.GARRISON;
    return;
   case ACR_EPanelAction.GET_IN:
    if (pc.m_ACR_CursorMode == ACR_ECursorMode.GET_IN) pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else pc.m_ACR_CursorMode = ACR_ECursorMode.GET_IN;
    return;
   case ACR_EPanelAction.SET_LZ:
    if (pc.m_ACR_CursorMode == ACR_ECursorMode.LZ) pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else pc.m_ACR_CursorMode = ACR_ECursorMode.LZ;
    return;
   case ACR_EPanelAction.RIDE:
    if (pc.m_ACR_CursorMode == ACR_ECursorMode.RIDE) pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else pc.m_ACR_CursorMode = ACR_ECursorMode.RIDE;
    return;
   case ACR_EPanelAction.GET_OUT: pc.ACR_RequestSelected(ACR_ERequest.ORDER_GET_OUT); return;
   case ACR_EPanelAction.UNLOAD: pc.ACR_RequestSelected(ACR_ERequest.ORDER_UNLOAD); return;
   case ACR_EPanelAction.REINFORCE: pc.ACR_RequestSelected(ACR_ERequest.REINFORCE); return;
   case ACR_EPanelAction.FOB_SUPPLY: pc.ACR_SendFob(pc.m_ACR_SelectedBase, false); return;
   case ACR_EPanelAction.FOB_BUILD: pc.ACR_SendFob(pc.m_ACR_SelectedBase, true); return;
   case ACR_EPanelAction.FOB_AI: pc.ACR_SendFobAI(pc.m_ACR_SelectedBase); return;
   case ACR_EPanelAction.SQUAD_AI: pc.ACR_RequestSelected(ACR_ERequest.ORDER_SQUAD_AI); return;
   case ACR_EPanelAction.ESTABLISH_FOB:
    if (pc.m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB) pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else pc.m_ACR_CursorMode = ACR_ECursorMode.ESTABLISH_FOB;
    return;
   case ACR_EPanelAction.SETUP_HIDEOUT: ACR_ToggleCursor(pc, ACR_ECursorMode.SETUP_HIDEOUT); return;
   case ACR_EPanelAction.GO_TO_GROUND: pc.ACR_SendGoToGround(pc.m_ACR_SelectedBase); return;
   case ACR_EPanelAction.TOGGLE_MUSTER: pc.m_ACR_MusterMode = !pc.m_ACR_MusterMode; return;
   case ACR_EPanelAction.DISPERSE:
    if (pc.m_ACR_SelectedGroup.IsValid()) pc.ACR_SendDisperse(pc.m_ACR_SelectedGroup);
    return;
   case ACR_EPanelAction.DRAW_IED_KIT: pc.ACR_SendDrawIedKit(); return;
   case ACR_EPanelAction.TOGGLE_DISGUISE: pc.ACR_SendToggleDisguise(); return;
   case ACR_EPanelAction.TOGGLE_NETWORK:
    pc.m_ACR_NetworkOpen = !pc.m_ACR_NetworkOpen;
    if (pc.m_ACR_NetworkOpen) pc.m_ACR_RequisitionOpen = false;
    pc.m_ACR_PendingEntry = -1;
    if (!pc.m_ACR_NetworkOpen) pc.m_ACR_PendingNetwork = -1;
    return;
   case ACR_EPanelAction.NET_CELL:
    if (pc.m_ACR_PendingNetwork == ACR_InsurgentNetwork.NET_CELL) pc.m_ACR_PendingNetwork = -1;
    else pc.m_ACR_PendingNetwork = ACR_InsurgentNetwork.NET_CELL;
    return;
   case ACR_EPanelAction.NET_IED:
    if (pc.m_ACR_PendingNetwork == ACR_InsurgentNetwork.NET_IED) pc.m_ACR_PendingNetwork = -1;
    else pc.m_ACR_PendingNetwork = ACR_InsurgentNetwork.NET_IED;
    return;
   case ACR_EPanelAction.NET_SABOTAGE:
    if (pc.m_ACR_PendingNetwork == ACR_InsurgentNetwork.NET_SABOTAGE) pc.m_ACR_PendingNetwork = -1;
    else pc.m_ACR_PendingNetwork = ACR_InsurgentNetwork.NET_SABOTAGE;
    return;
   case ACR_EPanelAction.NET_TECH_RAID:
    if (pc.m_ACR_PendingNetwork == ACR_InsurgentNetwork.NET_TECH_RAID) pc.m_ACR_PendingNetwork = -1;
    else pc.m_ACR_PendingNetwork = ACR_InsurgentNetwork.NET_TECH_RAID;
    return;
   case ACR_EPanelAction.ROADBLOCK: ACR_ToggleCursor(pc, ACR_ECursorMode.ROADBLOCK); return;
   case ACR_EPanelAction.ROADBLOCK_S: ACR_ToggleCursor(pc, ACR_ECursorMode.ROADBLOCK_S); return;
   case ACR_EPanelAction.ROADBLOCK_L: ACR_ToggleCursor(pc, ACR_ECursorMode.ROADBLOCK_L); return;
   case ACR_EPanelAction.BARRICADE: ACR_ToggleCursor(pc, ACR_ECursorMode.BARRICADE); return;
   case ACR_EPanelAction.QRF: ACR_ToggleCursor(pc, ACR_ECursorMode.QRF); return;
   case ACR_EPanelAction.EVAC: ACR_ToggleCursor(pc, ACR_ECursorMode.EVAC); return;
   case ACR_EPanelAction.TWOIC_EVAC: pc.ACR_Request(ACR_ERequest.TOGGLE_2IC_EVAC); return;
   case ACR_EPanelAction.TRUCE_OFFER: pc.ACR_Request(ACR_ERequest.TRUCE_OFFER); return;
   case ACR_EPanelAction.TRUCE_REFUSE: pc.ACR_Request(ACR_ERequest.TRUCE_REFUSE); return;
   case ACR_EPanelAction.MINES_AP: ACR_ToggleCursor(pc, ACR_ECursorMode.MINES_AP); return;
   case ACR_EPanelAction.MINES_AT: ACR_ToggleCursor(pc, ACR_ECursorMode.MINES_AT); return;
   case ACR_EPanelAction.MINES_MIXED: ACR_ToggleCursor(pc, ACR_ECursorMode.MINES_MIXED); return;
   case ACR_EPanelAction.DISARM_MINES: ACR_ToggleCursor(pc, ACR_ECursorMode.DISARM_MINES); return;
   case ACR_EPanelAction.FORTIFY: ACR_ToggleCursor(pc, ACR_ECursorMode.FORTIFY); return;
   case ACR_EPanelAction.BUILD_FOLD:
    if (pc.m_ACR_Panel) pc.m_ACR_Panel.ToggleBuildFold();
    return;
   case ACR_EPanelAction.UPGRADES_FOLD:
    if (pc.m_ACR_Panel) pc.m_ACR_Panel.ToggleUpgradesFold();
    return;
   case ACR_EPanelAction.MORE_FOLD:
    if (pc.m_ACR_Panel) pc.m_ACR_Panel.ToggleMoreFold();
    return;
   case ACR_EPanelAction.KEY_HINTS:
    ACR_KeyHints.Toggle();
    ACR_RefreshKeyHintsButton(pc);
    return;
   case ACR_EPanelAction.RADIO_AUDIO:
    ACR_RadioAudio.Toggle();
    if (pc.m_ACR_Panel) pc.m_ACR_Panel.RefreshRadioAudio();
    return;
   case ACR_EPanelAction.HALT: pc.ACR_RequestSelected(ACR_ERequest.ORDER_HALT); return;
   case ACR_EPanelAction.FIRE_AT_WILL: pc.ACR_RequestSelected(ACR_ERequest.ORDER_FIRE_AT_WILL); return;
   case ACR_EPanelAction.PATROL: pc.ACR_RequestSelected(ACR_ERequest.ORDER_PATROL); return;
   case ACR_EPanelAction.PING_LOOK: pc.ACR_PickPing(ACR_EPing.LOOK); return;
   case ACR_EPanelAction.PING_ATTACK: pc.ACR_PickPing(ACR_EPing.ATTACK); return;
   case ACR_EPanelAction.PING_DEFEND: pc.ACR_PickPing(ACR_EPing.DEFEND); return;
   case ACR_EPanelAction.PING_SUPPORT: pc.ACR_PickPing(ACR_EPing.SUPPORT); return;
   case ACR_EPanelAction.DEBUG_TOGGLE: pc.m_ACR_DebugOpen = !pc.m_ACR_DebugOpen; return;
   case ACR_EPanelAction.DEBUG_THINK: ACR_Think.Cycle(); return;
   case ACR_EPanelAction.TOGGLE_REQUISITION:
    pc.m_ACR_RequisitionOpen = !pc.m_ACR_RequisitionOpen;
    if (pc.m_ACR_RequisitionOpen) pc.m_ACR_SupportOpen = false;
    pc.m_ACR_PendingEntry = -1;
    pc.m_ACR_PendingSupport = -1;
    return;
   case ACR_EPanelAction.TOGGLE_REPORT: pc.m_ACR_ReportOpen = !pc.m_ACR_ReportOpen; return;
   case ACR_EPanelAction.TOGGLE_SUPPORTS:
    pc.m_ACR_SupportOpen = !pc.m_ACR_SupportOpen;
    pc.m_ACR_HasSupportTarget = false;
    if (pc.m_ACR_SupportOpen) pc.m_ACR_RequisitionOpen = false;
    pc.m_ACR_PendingEntry = -1;
    pc.m_ACR_PendingSupport = -1;
    return;
  }
  if (action >= ACR_EPanelAction.DEBUG_ACTION && action < ACR_EPanelAction.REQUISITION_ENTRY)
  {
   pc.ACR_RequestDebug(action - ACR_EPanelAction.DEBUG_ACTION);
   return;
  }
  if (action >= ACR_EPanelAction.FOB_UPGRADE && action < ACR_EPanelAction.FOB_UPGRADE + ACR_FobSlots.UPGRADE_COUNT)
  {
   int slot = action - ACR_EPanelAction.FOB_UPGRADE;
   pc.ACR_RequestFobUpgrade(pc.m_ACR_SelectedBase, slot);
   return;
  }
  if (action >= ACR_EPanelAction.SUPPORT_ENTRY)
  {
   int support = action - ACR_EPanelAction.SUPPORT_ENTRY;
   // Aimed from a Support ping: fire at it straight away.
   if (pc.m_ACR_HasSupportTarget)
   {
    pc.m_ACR_HasSupportTarget = false;
    pc.ACR_SendSupport(support, pc.m_ACR_SupportTarget, pc.m_ACR_SelectedGroup);
    pc.m_ACR_PendingSupport = -1;
    pc.m_ACR_SupportOpen = false;
    return;
   }
   if (pc.m_ACR_PendingSupport == support) pc.m_ACR_PendingSupport = -1;
   else pc.m_ACR_PendingSupport = support;
   pc.m_ACR_PendingEntry = -1;
   return;
  }
  if (action >= ACR_EPanelAction.REQUISITION_ENTRY)
  {
   int entry = action - ACR_EPanelAction.REQUISITION_ENTRY;
   if (pc.m_ACR_PendingEntry == entry) pc.m_ACR_PendingEntry = -1; // click again to cancel
   else pc.m_ACR_PendingEntry = entry;
  }
 }

 // Called by the map radial menu hook. Returns true if the right-click was used.
 // Game Master or a construction truck's build mode is open: its map clicks are its own
 // (in build mode they used to reach a possessed vehicle's squad as move orders).
 static bool ACR_EditorOpen()
 {
  SCR_EditorManagerEntity editor = SCR_EditorManagerEntity.GetInstance();
  return editor && editor.IsOpened();
 }

 // An order that takes a facing is armed for a squad we command: a right-drag aims it.
 static bool ACR_AimArmed(SCR_PlayerController pc)
 {
  return pc.m_ACR_IsCommander && pc.m_ACR_SelectedGroup.IsValid() && ACR_Aimable(pc) && !ACR_EditorOpen();
 }

 static bool ACR_Aimable(SCR_PlayerController pc)
 {
  if (ACR_CommandPanel.RoadblockOfMode(pc.m_ACR_CursorMode) >= 0 || pc.m_ACR_CursorMode == ACR_ECursorMode.FORTIFY || pc.m_ACR_CursorMode == ACR_ECursorMode.DEFEND) return true;
  return pc.m_ACR_CursorMode >= ACR_ECursorMode.MINES_AP && pc.m_ACR_CursorMode <= ACR_ECursorMode.MINES_MIXED;
 }

 // The map's right button has been held for the hold time (MapPanDrag, SCR_MapRadialUI).
 static void ACR_AimDown(SCR_PlayerController pc)
 {
  pc.m_ACR_AimHeld = false;
  if (!ACR_AimArmed(pc) || pc.ACR_PointerOverOwnUI()) return; // a press on our boxes aims nothing
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return;
  float wx, wz;
  mapEntity.GetMapCursorWorldPosition(wx, wz);
  pc.m_ACR_AimFrom = Vector(wx, 0, wz);
  WidgetManager.GetMousePos(pc.m_ACR_AimFromX, pc.m_ACR_AimFromY);
  pc.m_ACR_AimAt = System.GetTickCount();
  pc.m_ACR_AimHeld = true;
 }

 // While it is down and has moved past the dead zone: where the facing points now.
 static bool ACR_AimDrag(SCR_PlayerController pc, out vector to)
 {
  if (!pc.m_ACR_AimHeld) return false;
  if (System.GetTickCount() - pc.m_ACR_AimAt > SCR_PlayerController.AIM_MAX_MS || !ACR_Aimable(pc))
  {
   pc.m_ACR_AimHeld = false;
   return false;
  }
  int x, y;
  WidgetManager.GetMousePos(x, y);
  if (Math.AbsInt(x - pc.m_ACR_AimFromX) <= SCR_PlayerController.AIM_DEAD_ZONE && Math.AbsInt(y - pc.m_ACR_AimFromY) <= SCR_PlayerController.AIM_DEAD_ZONE) return false;
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return false;
  float wx, wz;
  mapEntity.GetMapCursorWorldPosition(wx, wz);
  to = Vector(wx, 0, wz);
  return true;
 }

 // ...and came up: a drag orders with its facing, a long press without one is a plain
 // right-click there.
 static void ACR_AimUp(SCR_PlayerController pc)
 {
  if (!pc.m_ACR_AimHeld) return;
  vector to;
  bool dragged = ACR_AimDrag(pc, to);
  pc.m_ACR_AimHeld = false;
  pc.m_ACR_AimReleased = dragged;
  pc.m_ACR_AimRelease = to;
  if (ACR_HandleMapRightClick(pc)) pc.m_ACR_ClickGate.ReleaseOrdered(System.GetTickCount());
  pc.m_ACR_AimReleased = false;
 }

 // The map's right-click (MapContextualMenu, SCR_MapRadialUI): an order, unless it is the press
 // a release has just ordered for (ACR_ClickGate).
 static bool ACR_HandleMapClick(SCR_PlayerController pc)
 {
  if (pc.m_ACR_ClickGate.SwallowClick(System.GetTickCount())) return true;
  return ACR_HandleMapRightClick(pc);
 }

 static bool ACR_HandleMapRightClick(SCR_PlayerController pc)
 {
  // A drag to aim ends in ACR_AimUp, which passes its facing here; a click has none.
  vector aimTo = pc.m_ACR_AimRelease;
  bool aimed = pc.m_ACR_AimReleased;
  pc.m_ACR_AimReleased = false;
  pc.m_ACR_AimHeld = false;
  // A right-click on our boxes orders nothing and opens no radial menu (a drag to aim that
  // started on the map may end over one: it still orders).
  if (!aimed && pc.ACR_PointerOverOwnUI()) return true;
  if (!ACR_Manager.GetInstance()) return false;
  if (ACR_EditorOpen()) return false;
  if (pc.ACR_ModifierHeld("ACR_MapAlt"))
  {
   SCR_MapEntity pickerMap = SCR_MapEntity.GetMapInstance();
   if (!pickerMap) return false;
   float cx, cz;
   pickerMap.GetMapCursorWorldPosition(cx, cz);
   pc.ACR_OpenPickerPublic(pickerMap, Vector(cx, 0, cz));
   return true;
  }
  pc.ACR_ClosePickerPublic();
  if (pc.m_ACR_PendingSupport >= 0)
  {
   SCR_MapEntity supportMap = SCR_MapEntity.GetMapInstance();
   if (!supportMap) return false;
   float sx, sz;
   supportMap.GetMapCursorWorldPosition(sx, sz);
   pc.ACR_SendSupport(pc.m_ACR_PendingSupport, Vector(sx, 0, sz), pc.m_ACR_SelectedGroup);
   pc.m_ACR_PendingSupport = -1;
   return true;
  }
  if (pc.m_ACR_PendingEntry >= 0)
  {
   pc.m_ACR_PendingEntry = -1;
   return true;
  }
  if (pc.m_ACR_PendingNetwork >= 0)
  {
   SCR_MapEntity netMap = SCR_MapEntity.GetMapInstance();
   if (!netMap) return false;
   float nx, nz;
   netMap.GetMapCursorWorldPosition(nx, nz);
   pc.ACR_SendNetworkOrder(pc.m_ACR_PendingNetwork, Vector(nx, 0, nz));
   pc.m_ACR_PendingNetwork = -1;
   return true;
  }
  if (!pc.m_ACR_SelectedGroup.IsValid()) return false;
  int index = pc.m_ACR_Data.m_GroupIds.Find(pc.m_ACR_SelectedGroup);
  if (index < 0 || index >= pc.m_ACR_Data.m_Commandable.Count() || pc.m_ACR_Data.m_Commandable[index] != 1) return false;
  if (!pc.m_ACR_IsCommander)
  {
   SCR_HintManagerComponent.ShowCustomHint("Only the commander can give map orders. Take command in the panel.", "AC: Actual", 4, true);
   return true;
  }
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return false;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  bool append = pc.ACR_ModifierHeld("ACR_MapShift");
  int command = ACR_ERequest.MOVE_TO;
  if (append) command = ACR_ERequest.QUEUE_MOVE;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.DEFEND)
  {
   command = ACR_ERequest.ORDER_DEFEND;
   if (append) command = ACR_ERequest.QUEUE_DEFEND;
  }
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.GARRISON)
  {
   command = ACR_ERequest.ORDER_GARRISON;
   if (append) command = ACR_ERequest.QUEUE_GARRISON;
  }
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.GET_IN) command = ACR_ERequest.ORDER_GET_IN;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.LZ) command = ACR_ERequest.ORDER_SET_LZ;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.RIDE) command = ACR_ERequest.ORDER_RIDE;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB || pc.m_ACR_CursorMode == ACR_ECursorMode.SETUP_HIDEOUT) command = ACR_ERequest.ORDER_ESTABLISH_FOB;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.ROADBLOCK) command = ACR_ERequest.ORDER_ROADBLOCK;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.QRF) command = ACR_ERequest.ORDER_QRF;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.EVAC) command = ACR_ERequest.ORDER_EVAC;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.MINES_AP) command = ACR_ERequest.ORDER_MINES_AP;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.MINES_AT) command = ACR_ERequest.ORDER_MINES_AT;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.MINES_MIXED) command = ACR_ERequest.ORDER_MINES_MIXED;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.DISARM_MINES) command = ACR_ERequest.ORDER_DISARM_MINES;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.ROADBLOCK_S) command = ACR_ERequest.ORDER_ROADBLOCK_S;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.ROADBLOCK_L) command = ACR_ERequest.ORDER_ROADBLOCK_L;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.BARRICADE) command = ACR_ERequest.ORDER_BARRICADE;
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.FORTIFY) command = ACR_ERequest.ORDER_FORTIFY;
  // Minefield points (#200): a right-click drops a point and the last one clicked again lays the field (one
  // point the square as before, two a line, three or four that area); a fourth lays it at once. A drag on
  // the first point still lays the square facing the drag.
  ACR_MineCornersCheck(pc);
  if (ACR_MineShapeMode(pc) && !append && !(aimed && pc.m_ACR_MineCorners.IsEmpty())) return ACR_MineShapeClick(pc, mapEntity, command, Vector(x, 0, z));
  vector target = Vector(x, 0, z);
  // Dragged: the order goes where the button went down, facing where it came up.
  vector aim;
  if (aimed && ACR_Aimable(pc))
  {
   aim = aimTo;
   target = pc.m_ACR_AimFrom;
  }
  // Contextual right-click in MOVE mode:
  //  - on a spotted enemy: hunt it down
  //  - on a friendly empty vehicle: get in
  //  - on an owned base icon: defend it
  if (pc.m_ACR_CursorMode == ACR_ECursorMode.MOVE && !append && !aimed)
  {
   int hunted = pc.ACR_ContactUnderCursor(mapEntity, x, z);
   if (hunted >= 0)
   {
    command = ACR_ERequest.ORDER_HUNT;
    target = pc.m_ACR_ContactPositions[hunted];
   }
   else
   {
    int veh = pc.ACR_VehicleUnderCursor(mapEntity, x, z);
    if (veh >= 0)
    {
     command = ACR_ERequest.ORDER_GET_IN;
     target = pc.m_ACR_VehiclePositions[veh];
    }
    else
    {
     int ownBase = pc.ACR_OwnBaseUnderCursor(mapEntity, x, z);
     if (ownBase >= 0)
     {
      command = ACR_ERequest.ORDER_DEFEND;
      target = pc.m_ACR_Data.m_BasePositions[ownBase];
     }
    }
   }
  }
  pc.ACR_IssueOrder(command, target, append, aim);
  return true;
 }

 // An order button arms its right-click order; a second click disarms it.
 static void ACR_ToggleCursor(SCR_PlayerController pc, ACR_ECursorMode mode)
 {
  if (pc.m_ACR_CursorMode == mode) pc.m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  else pc.m_ACR_CursorMode = mode;
  pc.m_ACR_MineCorners.Clear();
 }

 // ---------------------------------------------------------------- minefield points (#200)
 static bool ACR_MineShapeMode(SCR_PlayerController pc)
 {
  return pc.m_ACR_CursorMode == ACR_ECursorMode.MINES_AP || pc.m_ACR_CursorMode == ACR_ECursorMode.MINES_AT || pc.m_ACR_CursorMode == ACR_ECursorMode.MINES_MIXED;
 }

 // The points belong to the squad and the order they were clicked for: another squad, another order or the
 // order disarmed drops them.
 static void ACR_MineCornersCheck(SCR_PlayerController pc)
 {
  if (pc.m_ACR_MineCorners.IsEmpty()) return;
  if (!ACR_MineShapeMode(pc) || pc.m_ACR_MineCornersFor != pc.m_ACR_SelectedGroup || pc.m_ACR_MineCornersMode != pc.m_ACR_CursorMode) pc.m_ACR_MineCorners.Clear();
 }

 // A right-click with a mine order armed: a point more, or (the last point clicked again, or a fourth point)
 // the field laid.
 static bool ACR_MineShapeClick(SCR_PlayerController pc, SCR_MapEntity mapEntity, int command, vector point)
 {
  ACR_MineCornersCheck(pc);
  if (!pc.m_ACR_MineCorners.IsEmpty())
  {
   vector last = pc.m_ACR_MineCorners[pc.m_ACR_MineCorners.Count() - 1];
   int lx, ly, px, py;
   mapEntity.WorldToScreen(last[0], last[2], lx, ly, true);
   mapEntity.WorldToScreen(point[0], point[2], px, py, true);
   if (Math.AbsInt(lx - px) <= SCR_PlayerController.MINE_SAME_PX && Math.AbsInt(ly - py) <= SCR_PlayerController.MINE_SAME_PX)
   {
    pc.ACR_LayMineShape(command);
    return true;
   }
  }
  pc.m_ACR_MineCorners.Insert(point);
  pc.m_ACR_MineCornersFor = pc.m_ACR_SelectedGroup;
  pc.m_ACR_MineCornersMode = pc.m_ACR_CursorMode;
  if (pc.m_ACR_MineCorners.Count() >= ACR_MineShape.MOST_CORNERS) pc.ACR_LayMineShape(command);
  return true;
 }
}
