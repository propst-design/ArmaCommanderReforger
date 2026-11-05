// Client: the latest command-map picture from the server (see ACR_MapFeed),
// shared by the player controller (markers, panel, input) and the displays
// that draw on the map and HUD (ACR_OrderDisplay, ACR_PingDisplay).
class ACR_MapData : Managed
{
 int m_iMyId;   // the local player's id
 // squad leaders (group markers)
 ref array<RplId> m_LeaderIds = {};
 ref array<vector> m_LeaderPositions = {};
 ref array<RplId> m_GroupIds = {};
 ref array<int> m_Commandable = {};
 ref array<string> m_GroupNames = {};
 // status bars: 6 ints per group (hp, ammo, fuel, stock, ordered by, order age s)
 ref array<RplId> m_StatusGroups = {};
 ref array<int> m_Status = {};
 int m_iStatusAt;
 // order lines
 ref array<RplId> m_OrderGroups = {};
 ref array<vector> m_OrderTargets = {};
 ref array<int> m_OrderKinds = {};
 ref array<int> m_OrderIssuers = {};
 // bases and support danger zones
 ref array<vector> m_BasePositions = {};
 ref array<string> m_BaseNames = {};
 ref array<string> m_BaseOwners = {};
 ref array<string> m_BaseDetails = {};
 ref array<float> m_BaseRadii = {};
 ref array<int> m_BaseContested = {};
 ref array<int> m_BaseSupply = {};
 ref array<vector> m_ZonePositions = {};
 bool m_bAiming;     // drag to aim in progress: an arrow from m_vAimFrom to m_vAimTo
 vector m_vAimFrom;
 vector m_vAimTo;
 ref array<float> m_ZoneRadii = {};
 ref array<string> m_ZoneLabels = {};
 // own selection, and the faint lines of orders just clicked
 ref array<RplId> m_Selected = {};
 ref array<vector> m_PendingFrom = {};
 ref array<vector> m_PendingTo = {};
 // the commander's minefield points so far (#200) and the cursor, for the preview
 ref array<vector> m_MineCorners = {};
 vector m_vMineCursor;
 // the side's shaped minefields: each one's corners in turn, how many per field
 ref array<vector> m_MineShapeCorners = {};
 ref array<int> m_MineShapeCounts = {};
 ref array<int> m_PendingUntil = {};
 // spotted enemies seen moving: where, heading (degrees, 0 = north) and fade
 ref array<vector> m_ContactArrowPos = {};
 ref array<int> m_ContactArrowHeading = {};
 ref array<float> m_ContactArrowAlpha = {};
 // shared command
 ref array<int> m_CmdIds = {};
 ref array<string> m_CmdTags = {};
 ref array<string> m_CmdActions = {};
 ref array<int> m_CmdActionAges = {};
 int m_iCmdAt;
 ref array<RplId> m_SelGroups = {};
 ref array<int> m_SelOwners = {};
 // pings
 ref array<int> m_PingIds = {};
 ref array<int> m_PingKinds = {};
 ref array<vector> m_PingPositions = {};
 ref array<int> m_PingOwners = {};
 ref array<string> m_PingTags = {};
 ref array<int> m_PingAges = {};
 ref array<string> m_PingAnswers = {};
 int m_iPingsAt;

 // Seconds since a packet received at this tick count.
 static int Since(int tick)
 {
  return (System.GetTickCount() - tick) / 1000;
 }

 bool IsSelected(RplId groupId)
 {
  return groupId.IsValid() && m_Selected.Contains(groupId);
 }

 vector LeaderPos(int index)
 {
  // Server snapshots include the vehicle origin for mounted command units.
  // Do not replace that position with a client-side streamed character entity.
  return m_LeaderPositions[index];
 }

 bool IsCommanderId(int playerId)
 {
  return m_CmdIds.Contains(playerId);
 }

 bool AmCommander()
 {
  return m_CmdIds.Contains(m_iMyId);
 }

 // A commander's short name ("" for nobody, the AI or someone unknown).
 string TagOf(int playerId)
 {
  if (playerId <= 0) return "";
  int i = m_CmdIds.Find(playerId);
  if (i < 0) return "";
  return m_CmdTags[i];
 }

 // Who gave a squad's current order and how long ago (seconds, -1 unknown).
 int OrderedBy(RplId groupId, out int age)
 {
  age = -1;
  int i = m_StatusGroups.Find(groupId);
  if (i < 0 || i * 6 + 5 >= m_Status.Count()) return 0;
  int by = m_Status[i * 6 + 4];
  age = m_Status[i * 6 + 5];
  if (age >= 0) age += Since(m_iStatusAt);
  return by;
 }

 // Another commander ordered this squad less than 'seconds' ago: his tag, else "".
 string RecentOtherOrder(RplId groupId, int seconds)
 {
  int age;
  int by = OrderedBy(groupId, age);
  if (by <= 0 || by == m_iMyId || age < 0 || age >= seconds) return "";
  string tag = TagOf(by);
  if (tag.IsEmpty()) return "";
  return tag;
 }

 int Index(RplId groupId)
 {
  return m_GroupIds.Find(groupId);
 }
}
