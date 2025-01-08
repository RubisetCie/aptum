// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-save.h>
#include <apt-private/private-install.h>
#include <apt-private/private-output.h>

#include <sys/stat.h>

#include <string>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <zlib.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

/* Returns the modification time of a file path */
static time_t GetFileTime(const char *path)
{
   struct stat attr;
   if (unlikely(stat(path, &attr) == -1))
	   return 0;
   return attr.st_mtime;
}

// GetState - Get the current package state					/*{{{*/
// ---------------------------------------------------------------------
/* Get the list of currently installed packages */
struct PackageEntry
{
   string Name;
   string Arch;
   inline size_t Len() const { return Name.length() + Arch.length() + 2; };
   inline bool operator==(const PackageEntry &O) const { return Name == O.Name && Arch == O.Arch; };
};
static vector<PackageEntry> GetState()
{
   vector<PackageEntry> Packages;
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return Packages;
   for (pkgCache::GrpIterator G = Cache->GrpBegin(); G.end() == false; ++G)
   {
      // find the proper version to use
      pkgCache::PkgIterator P = G.FindPreferredPkg();
      if (P.end() == true)
	 continue;

      // skip if non-installed
      if (P->CurrentVer == 0)
	 continue;

      Packages.emplace_back(PackageEntry{ P.Name(), P.Arch() });
   }
   return Packages;
}
									/*}}}*/

// LoadState - Load a package state							/*{{{*/
// ---------------------------------------------------------------------
/* Load a package state from a saved file */
static vector<PackageEntry> LoadState(string_view Filename)
{
   vector<PackageEntry> Packages;

   FILE *File = fopen(Filename.data(), "rb");
   if (File == NULL)
      return Packages;

   unsigned char *InBuffer = NULL;
   unsigned char *OutBuffer = NULL;
   unsigned long InBufferLen, OutBufferLen;

   // read the initial data length
   if (fread(&InBufferLen, sizeof(size_t), 1, File) != 1)
      goto END;

   // tell the length of the compressed data
   {
      const long s = ftell(File);
      fseek(File, 0, SEEK_END);
      OutBufferLen = ftell(File) - s;
      fseek(File, s, SEEK_SET);
   }

   // allocate the buffers
   if (!(InBuffer  = (unsigned char*)malloc(InBufferLen)) ||
       !(OutBuffer = (unsigned char*)malloc(OutBufferLen)))
      goto END;

   // read the compressed data
   if (fread(OutBuffer, OutBufferLen, 1, File) != 1)
      goto END;

   // uncompress the buffer using ZLib
   if (uncompress2(InBuffer, &InBufferLen, OutBuffer, &OutBufferLen) != Z_OK)
      goto END;

   // free early the compressed buffer, as it's no longer needed
   free(OutBuffer); OutBuffer = NULL;

   // decode the package list
   {
      size_t C = 0, P = 0, D = 0;
      while (C < InBufferLen)
      {
	 if (InBuffer[C] == 0)
	 {
	    if (D == 0)
	       D = ++C;
	    else
	    {
	       Packages.emplace_back(PackageEntry{ reinterpret_cast<char*>(&InBuffer[P]), reinterpret_cast<char*>(&InBuffer[D]) });
	       P = ++C; D = 0;
	    }
	 }
	 else
	    ++C;
      }
   }

END:
   // close the file
   free(InBuffer);
   free(OutBuffer);
   fclose(File);
   return Packages;
}
									/*}}}*/

// LoadState - Load a package state							/*{{{*/
// ---------------------------------------------------------------------
/* Load a package state from a saved file */
static bool PackageCompare(const PackageEntry &A, const PackageEntry &B)
{
   return A.Name < B.Name;
}
									/*}}}*/

