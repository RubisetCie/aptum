#ifndef APT_PRIVATE_SRCBUILD_H
#define APT_PRIVATE_SRCBUILD_H

#include <apt-pkg/depcache.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <list>
#include <string>
#include <utility>

class CacheFile;
class CommandLine;
class pkgProblemResolver;

// Operation mode
enum SrcBuildOpMode
{
   // Perform download of package sources, build, obtain a .deb file, run APT install/upgrade on it
   SRC_BUILD_INSTALL = 0,
   SRC_BUILD_UPGRADE = 1
};

APT_PUBLIC bool DoSrcInstallPrivate(CommandLine &CmdL, SrcBuildOpMode opMode, bool *pExitSuccess);

//APT_PUBLIC bool DoSrcInstall(CommandLine &Cmd);

#endif
