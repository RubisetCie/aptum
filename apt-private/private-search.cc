// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-json-hooks.h>
#include <apt-private/private-output.h>
#include <apt-private/private-search.h>
#include <apt-private/private-show.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <string.h>

#include <apti18n.h>
									/*}}}*/

static std::vector<pkgCache::DescIterator> const TranslatedDescriptionsList(pkgCache::VerIterator const &V) /*{{{*/
{
   std::vector<pkgCache::DescIterator> Descriptions;

   for (std::string const &lang: APT::Configuration::getLanguages())
   {
      pkgCache::DescIterator Desc = V.TranslatedDescriptionForLanguage(lang);
      if (Desc.IsGood())
         Descriptions.push_back(Desc);
   }

   if (Descriptions.empty() && V.TranslatedDescription().IsGood())
      Descriptions.push_back(V.TranslatedDescription());

   return Descriptions;
}

									/*}}}*/
// LocalitySort - Sort a version list by package file locality		/*{{{*/
static int LocalityCompare(const void * const a, const void * const b)
{
   pkgCache::VerFile const * const A = *static_cast<pkgCache::VerFile const * const *>(a);
   pkgCache::VerFile const * const B = *static_cast<pkgCache::VerFile const * const *>(b);

   if (A == 0 && B == 0)
      return 0;
   if (A == 0)
      return 1;
   if (B == 0)
      return -1;

   if (A->File == B->File)
      return A->Offset - B->Offset;
   return A->File - B->File;
}
void LocalitySort(pkgCache::VerFile ** const begin, unsigned long long const Count,size_t const Size)
{
   qsort(begin,Count,Size,LocalityCompare);
}
static void LocalitySort(pkgCache::DescFile ** const begin, unsigned long long const Count,size_t const Size)
{
   qsort(begin,Count,Size,LocalityCompare);
}
									/*}}}*/