// PackageForEachDelta - Compute each difference			/*{{{*/
// ---------------------------------------------------------------------
/* Callback for every addition / removals between two lists */
static void PackageForEachDelta(const vector<PackageEntry> &Saved, const vector<PackageEntry> &Current,
                                function<void(const PackageEntry *const)> Added, function<void(const PackageEntry *const)> Removed)
{
   vector<PackageEntry>::const_iterator SavedIt = Saved.begin();
   vector<PackageEntry>::const_iterator CurrentIt = Current.begin();
   do
   {
      if (*SavedIt == *CurrentIt)
	 ++SavedIt, ++CurrentIt;
      else
      {
	 if (!binary_search(Saved.begin(), Saved.end(), *CurrentIt, PackageCompare))
	 {
	    Added(CurrentIt.base());
	    ++CurrentIt;
	 }
	 else if (!binary_search(Current.begin(), Current.end(), *SavedIt, PackageCompare))
	 {
	    Removed(SavedIt.base());
	    ++SavedIt;
	 }
	 else
	    ++SavedIt, ++CurrentIt;
      }
      if (SavedIt == Saved.end() || CurrentIt == Current.end())
      {
	 if (SavedIt == Saved.end())
	 {
	    for (; CurrentIt != Current.end(); ++CurrentIt)
	       Added(CurrentIt.base());
	 }
	 else
	 {
	    for (; SavedIt != Saved.end(); ++SavedIt)
	       Removed(SavedIt.base());
	 }
	 break;
      }
   } while (true);
}
									/*}}}*/

// List - List the saved states								/*{{{*/
// ---------------------------------------------------------------------
/* List the saved states ordered by time */
struct StateEntry
{
   char Name[256];
   time_t Time;
};
static bool List(string_view Dirname)
{
   vector<StateEntry> States;
   struct dirent *Dir;
   char Buffer[256];
   DIR *D = opendir(Dirname.data());
   if (D == NULL)
      return _error->Error(_("Couldn't open the states directory"));
   while ((Dir = readdir(D)) != NULL)
   {
      if (Dir->d_type == DT_REG)
      {
	 StateEntry E;
	 snprintf(Buffer, 256, "%s%s", Dirname.data(), Dir->d_name);
	 strcpy(E.Name, Dir->d_name); E.Time = GetFileTime(Buffer);
	 States.emplace_back(E);
      }
   }
   closedir(D);

   // sort the entries by time
   sort(States.begin(), States.end(), [](const StateEntry &A, const StateEntry &B) { return A.Time < B.Time; });

   // print the entries
   char *TimeStr;
   for (const StateEntry &E : States)
   {
      TimeStr = ctime(&E.Time);
      TimeStr[strlen(TimeStr)-1] = '\0';  // remove trailing newline caused by `ctime`
      ioprintf(c1out, "%s -/- %s\n", E.Name, TimeStr);
   }
   return true;
}
									/*}}}*/

// Delete - Delete a saved state							/*{{{*/
// ---------------------------------------------------------------------
/* Delete the file representing a saved state */
static bool Delete(string_view Dirname, string_view Filename)
{
   if (!Filename.empty())
   {
      if (access(Filename.data(), F_OK) == 0)
	 return _error->Error(_("Saved state not found"));
      if (remove(Filename.data()) != 0)
	 return _error->Error(_("Couldn't delete the save file, are you root?"));
      ioprintf(c1out, "%s\n", _("The saved state has been successfully deleted"));
   }
   else
   {
      ioprintf(c1out, "%s\n", _("The delete command with no name means deleting all saved states!"));
      if (YnPrompt(_("Do you want to continue?")) == false)
      {
	 ioprintf(c2out, "%s\n", _("Abort."));
	 exit(1);
      }
      struct dirent *Dir;
      char Buffer[256];
      DIR *D = opendir(Dirname.data());
      if (D == NULL)
	 return _error->Error(_("Couldn't open the states directory"));
      while ((Dir = readdir(D)) != NULL)
      {
	 if (Dir->d_type == DT_REG)
	 {
	    snprintf(Buffer, 256, "%s%s", Dirname.data(), Dir->d_name);
	    remove(Buffer);
	 }
      }
      closedir(D);
      ioprintf(c1out, "%s\n", _("All the saved states were successfully deleted"));
   }
   return true;
}
									/*}}}*/

