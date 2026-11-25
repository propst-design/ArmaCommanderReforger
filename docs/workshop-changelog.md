0.7.0

New match setup lobby with era skins and presets, FIA spoils and town grievance, experimental truck supply runs, period jets and 2IC promoted to default, reworked jet flyover sounds, and AI/UI fixes. Needs Better Battle Buddies 0.3.0.

NEW

- Match setup lobby and era skins: replaces the old button dialog with an operations-order lobby, live layout sketch with highlighted HQs, focus-driven help and tooltips, Space or Y to start, and era skins (command blue, Cold War green terminal, modern tablet via -acrSetupSkin; classic screen still available with -acrSetupClassic 1).
- Setup presets: save, load, rename, delete, copy/paste, and auto-load favourite match setups from the Presets tab, or load one on a dedicated server with -acrPreset.
- Insurgency, FIA spoils (-acrFIASpoils): FIA loots weapons from the fallen to arm recruits, steals unattended vehicles for raids, and takes knocked-out or surrendered soldiers and players prisoner at hideouts (free prisoners for +3 RP each).
- Insurgency, civilian and town grievance (-acrFIAGrievance): civilian deaths, destroyed buildings, and shelling or airstrikes in towns speed up nearby FIA hideout recruitment and cost standing when leaning is on.
- Physical logistics, part 1 (Experimental, off by default): truck-driven FOB supply runs (-acrSupplyRuns 1, +25% supplies when player-driven), automatic recorded routes (-acrSaveRoutes 1), and logistics hubs (-acrLogisticsHubs 1).
- Promoted out of Experimental (now on by default): period jets (with night nozzle glow and recon-dash afterburners), fields of fire (guns clear bushes), pull over before getting out, and the second-in-command (plain 2IC).

FIXED AND IMPROVED

- Period jet flyover and engine sounds: reworked A-10A, RF-4C, Su-17M4R and Su-25 audio at full quality, removing close-in turbojet fluttering, softening overhead ground-bounce phasing, fixing approach air absorption, and giving the A-10 its high-bypass TF34 fan whistle and duct moan.
- AI commander: flanks around strongly held bases, grabs nearby income bases when short of RP, buys cheap unarmed jeeps for small teams, stops pulling squads off committed assaults over forecasts, and tracks contested AI-handed FOBs on the map.
- Vehicles and crews: pulling over no longer tips trucks or APCs over (needs BBB 0.3.0), AI vehicles no longer stop at every waypoint, stuck-circling vehicles are caught, armed crews stay aboard in fights, and FIA technicals only raid road-reachable targets within 2.5 km.
- Garrisons and night fights: garrisons stay out while downed enemies or enemy players are nearby, and AI squads fire night illumination flares in Fast mode again.
- UI and launchers: deck builder and command panels no longer click-through or scroll-through to the map, setup lobby spinboxes and tooltips fit cleanly, deck budgets are 125 / 200 / 300, and launchers warn when an expansion pack is missing Workshop mods.