// Search - Perform a search						/*{{{*/
// ---------------------------------------------------------------------
/* This searches the package names and package descriptions for a pattern */
struct ExDescFile
{
   pkgCache::DescFile *Df;
   pkgCache::VerIterator V;
   map_id_t ID;
   ExDescFile() : Df(nullptr), ID(0) {}
};
static inline bool Search(CommandLine &CmdL)
{
   bool const ShowFull = _config->FindB("APT::Cache::ShowFull",false);
   bool const Installed = _config->FindB("APT::Cache::Installed",false);
   bool const Library = _config->FindB("APT::Cache::Libs",false);
   bool const Devel = _config->FindB("APT::Cache::Devel",false);
   unsigned int const NumPatterns = CmdL.FileSize() -1;
   
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache::Policy *Plcy = CacheFile.GetPolicy();
   if (unlikely(Cache == NULL || Plcy == NULL))
      return false;

   // Make sure there is at least one argument
   if (NumPatterns < 1)
      return _error->Error(_("You must give at least one search pattern"));
   
   // Compile the regex pattern
   regex_t *Patterns = new regex_t[NumPatterns];
   memset(Patterns,0,sizeof(*Patterns)*NumPatterns);
   for (unsigned I = 0; I != NumPatterns; I++)
   {
      if (regcomp(&Patterns[I],CmdL.FileList[I+1],REG_EXTENDED | REG_ICASE | 
		  REG_NOSUB) != 0)
      {
	 for (; I != 0; I--)
	    regfree(&Patterns[I]);
	 return _error->Error("Regex compilation error");
      }      
   }
   
   if (_error->PendingError() == true)
   {
      for (unsigned I = 0; I != NumPatterns; I++)
	 regfree(&Patterns[I]);
      return false;
   }
   
   size_t const descCount = Cache->HeaderP->GroupCount + 1;
   ExDescFile *DFList = new ExDescFile[descCount];

   bool *PatternMatch = new bool[descCount * NumPatterns];
   memset(PatternMatch,false,sizeof(*PatternMatch) * descCount * NumPatterns);

   // Map versions that we want to write out onto the VerList array.
   bool const NamesOnly = _config->FindB("APT::Cache::NamesOnly",false);
   bool const NoDesc = _config->FindB("APT::Cache::NoDesc",false);
   for (pkgCache::GrpIterator G = Cache->GrpBegin(); G.end() == false; ++G)
   {
      size_t const PatternOffset = G->ID * NumPatterns;
      size_t unmatched = 0, matched = 0;
      for (unsigned I = 0; I < NumPatterns; ++I)
      {
	 if (PatternMatch[PatternOffset + I] == true)
	    ++matched;
	 else if (regexec(&Patterns[I],G.Name(),0,0,0) == 0)
	    PatternMatch[PatternOffset + I] = true;
	 else
	    ++unmatched;
      }

      // already dealt with this package?
      if (matched == NumPatterns)
	 continue;

      // Doing names only, drop any that don't match..
      if (NamesOnly == true && unmatched == NumPatterns)
	 continue;

      // Find the proper version to use
      pkgCache::PkgIterator P = G.FindPreferredPkg();
      if (P.end() == true)
	 continue;

      // Skip if non-installed (if asked)
      if (Installed && P->CurrentVer == 0)
	 continue;

      pkgCache::VerIterator V = Plcy->GetCandidateVer(P);
      if (V.end() == false)
      {
	    // Skip if filtered by section
	    if (Library || Devel)
	    {
		   const char *section = V.Section();
		   bool skip = true;
		   if (Library && strcmp(section, "libs") == 0)
			skip = false;
		   if (skip && Devel && strcmp(section, "devel") == 0)
			skip = false;
		   if (skip)
			continue;
	    }

	 pkgCache::DescIterator const D = V.TranslatedDescription();
	 //FIXME: packages without a description can't be found
	 if (D.end() == true)
	    continue;
	 DFList[G->ID].Df = D.FileList();
	 DFList[G->ID].V = V;
	 DFList[G->ID].ID = G->ID;
      }

      if (unmatched == NumPatterns)
	 continue;

      // Include all the packages that provide matching names too
      for (pkgCache::PrvIterator Prv = P.ProvidesList() ; Prv.end() == false; ++Prv)
      {
	 pkgCache::VerIterator V = Plcy->GetCandidateVer(Prv.OwnerPkg());
	 if (V.end() == true)
	    continue;

	 unsigned long id = Prv.OwnerPkg().Group()->ID;
	 pkgCache::DescIterator const D = V.TranslatedDescription();
	 //FIXME: packages without a description can't be found
	 if (D.end() == true)
	    continue;
	 DFList[id].Df = D.FileList();
	 DFList[id].V = V;
	 DFList[id].ID = id;

	 size_t const PrvPatternOffset = id * NumPatterns;
	 for (unsigned I = 0; I < NumPatterns; ++I)
	    PatternMatch[PrvPatternOffset + I] |= PatternMatch[PatternOffset + I];
      }
   }

   LocalitySort(&DFList->Df, Cache->HeaderP->GroupCount, sizeof(*DFList));

   // Create the text record parser
   pkgRecords Recs(*Cache);
   // Iterate over all the version records and check them
   for (ExDescFile *J = DFList; J->Df != 0; ++J)
   {
      size_t const PatternOffset = J->ID * NumPatterns;
      if (not NamesOnly)
      {
         std::vector<std::string> PkgDescriptions;
         for (auto &Desc: TranslatedDescriptionsList(J->V))
         {
            pkgRecords::Parser &parser = Recs.Lookup(Desc.FileList());
            PkgDescriptions.push_back(parser.LongDesc());
         }

         std::vector<bool> SkipDescription(PkgDescriptions.size(), false);
         for (unsigned I = 0; I < NumPatterns; ++I)
         {
            if (PatternMatch[PatternOffset + I])
               continue;
            else
            {
               bool found = false;

               for (std::vector<std::string>::size_type k = 0; k < PkgDescriptions.size(); ++k)
               {
                  if (not SkipDescription[k])
                  {
                     if (regexec(&Patterns[I], PkgDescriptions[k].c_str(), 0, 0, 0) == 0)
                     {
                        found = true;
                        PatternMatch[PatternOffset + I] = true;
                     }
                     else
                        SkipDescription[k] = true;
                  }
               }

               if (not found)
                  break;
            }
         }
      }

      bool matchedAll = true;
      for (unsigned I = 0; I < NumPatterns; ++I)
	 if (PatternMatch[PatternOffset + I] == false)
	 {
	    matchedAll = false;
	    break;
	 }

      if (matchedAll == true)
      {
	 if (ShowFull == true)
	 {
	    pkgCache::VerFileIterator Vf;
	    auto &Parser = LookupParser(Recs, J->V, Vf);
	    char const *Start, *Stop;
	    Parser.GetRec(Start, Stop);
	    size_t const Length = Stop - Start;
	    DisplayRecordV1(CacheFile, Recs, J->V, Vf, Start, Length, std::cout);
	 }
	 else
	 {
	    pkgRecords::Parser &P = Recs.Lookup(pkgCache::DescFileIterator(*Cache, J->Df));
	    if (not NoDesc)
         printf("%s - %s\n", P.Name().c_str(), P.ShortDesc().c_str());
	    else
         printf("%s\n", P.Name().c_str());
	 }
      }
   }
   
   delete [] DFList;
   delete [] PatternMatch;
   for (unsigned I = 0; I != NumPatterns; I++)
      regfree(&Patterns[I]);
   delete [] Patterns;
   if (ferror(stdout))
       return _error->Error("Write to stdout failed");
   return true;
}
									/*}}}*/
bool DoSearch(CommandLine &CmdL)					/*{{{*/
{
   return Search(CmdL);
}

