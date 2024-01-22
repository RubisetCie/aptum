// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/install-progress.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/prettyprinters.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/upgrade.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <stdlib.h>
#include <string.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-download.h>
#include <apt-private/private-srcbuild.h>
#include <apt-private/private-json-hooks.h>
#include <apt-private/private-output.h>
#include <apt-private/private-update.h>
#include <apt-private/private-source.h>

#include <apti18n.h>

#include <fstream>
#include <sstream>
#include <cerrno>
#include <filesystem>
									/*}}}*/

// Exception that is thrown by a configuration reader
// in case of error that it is not acceptable to move forward.
struct ConfException : public std::exception
{
    std::string msg;
public:
    ConfException(const std::string &msgIn)
    {
        this->msg = msgIn;
    }
    virtual const char* what() const throw() override
    {
        return msg.c_str();
    }
};

// Read a configuration file
// By default it is stored at the following location: /etc/apt/preferences.d
bool read_conf_file(std::vector<std::string> *pPackageList
    , std::vector<std::string> *pBuildOptList)
{
    if (pPackageList == nullptr)
        throw new ConfException("pPackageList == nullptr");
    if (pBuildOptList == nullptr)
        throw new ConfException("pBuildOptList == nullptr");

    const std::string confFileName = "srcinstall.conf";
    const std::string confFileDir = "/etc/apt/preferences.d";

    std::stringstream ssConfFilePath;

    ssConfFilePath << confFileDir;
    ssConfFilePath << "/";
    ssConfFilePath << confFileName;

    std::ifstream confFile(ssConfFilePath.str());

    if (confFile.fail() == true)
        return false;

    int i = 1;
    int k = 1;
    int j = 1;
    for (std::string lStr; std::getline(confFile, lStr); i++)
    {
        std::cout << "read_conf_file(" << i << "): " << lStr << std::endl;

        const std::string strPackageNameStrStartWord = "Package:";
        const std::string strBuildOptStartWord = "Build-Options:";

        auto pFound1 = lStr.find(strPackageNameStrStartWord);
        if (pFound1 != std::string::npos)
        {
            // Read package name and add it to the package list of the caller.
            std::string packageName1 = lStr.substr(strPackageNameStrStartWord.length()
                , lStr.length() - strPackageNameStrStartWord.length());

            std::vector<std::string> nameList1 = VectorizeString(packageName1, ' ');
            for (auto n : nameList1)
            {
                std::string strpStr = APT::String::Strip(n);
                std::cout << "read_conf_file( *package #" << k++ << "* ): " << strpStr << std::endl;
                pPackageList->push_back(strpStr);
            }
        }

        auto pFound2 = lStr.find(strBuildOptStartWord);
        if (pFound2 != std::string::npos)
        {
            // Read option name and add it to the option list of the caller.
            std::string optName1 = lStr.substr(strBuildOptStartWord.length()
                , lStr.length() - strBuildOptStartWord.length());

            std::vector<std::string> optList1 = VectorizeString(optName1, ' ');
            for (auto n : optList1)
            {
                std::string strpStr = APT::String::Strip(n);
                std::cout << "read_conf_file( *opt #" << j++ << "* ):" << strpStr << std::endl;
                pBuildOptList->push_back(strpStr);
            }
        }
    }

    confFile.close();
    return true;
}

// Display the error to standard console output.
void display_err(const std::string &strErr)
{
    std::cout << "SRCBUILD ERROR: " << strErr << std::endl;
}

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

