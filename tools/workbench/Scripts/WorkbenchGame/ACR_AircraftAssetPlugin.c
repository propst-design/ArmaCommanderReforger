// Headless Workbench plugin for the aircraft blockouts (tools/blender/build_aircraft.py), run by
// tools/Build-AircraftAssets.ps1 (not shipped: this is the tools project, not the mod's Scripts). Steps, from
// $profile:aircraft_job.txt:
//  EMAT <path>|<base>|<key>=<value>...: sets up the .png textures it names (vanilla import settings for the
//  suffix) and registers the material (the runner writes its text with their resource names);
//  FBX <path>: registers and builds the model (static mesh: no skinning or hierarchy).
// The prefabs (Prefabs/Vehicles/Aircraft) have hand-written metas, which the game reads but this tools project's
// resource cache doesn't: srv:aircraft checks them in a game.
// Outcomes go to $profile:aircraft_result.txt.
[WorkbenchPluginAttribute(name: "ACR Aircraft Assets", wbModules: {"ResourceManager"})]
class ACR_AircraftAssetPlugin : WorkbenchPlugin
{
	ref FileHandle m_Result;

	void Log(string line)
	{
		m_Result.WriteLine(line);
		Print("[ACR aircraft] " + line);
	}

	MetaFile Meta(string path, bool build)
	{
		ResourceManager rm = Workbench.GetModule(ResourceManager);
		MetaFile meta = rm.GetMetaFile(path);
		if (!meta && rm.RegisterResourceFile(path, build))
			meta = rm.GetMetaFile(path);
		return meta;
	}

	string Texture(string path)
	{
		MetaFile meta = Meta(path, false);
		if (!meta)
		{
			Log("TEXTURE FAIL register " + path);
			return "";
		}
		TextureTypes types = new TextureTypes();
		types.CheckAncestors(TextureIssueOp.Fix, path, meta, types.FindTextureType(path));
		meta.Save();
		ResourceManager rm = Workbench.GetModule(ResourceManager);
		rm.RebuildResourceFile(path, "PC", false);
		Log("TEXTURE OK " + meta.GetResourceID());
		return meta.GetResourceID();
	}

	void Emat(string spec)
	{
		array<string> f = {};
		spec.Split("|", f, false);
		for (int i = 2; i < f.Count(); i++)
		{
			int eq = f[i].IndexOf("=");
			string val = f[i].Substring(eq + 1, f[i].Length() - eq - 1);
			if (val.EndsWith(".png"))
				Texture(val);
		}
		MetaFile meta = Meta(f[0], false);
		if (meta)
			Log("EMAT OK " + meta.GetResourceID());
		else
			Log("EMAT FAIL register " + f[0]);
	}

	void ImportFbx(string path)
	{
		MetaFile meta = Meta(path, false);
		if (!meta)
		{
			Log("FBX FAIL register " + path);
			return;
		}
		ResourceManager rm = Workbench.GetModule(ResourceManager);
		rm.RebuildResourceFile(path, "PC", false);
		Log("FBX OK " + meta.GetResourceID());
	}


	override void RunCommandline()
	{
		thread Work(); // a thread can Sleep, which lets Workbench run the builds it queued
	}

	void Work()
	{
		m_Result = FileIO.OpenFile("$profile:aircraft_result.txt", FileMode.WRITE);
		array<string> models = {};
		string line;
		FileHandle job = FileIO.OpenFile("$profile:aircraft_job.txt", FileMode.READ);
		while (job && job.ReadLine(line) >= 0)
		{
			int sp = line.IndexOf(" ");
			if (sp < 0)
				continue;
			string cmd = line.Substring(0, sp);
			string arg = line.Substring(sp + 1, line.Length() - sp - 1);
			if (cmd == "EMAT")
				Emat(arg); // before the models, which link their material by name
			else if (cmd == "FBX")
				models.Insert(arg);
		}
		if (job)
			job.Close();
		else
			Log("FAIL no $profile:aircraft_job.txt");
		foreach (string fbx : models)
			ImportFbx(fbx);
		// the rebuilds run in the background (a few seconds each; FileIO can't see the addon's absolute paths)
		int waited = 20;
		Sleep(waited * 1000);
		Log(string.Format("DONE after %1 s", waited));
		m_Result.Close();
		Workbench.Exit(0);
	}
}