// Load - Load a previously saved state						/*{{{*/
// ---------------------------------------------------------------------
/* Load a save state file */
static bool Load(string_view Filename)
{
   if (access(Filename.data(), F_OK) != 0)
      return _error->Error(_("Saved state not found"));

   // load the list of packages constituting the saved state
   OpTextProgress Progress(*_config);
   Progress.OverallProgress(0, 4, 1, _("Loading state"));
   Progress.SubProgress(1, _("Reading saved state"));
   vector<PackageEntry> Saved = LoadState(Filename);
   if (Saved.empty())
      return _error->Error(_("Couldn't load the saved state"));

   // get the list of current packages constituting the state
   Progress.OverallProgress(1, 4, 1, _("Loading state"));
   Progress.SubProgress(1, _("Reading current state"));
   vector<PackageEntry> Current = GetState();
   if (Current.empty())
      return _error->Error(_("Couldn't load the current state"));

   // sort the list to optimize the comparison on load
   Progress.OverallProgress(2, 4, 1, _("Loading state"));
   Progress.SubProgress(1, _("Sorting"));
   sort(Current.begin(), Current.end(), PackageCompare);
   Progress.OverallProgress(3, 4, 1, _("Loading state"));
   Progress.SubProgress(1, _("Deltas"));

   CacheFile Cache;
   if (_config->FindB("APT::Save::Diff", false) == false)
   {
      // initialize the package manager
      Cache.InhibitActionGroups(true);
      bool const WantLock = _config->FindB("APT::Get::Print-URIs", false) == false;
      _config->Set("APT::Install-Recommends", false);

      if (Cache.Open(WantLock) == false)
	 return false;

      pkgProblemResolver Fix(Cache.GetDepCache());
      TryToInstall InstallAction(Cache, &Fix, false);

      // compute the differences between the current state and the saved state
      PackageForEachDelta(Saved, Current, [&Cache](const PackageEntry *const E) {
	 pkgCache::PkgIterator const pkg = Cache->FindPkg(E->Name.c_str(), E->Arch.c_str());
	 Cache->MarkDelete(pkg, false, 0, true);
      }, [&Cache, &InstallAction](const PackageEntry *const E) {
	 pkgCache::PkgIterator const pkg = Cache->FindPkg(E->Name.c_str(), E->Arch.c_str());
	 Cache->SetCandidateVersion(pkg.VersionList()); InstallAction(Cache[pkg].CandidateVerIter(Cache));
      });

      Progress.Done();

      // do not upgrade any package
      APT::PackageVector HeldBackPackages;
      APT::PackageSet UpgradablePackages;
      {
	 APT::CacheSetHelper helper;
	 helper.PackageFrom(APT::CacheSetHelper::PATTERN, &UpgradablePackages, Cache, "?upgradable");
      }
      SortedPackageUniverse Universe(Cache);
      for (auto const &Pkg: Universe)
	 if (Pkg->CurrentVer != 0 && not Cache[Pkg].Delete() &&
	     UpgradablePackages.find(Pkg) != UpgradablePackages.end())
	    HeldBackPackages.push_back(Pkg);

      if (_error->PendingError() || not InstallPackages(Cache, HeldBackPackages, false, true, false))
	 return _error->Error(_("Failed to restore the saved state"));
   }
   else
   {
      const auto addedColor = APT::Configuration::color("action::install");
      const auto removedColor = APT::Configuration::color("action::remove");
      const auto resetColor = not addedColor.empty() ? APT::Configuration::color("neutral") : "";
      const size_t ScreenW = (ScreenWidth > 3) ? ScreenWidth - 3 : 0;
      const string NativeArch = Cache.GetPkgCache()->NativeArch();
      vector<string> Added, Removed;

      // just log the diff
      PackageForEachDelta(Saved, Current, [&NativeArch,&Added](const PackageEntry *const E) {
	 Added.emplace_back(NativeArch != E->Arch ? E->Name + ':' + E->Arch : E->Name);
      }, [&NativeArch,&Removed](const PackageEntry *const E) {
	 Removed.emplace_back(NativeArch != E->Arch ? E->Name + ':' + E->Arch : E->Name);
      });

      Progress.Done();

      c1out << _("The following packages were installed:") << '\n' << addedColor;
      ShowWithColumns(c1out, Added, 2, ScreenW);
      c1out << resetColor << '\n' << _("The following packages were removed:") << '\n' << removedColor;
      ShowWithColumns(c1out, Removed, 2, ScreenW);
      c1out << resetColor << endl;
   }

   return true;
}
									/*}}}*/

