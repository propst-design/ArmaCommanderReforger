# Arma Commander Actual - Civil Population compat

Optional add-on that plugs Shadow Company Civil Population (BCE0D49568A9E6BF) into
Arma Commander Actual (AC230926A0010001). Load it with both mods; each mod runs without it.

- The civilians' AI agents get their own room in ACA's active AI limit, so they never push
  ACA squads over the engine limit.
- ACA's scripted bomb runs, battery rounds and cook-offs scare civilians like real explosions.
- A civilian killed by a side's fire shows in that side's command feed ("Civilian killed by our
  fire near <base>"). The other side isn't told.
- Each side's map shows roughly where civilians are around its soldiers, players and bases: a violet
  "Civilians seen: a few / some / many" ring over that part of town, never a pin.

## Trying it (experimental)

ACA has the socket this add-on plugs into from 0.5.9; without this add-on it does nothing.
The civilian rings need ACA 0.5.13 or later: this version of the add-on won't load with an older ACA.

1. Copy this `ACACivilPopulation` folder into your Workbench addons folder, beside `ArmaCommanderReforger`
   (the launchers look for it there; ACA's own release builds leave `compat/` out).
2. Subscribe to Civil Population on the Workshop (or have it in `E:\ArmaReforgerMods\addons`).
3. Play-ACA / Host-ACA / Join-ACA now show a "Civilians" row; set it to yes. Everyone in an MP game needs the same.

Known gaps: kills by ACA bomb runs and artillery aren't reported (those blasts have no instigator).
