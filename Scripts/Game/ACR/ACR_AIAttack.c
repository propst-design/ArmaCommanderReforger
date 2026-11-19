// Records the AI commander (ACR_AICommander) keeps: base attacks, build jobs ridden out to, and trips.

// One base attack: groups gather at a staging point, then assault together.
class ACR_AIAttack
{
 ACR_Base m_Target;
 ref array<ACR_Squad> m_Squads = {};
 ref array<vector> m_aStaging = {};  // one staging point per squad (pincers)
 ref array<int> m_aSlot = {};        // each squad's approach line (ACR_AICommander.s_aApproachTurns index, -1 none)
 vector m_Staging;                   // main approach, for smoke
 vector m_vApproach;                 // main approach direction, from the target towards the attackers
 bool m_bAssaulting;
 bool m_bGivenAway;                  // its last squad went to a player (Forget) or another job (LeaveAttacks): ends without a "failed" report
 float m_fStageDeadline;
 ACR_Squad m_Suppressor;             // support-by-fire squad
 float m_fFireBaseMoveSince;         // world time the base of fire started moving up to a firing spot
 vector m_vOverwatch;                // where the base of fire holds while no enemy is seen at the objective
 bool m_bOverwatch;
 float m_fStageLimit;                // latest the forming up may be stretched to for late joiners
 int m_iStartStrength;
 float m_fLaunchedAt;                // world time it was launched (the allocator's grace period)
 bool m_bCounter;
 bool m_bGrab;                       // quick grab of an empty base (doesn't count against the attack limit)
 float m_fAssaultStart;              // world time the assault (or grab) began
 float m_fNextPush;                  // next time idle attackers are sent in again
 float m_fGrabSeconds;               // a grab's own time limit: the trip there plus slack (big maps)
 float m_fTravelSeconds;             // the slowest squad's way there on foot or by ride (#27): the forming up waits for it
 bool m_bFobRaid;                    // demolition raid against an enemy forward operating base
 ref ACR_SapperDemo m_SapperDemo;
 // #101: time BBB's squad tactics spent fighting for it (gaining ground, or holding outnumbered or
 // flanked) doesn't count as stalling; and when it last called support for a squad that asked.
 float m_fBBBFightS;
 float m_fBBBCheckAt;
 float m_fBBBSupportAt;
 // The ground picture (-acrAttackGround, ACR_AttackGround): built at launch on a held base,
 // rebuilt when an enemy is seen where no eye was; and the build last logged.
 ref ACR_AttackGround m_Ground;
 int m_iGroundLogged;

 vector StagingOf(ACR_Squad squad)
 {
  int i = m_Squads.Find(squad);
  if (i >= 0 && i < m_aStaging.Count()) return m_aStaging[i];
  return m_Staging;
 }
 void Remove(int index)
 {
  if (m_Suppressor == m_Squads[index]) m_Suppressor = null;
  m_Squads.RemoveOrdered(index);
  if (index < m_aStaging.Count()) m_aStaging.RemoveOrdered(index);
  if (index < m_aSlot.Count()) m_aSlot.RemoveOrdered(index);
 }
 void Add(ACR_Squad squad, vector stage, int slot)
 {
  m_Squads.Insert(squad);
  m_aStaging.Insert(stage);
  m_aSlot.Insert(slot);
 }
}

// A build job a far squad rides out to (ACR_AICommander.RideToBuild): given, and paid, once it is
// out at the drop.
class ACR_PendingBuild
{
 ACR_Squad m_Squad;
 int m_iTask;        // ACR_EAITask.ROADBLOCK, MINES or FORTIFY
 ACR_Base m_Base;    // the front base it is for
 vector m_vEnemy;    // that base's enemy base
 vector m_vSite;     // a roadblock's road site (m_fYaw its facing); else where the ride goes (the base)
 float m_fYaw;
 float m_fDeadline;  // world ms
 bool m_bWorks;      // FORTIFY: a whole base's works (ACR_BaseWorks), not a position
}

// A trip Travel sent a squad on (ACR_AICommander.NoteTrip): where to and what for, and the order it
// carries out there, so a ride can still be caught on the way (CatchRides).
class ACR_AITrip
{
 vector m_vTo;
 ACR_Base m_Base;      // the own base it is going to, if any (lost: the trip is off)
 ResourceName m_sPrefab;
 float m_fRadius;
 string m_sOrder;
 int m_iKind;
 float m_fDropShort;
 float m_fUntil;       // world ms: past this the trip no longer holds the squad
 float m_fNextRide;    // world ms: next look for a ride on the way
 int m_iRideAsks;      // refused asks so far (each doubles the wait for the next)
}
