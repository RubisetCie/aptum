#ifndef APT_PRIVATE_SAVE_H
#define APT_PRIVATE_SAVE_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool DoSave(CommandLine&);
APT_PUBLIC bool DoRollback(CommandLine&);
APT_PUBLIC bool DoApply(CommandLine&);
APT_PUBLIC bool DoLog(CommandLine&);

#endif
