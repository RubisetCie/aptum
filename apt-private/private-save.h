#ifndef APT_PRIVATE_SAVE_H
#define APT_PRIVATE_SAVE_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool DoSave(CommandLine &CmdL);
APT_PUBLIC bool DoLoad(CommandLine &CmdL);

#endif