// Save - Create a new save state							/*{{{*/
// ---------------------------------------------------------------------
/* Save a state as a file */
static bool Save(string_view Filename, string_view Name)
{
   if (access(Filename.data(), F_OK) == 0)
   {
      time_t const SaveDate = GetFileTime(Filename.data());
      ioprintf(c1out, "%s\n\n%s %s\n%s %s\n", _("A previous save already exist, with the following attributes"), _("Name   :"), Name.data(), _("Created:"), ctime(&SaveDate));
      if (YnPrompt(_("Do you want to override it?")) == false)
      {
	 ioprintf(c2out, "%s\n", _("Abort."));
	 exit(1);
      }
   }
   FILE *File = fopen(Filename.data(), "wb");
   if (File == NULL)
      return _error->Error(_("Couldn't create the save file, are you root?"));

   OpTextProgress Progress(*_config);
   unsigned char *InBuffer = NULL;
   unsigned char *OutBuffer = NULL;
   unsigned long OutBufferLen;
   unsigned long long Done = 0, Total;
   size_t InBufferLen = 0;
   bool success = true;
   {
      // get the list of packages constituting the state
      vector<PackageEntry> Packages = GetState();
      if (Packages.empty())
      {
	 success = false;
	 goto END;
      }

      // sort the list to optimize the comparison on load
      Total = Packages.size();
      Progress.OverallProgress(0, Total * 3, Total, _("Saving state"));
      Progress.SubProgress(Total, _("Sorting"));
      sort(Packages.begin(), Packages.end(), PackageCompare);

      // compute the total length of the buffer to write
      for (const PackageEntry &P : Packages)
	 InBufferLen += P.Len();

      // allocate the input buffer
      if (!(InBuffer = (unsigned char*)malloc(InBufferLen)))
      {
	 success = false;
	 goto END;
      }

      // write the package names and arch into the buffer
      Progress.OverallProgress(Total, Total * 3, Total, _("Saving state"));
      Progress.SubProgress(Total, _("Writing current state"));
      {
	 size_t I = 0;
	 for (const PackageEntry &P : Packages)
	 {
	    if (Done % 50 == 0)
	       Progress.Progress(Done);
	    memcpy(&InBuffer[I], P.Name.c_str(), P.Name.length()); I += P.Name.length(); InBuffer[I] = 0; ++I;
	    memcpy(&InBuffer[I], P.Arch.c_str(), P.Arch.length()); I += P.Arch.length(); InBuffer[I] = 0; ++I;
	    ++Done;
	 }
      }
   }

   Progress.OverallProgress(Total * 2, Total * 3, Total, _("Saving state"));
   Progress.SubProgress(Total, _("Compressing data"));

   // allocate the output buffer
   OutBufferLen = InBufferLen;
   if (!(OutBuffer = (unsigned char*)malloc(OutBufferLen)))
   {
      success = false;
      goto END;
   }

   // compress the buffer using ZLib
   if (compress2(OutBuffer, &OutBufferLen, InBuffer, InBufferLen, Z_BEST_COMPRESSION) != Z_OK)
   {
      success = false;
      goto END;
   }

   Progress.Done();

   // write the initial data length
   fwrite(&InBufferLen, sizeof(size_t), 1, File);
   // write the compressed string into the file
   fwrite(OutBuffer, OutBufferLen, 1, File);

END:
   // close the file, and setup permissions
   free(InBuffer);
   free(OutBuffer);
   fclose(File);
   chmod(Filename.data(), 0644);
   if (!success)
      return _error->Error(_("Couldn't save the current state"));
   ioprintf(c1out, "%s\n", _("The current state has been successfully saved"));
   return true;
}
									/*}}}*/

// Common - Handle the common save related actions		/*{{{*/
// ---------------------------------------------------------------------
/* Handle actions like list or delete saves */
static bool Common(CommandLine &CmdL)
{
   string const SavesDir = _config->FindDir("Dir::State::save");
   if (SavesDir.empty())
      return _error->Error(_("The save directory is not configured, saving states are not available"));

   // handle the list action
   if (_config->FindB("APT::Save::List", false))
      return List(SavesDir);

   // get the save filename
   string SaveFilename;
   if (CmdL.FileList[1] != NULL)
      SaveFilename = SavesDir + CmdL.FileList[1];

   // handle the delete action
   if (_config->FindB("APT::Save::Delete", false))
      return Delete(SavesDir, SaveFilename);

   // do the main action
   if (SaveFilename.empty())
      return _error->Error(_("You must specify a name"));
   if (_config->FindB("APT::Save::Load", false) ||
       _config->FindB("APT::Save::Diff", false))
      return Load(SaveFilename);

   return Save(SaveFilename, CmdL.FileList[1]);
}
									/*}}}*/

// DoSave - Create a new save state							/*{{{*/
// ---------------------------------------------------------------------
/* Save a new temporary state */
bool DoSave(CommandLine &CmdL)
{
   return Common(CmdL);
}
									/*}}}*/

// DoLoad - Rollback to previous saved state				/*{{{*/
// ---------------------------------------------------------------------
/* Revert to a previously saved state */
bool DoLoad(CommandLine &CmdL)
{
   _config->Set("APT::Save::Load", true);
   return Common(CmdL);
}
									/*}}}*/
