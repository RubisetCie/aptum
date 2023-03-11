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

#include <apti18n.h>
									/*}}}*/

using namespace std;

// DoSave - Create the save file							/*{{{*/
// ---------------------------------------------------------------------
/* Initialize the temporary state */
bool DoSave(CommandLine&)
{
   FILE *f = NULL;
   string const save_name = _config->FindFile("Dir::State::save", "/dev/null");
   if (save_name == "/dev/null")
      return _error->Error(_("The default save file is null, temporary state are not available"));
   if (access(save_name.c_str(), F_OK) == 0)
   {
      ioprintf(c1out, "%s\n", _("A previous temporary session exists, and will be overridden"));
      if (YnPrompt(_("Do you want to continue?")) == false)
      {
	 ioprintf(c2out, "%s\n", _("Abort."));
	 exit(1);
      }
   }
   f = fopen(save_name.c_str(), "w");
   if (f == NULL)
      return _error->Error(_("Couldn't create the save file"));
   fclose(f);
   chmod(save_name.c_str(), 0644);
   ioprintf(c1out, "%s\n", _("The temporary state has been initialized"));
   return true;
}
									/*}}}*/

// DoRollback - Rollback to previous save					/*{{{*/
// ---------------------------------------------------------------------
/* Revert to the previous saved state */
bool DoRollback(CommandLine&)
{
   FILE *f = NULL;
   string const save_name = _config->FindFile("Dir::State::save", "/dev/null");
   if (save_name == "/dev/null")
      return _error->Error(_("The default save file is null, temporary state are not available"));
   if (access(save_name.c_str(), R_OK) != 0)
      return _error->Error(_("No temporary state is in use"));
   f = fopen(save_name.c_str(), "r");
   if (f == NULL)
      return _error->Error(_("Couldn't read the save file"));

   bool const WantLock = _config->FindB("APT::Get::Print-URIs", false) == false;
   CacheFile Cache;
   Cache.InhibitActionGroups(true);

   _config->Set("APT::Install-Recommends", false);
   //_config->Set("APT::Get::Purge", true);

   if (Cache.Open(WantLock) == false)
      return false;

   pkgProblemResolver Fix(Cache.GetDepCache());
   TryToInstall InstallAction(Cache, &Fix, false);

   // parse the save file
   char *line = NULL;
   size_t len = 0;
   while (getline(&line, &len, f) != -1)
   {
      size_t const length = strlen(line);
      if (length < 3)
	 continue;
      char const prefix = line[0];
      char *entry = (char*)malloc(length-2);
      strncpy(entry, &line[2], length-3);
      entry[length-3] = '\0';

      // extract the package name and arch based on the ':' separator
      size_t arch = 0;
      for (size_t i = 0; i < length-2; i++)
      {
	 if (entry[i] == ':')
	 {
	    entry[i] = '\0';
	    arch = i+1;
	    break;
	 }
      }

      // mark the package for installation/removal
      if (prefix == '-')
      {
	 pkgCache::PkgIterator const pkg = arch == 0 ?
	    Cache->FindPkg(entry, &entry[arch]) :
	    Cache->FindPkg(entry);
	 Cache->SetCandidateVersion(pkg.VersionList());
	 InstallAction(Cache[pkg].CandidateVerIter(Cache));
      }
      else
      {
	 pkgCache::PkgIterator const pkg = arch == 0 ?
	    Cache->FindPkg(entry, &entry[arch]) :
	    Cache->FindPkg(entry);
	 Cache->MarkDelete(pkg, false, 0, true);
      }
      free(entry);
   }
   if (line)
      free(line);

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

   if (_error->PendingError() || not InstallPackages(Cache, HeldBackPackages, false))
      return _error->Error(_("Failed to process rollback"));
   if (remove(save_name.c_str()) != 0)
      return _error->Error(_("Couldn't remove the save file"));
   return true;
}
									/*}}}*/

// DoApply - Delete the save file							/*{{{*/
// ---------------------------------------------------------------------
/* Close the temporary state */
bool DoApply(CommandLine&)
{
   string const save_name = _config->FindFile("Dir::State::save", "/dev/null");
   if (save_name == "/dev/null")
      return _error->Error(_("The default save file is null, temporary state are not available"));
   if (remove(save_name.c_str()) != 0)
      return _error->Error(_("Couldn't remove the save file"));
   ioprintf(c1out, "%s\n", _("The temporary state has been applied"));
   return true;
}
									/*}}}*/

// DoLog - Print the diff of the temporary state			/*{{{*/
// ---------------------------------------------------------------------
bool DoLog(CommandLine&)
{
   FILE *f = NULL;
   string const save_name = _config->FindFile("Dir::State::save", "/dev/null");
   if (save_name == "/dev/null")
      return _error->Error(_("The default save file is null, temporary state are not available"));
   if (access(save_name.c_str(), R_OK) != 0)
      return _error->Error(_("No temporary state is in use"));
   f = fopen(save_name.c_str(), "r");
   if (f == NULL)
      return _error->Error(_("Couldn't read the save file"));

   char *line = NULL;
   size_t len = 0;
   while (getline(&line, &len, f) != -1)
   {
      size_t const length = strlen(line);
      if (length < 3)
	 continue;
      char const prefix = line[0];
      char *entry = (char*)malloc(length-2);
      strncpy(entry, &line[2], length-3);
      entry[length-3] = '\0';
      ioprintf(c1out, "[%s] %s\n", prefix == '+' ? _("I") : _("R"), entry);
      free(entry);
   }
   if (line)
      free(line);
   else
      ioprintf(c1out, "%s\n", _("Nothing in the temporary state"));

   fclose(f);
   return true;
}
									/*}}}*/