std::string getexecname()
{
    char *pOutChars = new char[MAX_PATH];

    int i = readlink("/proc/self/exe", pOutChars, MAX_PATH);
    if (i == -1)
    {
       perror("readlink");
       return std::string();
    }
    std::string retStr = std::string(pOutChars);
    delete [] pOutChars;
    return retStr;
}
bool read_tmp_deb_result(std::vector<std::string> *pLocalDebFileList)
{
    std::cout << "read_tmp_deb_result...1" << std::endl;
    const std::string strDebTmpFileResult = "tmp.deb.result";
    //if (std::filesystem::exists(strDebTmpFileResult) == false)
	//	return false;
    std::cout << "read_tmp_deb_result...2" << std::endl;
    std::ifstream confFile(strDebTmpFileResult);
    if (confFile.fail() == true)
        return false;
    std::cout << "read_tmp_deb_result...3" << std::endl;

    if (confFile.fail() == true)
        return false;
    std::cout << "read_tmp_deb_result...4" << std::endl;

    for (std::string lStr; std::getline(confFile, lStr); )
    {
        std::cout << "read_tmp_deb_result...4.1" << std::endl;
        const std::string strPackageNameStrStartWord = "/";
        const std::string strPackageNameStrEndWord = ".deb";
        std::string::size_type pFound1 = lStr.find(strPackageNameStrStartWord);
        std::string::size_type pFound2 = lStr.find(strPackageNameStrEndWord);
        if (pFound1 != std::string::npos)
        {
            const std::string::size_type start_pos = pFound1 + 1;
            const std::string::size_type end_pos = pFound2 - pFound1 + (strPackageNameStrEndWord.length() - 1);
            std::string packageFileName = lStr.substr(start_pos, end_pos);
            std::cout << "packageFileName = " << packageFileName << std::endl;
            pLocalDebFileList->push_back(packageFileName);
        }
    }
    confFile.close();
    std::cout << "read_tmp_deb_result...5" << std::endl;
    //_config->localDebFileList = localDebFileList;

    return true;
}
bool DoSrcInstallPrivate(CommandLine &CmdL, SrcBuildOpMode opMode, bool *pExitSuccess)
{
    if (pExitSuccess == nullptr)
        return false;

    // If a local .deb file was specified then return true without operation
    // else continue
    for (int i = 0; i < CmdL.FileSize(); i++)
    {
        const std::string strFileName = CmdL.FileList[i];
        std::cout << "DoSrcInstall... check1: " << strFileName << std::endl;
        std::string optNameEnds = ".deb";
        auto pFound1 = strFileName.find(optNameEnds);
        if (pFound1 != std::string::npos)
        {
            std::cout << "DoSrcInstall... .deb file found in arguments, continuing without srcinstall operation." << std::endl;
            return true;
        }
    }

    std::cout << "DoSrcInstall..." << std::endl;
    std::vector<std::string> packageList;
    std::vector<std::string> buildOptList;

    if (read_conf_file(&packageList, &buildOptList) == false)
    {
        display_err("read_config_file FAILED!");
        return false;
    }

    bool packageFoundInConfList = false;
    const std::string strInstall = "install";
    const std::string strUpgrade = "upgrade";
    bool bInstallCmdFound = false;
    bool bUpgradeCmdFound = false;
    // If no argument was specified with a package found
    // in a configuration list then return true without operation
    for (int i = 0; i < CmdL.FileSize(); i++)
    {
        const std::string strFileName = CmdL.FileList[i];
        if (strFileName == strInstall)
            bInstallCmdFound = true;
        if (strFileName == strUpgrade)
            bUpgradeCmdFound = true;
        std::cout << "DoSrcInstall check2: " << strFileName << std::endl;
        for (int j = 0; j < packageList.size(); j++)
        {
            std::string optName = packageList[j];
            int cmpResult = strFileName.compare(optName);
            if (cmpResult == 0)
            {
                std::cout << "DoSrcInstall check2, package name found in a config file: " << CmdL.FileList[i] << std::endl;
                packageFoundInConfList = true;
                break;
            }
        }
        if (packageFoundInConfList == true)
            break;
    }
    if (packageFoundInConfList == false)
    {
        std::cout << "DoSrcInstall... config file list match not found in arguments, continuing without srcinstall operation." << std::endl;
        return true;
    }
    if (bInstallCmdFound == false && bUpgradeCmdFound == false)
    {
        std::cout << "DoSrcInstall... no install/upgrade commands found, continuing without srcinstall operation." << std::endl;
        return true;
    }

    // Perform update if required.
    if (_config->FindB("APT::Update") == true)
    {
        std::cout << "DoSrcInstall... Performing DoUpdate()..." << std::endl;
        if (DoUpdate() == false)
            return false;
        std::cout << "DoSrcInstall... Performing DoUpdate()...OK" << std::endl;
    }

    // Assign list of build packages
    // and build options list to the global configuration.
    _config->buildPkgList = packageList;
    _config->buildOptList = buildOptList;

    std::cout << "DoSrcInstall... Performing DoSource()..." << std::endl;
    // This routine should download the package source, then compile it using
    // the options provided by global configuration _config->buildOptList list.
    if (DoSource(CmdL) == false)
    {
        display_err("DoSource FAILED!");
        return false;
    }
    //_config->localDebFileList.push_back("libogg0_1.3.5-3_amd64.deb");
    std::cout << "DoSrcInstall... Performing DoSource()...OK" << std::endl;

    // The following will perform installation and resolve deps
    // for the .deb file:
    // 1. Make a fork of this process
    // 2. Execute APT in a separate process to install the obtained .edb file.
    // 3. The separate APT process will install and resolve dependencies.
    // 4. This process exits normally.

    pid_t procPID1 = ExecFork();
    if (procPID1 == 0)
    {
        std::vector<std::string> localDebFileList;
        read_tmp_deb_result(&localDebFileList);

        const std::string filePath = getexecname();
        std::cout << "DoSrcInstall... Child...3.1" << std::endl;
        std::string opCmd;
        if (opMode == SrcBuildOpMode::SRC_BUILD_INSTALL)
            opCmd = "install";
        else if (opMode == SrcBuildOpMode::SRC_BUILD_UPGRADE)
            opCmd = "upgrade";

        std::string targetDebPackageFileName1;
        for (int i = 0; i < localDebFileList.size(); i++)
        {
            if (    localDebFileList[i].find("-dev_") != std::string::npos
                ||  localDebFileList[i].find("-dbgsym_") != std::string::npos )
                continue;
            else
            {
                targetDebPackageFileName1 = localDebFileList[i];
                break;
            }
        }
        if (targetDebPackageFileName1.empty() == true)
        {
            display_err("targetDebPackageFileName is empty!");
            return false;
        }

        std::stringstream ss;
        ss << "./";
        // Get the first .deb file from the configuration saved list
        // The other two are dbg and dev, need the first one
        ss << targetDebPackageFileName1;

        std::stringstream ss1;
        ss1 << "DoSrcInstall... continue to install/upgrade the .deb that was built in previous step..."
            << std::endl
            << "cmdline: "
            << filePath;
            ss1 << " " << opCmd << " ";
            ss1  << ss.str();

        puts(ss1.str().c_str());

        std::cout << "DoSrcInstall... Executing the separate process using APT path, " << opCmd << " operation"
            << std::endl
            << "\tfilePath = " << filePath
            << std::endl
            << "\tss.str() = " << ss.str()
            << std::endl;

        const char *Args[4];
        Args[0] = filePath.c_str();
        Args[1] = opCmd.c_str();
        std::string strDebFile = ss.str();
        Args[2] = strDebFile.c_str();
        Args[3] = 0;
        execv(Args[0], (char **)Args);
    }

    std::cout << "DoSrcInstall... (2)" << std::endl;

    *pExitSuccess = true;
    if (ExecWait(procPID1, "apt", true) == false)
    {
        pid_t procPID2 = ExecFork();
        if (procPID2 == 0)
        {
            std::cout << "DoSrcInstall... (Child 2)" << std::endl;
            const std::string filePath = getexecname();
            const char *Args[4];
            Args[0] = filePath.c_str();
            Args[1] = "--fix-broken";
            Args[2] = "install";
            Args[3] = 0;
            execv(Args[0], (char **)Args);
        }
        ExecWait(procPID2, "apt", true);
    }
    else
    {
        std::vector<std::string> newCmdParams;
        // If other packages instructed for install/upgrade
        // fork a new process to perform excluding the packages that were buit from sources
        for (int i = 0; i < CmdL.FileSize(); i++)
        {
            const std::string strFileName = CmdL.FileList[i];
            if (strFileName == strInstall)
                continue;
            if (strFileName == strUpgrade)
                continue;
            std::cout << "DoSrcInstall check3: " << strFileName << std::endl;
            bool bFound = false;
            for (int j = 0; j < packageList.size(); j++)
            {
                std::string optName = packageList[j];
                int cmpResult = strFileName.compare(optName);
                if (cmpResult == 0)
                {
                    std::cout << "DoSrcInstall check4, package name found in a config file: " << strFileName << std::endl;
                    bFound = true;
                    break;
                }
            }
            if (bFound == false)
            {
                std::cout << "DoSrcInstall check5, adding a new cmd param: " << strFileName << std::endl;
                newCmdParams.push_back(strFileName);
            }
        }
        const std::string filePath = getexecname();
        std::string opCmd;
        if (opMode == SrcBuildOpMode::SRC_BUILD_INSTALL)
            opCmd = "install";
        else if (opMode == SrcBuildOpMode::SRC_BUILD_UPGRADE)
            opCmd = "upgrade";
        const int newArgsSize = newCmdParams.size() + 3;
        char **newArgs = new char* [newArgsSize];
        for (int i = 0, k = 0; i < newArgsSize; i++)
        {
            if (i == 0)
            {
                newArgs[i] = (char *)filePath.c_str();
                continue;
            }
            if (i == 1)
            {
                newArgs[i] = (char *)opCmd.c_str();
                continue;
            }
            if (i == (newArgsSize - 1))
            {
                newArgs[i] = (char *)NULL;
                continue;
            }
            newArgs[i] = (char *)newCmdParams[k++].c_str();
        }
        execv(newArgs[0], (char **)newArgs);
    }
    std::cout << "DoSrcInstall... (Parent) OK" << std::endl;
    return true;
}
