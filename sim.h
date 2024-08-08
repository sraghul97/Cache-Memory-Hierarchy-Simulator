
#define MaxAddressBitSize 32
#define TagB 1
#define IndexB 2
#define OffsetB 3

#include <vector>
#include <math.h>
#include <string>
#include <iostream>
#include <bitset>
#include <array>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bitset>

using namespace std;

class CacheModule
{
public:
   // InputParameters
   uint32_t BLOCKSIZE;
   uint32_t SIZE = 0;
   uint32_t ASSOC;
   uint32_t PREF_N = 0;
   uint32_t PREF_M = 0;

   // ComputedParameters
   uint32_t NumberOfSets;
   uint32_t IndexBitCount;
   uint32_t BlockOffsetBitCount;
   uint32_t TagOffsetBitCount;
   uint32_t TagAddressPrefetchOffset;

   // OutputPerformanceParameters
   uint32_t ReadCount = 0;
   uint32_t ReadMissCount = 0;
   uint32_t ReadPrefetchCount = 0;
   uint32_t ReadMissPrefetchCount = 0;
   uint32_t WriteCount = 0;
   uint32_t WriteMissCount = 0;
   uint32_t WriteBackCount = 0;
   uint32_t PrefetchesCount = 0;
   uint32_t MemoryTraffic = 0;

   // CacheContents
   vector<vector<bool>> CacheValidBit;
   vector<vector<bool>> CacheDirtyBit;
   vector<vector<uint32_t>> CacheTagAddress;
   vector<vector<uint32_t>> CacheTag;
   vector<vector<uint32_t>> CacheLruBit;

   // PrefetchContents
   vector<bool> PrefetchValidBit;
   vector<vector<uint32_t>> PrefetchTagAddress;
   vector<uint32_t> PrefetchLruBit;

   CacheModule(uint32_t InputBlockSize, uint32_t InputSize, uint32_t InputAssoc, uint32_t InputPrefetchN, uint32_t InputPrefetchM)
   {
      SIZE = InputSize;
      ASSOC = InputAssoc;
      if (InputSize > 0)
      {
         BLOCKSIZE = InputBlockSize;
         PREF_N = InputPrefetchN;
         PREF_M = InputPrefetchM;
         NumberOfSets = InputSize / (InputAssoc * InputBlockSize);
         IndexBitCount = log2(NumberOfSets);
         BlockOffsetBitCount = log2(InputBlockSize);
         TagOffsetBitCount = MaxAddressBitSize - IndexBitCount - BlockOffsetBitCount;
         TagAddressPrefetchOffsetCalculation();

         // CacheContents
         CacheValidBit.resize(NumberOfSets, vector<bool>(ASSOC, false));
         CacheDirtyBit.resize(NumberOfSets, vector<bool>(ASSOC, false));
         CacheTagAddress.resize(NumberOfSets, vector<uint32_t>(ASSOC, 0));
         CacheTag.resize(NumberOfSets, vector<uint32_t>(ASSOC, 0));
         CacheLruBit.resize(NumberOfSets, vector<uint32_t>(ASSOC, 0));
         for (uint32_t SetCounter = 0; SetCounter < NumberOfSets; SetCounter++)
         {
            for (uint32_t LruCounter = 0; LruCounter < ASSOC; LruCounter++)
               CacheLruBit[SetCounter][LruCounter] = LruCounter;
         }

         // PrefetchContents
         PrefetchValidBit.resize(PREF_N);
         PrefetchTagAddress.resize(PREF_N, vector<uint32_t>(PREF_M, 0));
         PrefetchLruBit.resize(PREF_N);
         for (uint32_t LruCounter = 0; LruCounter < PREF_N; LruCounter++)
            PrefetchLruBit[LruCounter] = LruCounter;

         // OutputParameters
         ReadCount = 0;
         ReadMissCount = 0;
         ReadPrefetchCount = 0;
         ReadMissPrefetchCount = 0;
         WriteCount = 0;
         WriteMissCount = 0;
         WriteBackCount = 0;
         PrefetchesCount = 0;
         MemoryTraffic = 0;
      }
   }

   ~CacheModule()
   {
   }

   string IntegerTostring(uint32_t IntData, bool SameLengthFlag = false)
   {
      ostringstream ss;
      if (SameLengthFlag)
         ss << "0x" << setfill('0') << setw(8) << hex << IntData;
      else
         ss << hex << IntData;
      return ss.str();
   }

   array<uint32_t, 4> GetTagParameters(uint32_t TagData)
   {
      array<uint32_t, 4> TagIndividualDataBits;
      string TagDataBinary = bitset<32>(TagData).to_string();
      TagIndividualDataBits[0] = TagData;
      TagIndividualDataBits[TagB] = stoull(TagDataBinary.substr(0, TagOffsetBitCount), 0, 2);
      try
      {
         TagIndividualDataBits[IndexB] = stoull(TagDataBinary.substr(MaxAddressBitSize - IndexBitCount - BlockOffsetBitCount, IndexBitCount), 0, 2);
      }
      catch (const std::exception &e)
      {
         TagIndividualDataBits[IndexB] = stoull("0", 0, 2);
      }
      TagIndividualDataBits[OffsetB] = stoull(TagDataBinary.substr(MaxAddressBitSize - BlockOffsetBitCount, BlockOffsetBitCount), 0, 2);
      return TagIndividualDataBits;
   }

   uint32_t TagAddressCalculation(uint32_t AddressTag, uint32_t AddressIndex, uint32_t AddressBlock = 0)
   {
      if (AddressIndex >= pow(2, IndexBitCount))
      {
         AddressTag += AddressIndex / pow(2, IndexBitCount);
         AddressIndex = AddressIndex - pow(2, IndexBitCount);
      }

      if (AddressTag >= pow(2, TagOffsetBitCount))
      {
         AddressTag = 0;
         AddressIndex = AddressIndex - pow(2, IndexBitCount);
      }

      string TagBinary = bitset<32>(AddressTag).to_string().substr(32 - TagOffsetBitCount);
      string IndexBinary = bitset<32>(AddressIndex).to_string().substr(32 - IndexBitCount);
      string BlockBinary = bitset<32>(0).to_string().substr(32 - BlockOffsetBitCount);
      uint32_t ResultTagAddress = stoull(TagBinary + IndexBinary, 0, 2);

      return ResultTagAddress;
   }

   void TagAddressPrefetchOffsetCalculation()
   {
      TagAddressPrefetchOffset = (TagAddressCalculation(0, 1) - TagAddressCalculation(0, 0));
   }

   bool CacheMiss(uint32_t TagData)
   {
      if (SIZE == 0)
         return true;

      array<uint32_t, 4> TagIndividualDataBits = GetTagParameters(TagData);
      for (uint32_t AssociativitySearch = 0; AssociativitySearch < ASSOC; AssociativitySearch++)
      {
         if ((CacheTag[TagIndividualDataBits[IndexB]][AssociativitySearch] == TagIndividualDataBits[TagB]) && (CacheValidBit[TagIndividualDataBits[IndexB]][AssociativitySearch]))
            return false;
      }
      return true;
   }

   void CacheDirtyBitEviction(uint32_t TagData, CacheModule &LowerCache, CacheModule &LowerCache1)
   {
      array<uint32_t, 4> TagIndividualDataBits = GetTagParameters(TagData);
   
      for (uint32_t AssociativitySearch = 0; AssociativitySearch < ASSOC; AssociativitySearch++)
      {
         if (((CacheLruBit[TagIndividualDataBits[IndexB]][AssociativitySearch]) == (ASSOC - 1)) && (CacheValidBit[TagIndividualDataBits[IndexB]][AssociativitySearch]) && ((CacheTagAddress[TagIndividualDataBits[IndexB]][AssociativitySearch]) != TagData) && (CacheDirtyBit[TagIndividualDataBits[IndexB]][AssociativitySearch]))
         {
            TagIndividualDataBits = GetTagParameters((CacheTagAddress[TagIndividualDataBits[IndexB]][AssociativitySearch]));
            if ((CacheDirtyBit[TagIndividualDataBits[IndexB]][AssociativitySearch]) && (CacheValidBit[TagIndividualDataBits[IndexB]][AssociativitySearch])) // check if valid = 1 and dirty = 1
            {
               if (LowerCache.SIZE != 0)
               {
                  bool UpdatePrefetchFlag = true;
                  uint32_t TagData = CacheTagAddress[TagIndividualDataBits[IndexB]][AssociativitySearch];
                  if (!(LowerCache.CacheMiss(TagData)))
                  { // L2 Hit
                     if (LowerCache.PrefetchMiss(TagData))
                        UpdatePrefetchFlag = false;
                  }
                  LowerCache.UpdateCacheContents(TagData, true, LowerCache1, LowerCache1, UpdatePrefetchFlag, false, false, true);
                  LowerCache.WriteCount += 1;
               }
               else
                  MemoryTraffic += 1;
               CacheDirtyBit[TagIndividualDataBits[IndexB]][AssociativitySearch] = false;
               WriteBackCount += 1;
               return;
            }
         }
      }
   }

   void UpdateCacheContents(uint32_t TagData, bool WriteFlag, CacheModule &LowerCache, CacheModule &LowerCache1, bool UpdatePrefetchFlag, bool Iteration = false, bool MaskCacheLruUpdate = false, bool EvictionFlag = false)
   {
      if (SIZE == 0)
         return;

      array<uint32_t, 4> TagIndividualDataBits = GetTagParameters(TagData);

      vector<uint32_t> CacheLruOrderSearch;
      CacheLruOrderSearch.resize(ASSOC);
      for (uint32_t i = 0; i < ASSOC; i++)
      {
         for (uint32_t ii = 0; ii < ASSOC; ii++)
         {
            if (CacheLruBit[TagIndividualDataBits[IndexB]][ii] == i)
            {
               CacheLruOrderSearch[i] = ii;
               break;
            }
         }
      }

   Position1:
      for (uint32_t CacheAssociativitySearch = 0; CacheAssociativitySearch < ASSOC; CacheAssociativitySearch++)
      {
         uint32_t AssociativityLruSearch = CacheLruOrderSearch[CacheAssociativitySearch];
         if (((CacheTag[TagIndividualDataBits[IndexB]][AssociativityLruSearch] == TagIndividualDataBits[TagB]) && (CacheValidBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch])) || (((CacheLruBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch]) == (ASSOC - 1)) && (!MaskCacheLruUpdate)))
         {
            if ((CacheDirtyBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch]) && (CacheValidBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch]) && (CacheTag[TagIndividualDataBits[IndexB]][AssociativityLruSearch] == TagIndividualDataBits[TagB]))
            { // Cache Hit
               CacheTagAddress[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = TagData;
               CacheTag[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = TagIndividualDataBits[TagB];
               CacheValidBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = true;

               if (!MaskCacheLruUpdate)
                  UpdateCacheLRU(TagData, AssociativityLruSearch);
               if (UpdatePrefetchFlag)
                  UpdatePrefetchContents(TagData, WriteFlag);
               return;
            }
            else if (!((CacheDirtyBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch]) && (CacheValidBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch]))) // check if ValidBit != 1 or DirtyBit != 1
            {                                                                                                                                                             // Cache Miss
               if ((EvictionFlag) && (CacheTag[TagIndividualDataBits[IndexB]][AssociativityLruSearch] != TagIndividualDataBits[TagB]) && PrefetchMiss(TagData))
               {
                  if (WriteFlag)
                     WriteMissCount += 1;
                  else
                     ReadMissCount += 1;
                  MemoryTraffic += 1;
               }

               CacheTagAddress[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = TagData;
               CacheTag[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = TagIndividualDataBits[TagB];
               CacheValidBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = true;
               CacheDirtyBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = WriteFlag;

               // if (LowerCache.SIZE == 0)
               //  MemoryTraffic += 1;

               if (!MaskCacheLruUpdate)
                  UpdateCacheLRU(TagData, AssociativityLruSearch);
               if (UpdatePrefetchFlag)
                  UpdatePrefetchContents(TagData, WriteFlag);
               return;
            }
            else if (!Iteration)
            {
               Iteration = true;
               if (LowerCache.SIZE != 0)
               {
                  // LowerCache.UpdateCacheContents(CacheTagAddress[TagIndividualDataBits[IndexB]][AssociativityLruSearch], true, LowerCache1, LowerCache1, UpdatePrefetchFlag, Iteration, false, true);
                  // LowerCache.WriteCount += 1;
               }
               else
                  MemoryTraffic += 1;
               WriteBackCount += 1;

               CacheDirtyBit[TagIndividualDataBits[IndexB]][AssociativityLruSearch] = false;
               goto Position1;
            }
         }
      }
   }

   void UpdateCacheLRU(uint32_t TagData, uint32_t AssociativityReference = 0xffffffff)
   {
      if (SIZE == 0)
         return;
      array<uint32_t, 4> TagIndividualDataBits = GetTagParameters(TagData);
      uint32_t CacheLruReference = 0;

      if (AssociativityReference == 0xffffffff)
      {
         AssociativityReference = ASSOC - 1;
         for (uint32_t AssociativitySearch = 0; AssociativitySearch < ASSOC; AssociativitySearch++)
         {
            if (((CacheTagAddress[TagIndividualDataBits[IndexB]][AssociativitySearch]) == TagData) && (CacheValidBit[TagIndividualDataBits[IndexB]][AssociativitySearch]))
            {
               AssociativityReference = AssociativitySearch;
               break;
            }
         }
      }

      CacheLruReference = CacheLruBit[TagIndividualDataBits[IndexB]][AssociativityReference];
      for (uint32_t AssociativitySearch = 0; AssociativitySearch < ASSOC; AssociativitySearch++)
      {
         if (CacheLruBit[TagIndividualDataBits[IndexB]][AssociativitySearch] < CacheLruReference)
            CacheLruBit[TagIndividualDataBits[IndexB]][AssociativitySearch]++;
      }
      CacheLruBit[TagIndividualDataBits[IndexB]][AssociativityReference] = 0;
   }

   void CacheOutputDisplay(string L1L2)
   {
      if (SIZE == 0)
         return;

      cout << endl
           << "===== " << L1L2 << " contents =====";

      for (uint32_t SetSearch = 0; SetSearch < NumberOfSets; SetSearch++)
      {
         vector<uint32_t> CacheAssocLruOrder;
         CacheAssocLruOrder.resize(ASSOC);
         for (uint32_t i = 0; i < ASSOC; i++)
         {
            for (uint32_t ii = 0; ii < ASSOC; ii++)
            {
               if (CacheLruBit[SetSearch][ii] == i)
               {
                  CacheAssocLruOrder[i] = ii;
                  break;
               }
            }
         }
         bool InitialFlag = true;
         for (uint32_t AssocSearch = 0; AssocSearch < ASSOC; AssocSearch++)
         {
            uint32_t AssocOrder = CacheAssocLruOrder[AssocSearch];

            if (CacheValidBit[SetSearch][AssocOrder])
            {
               if (InitialFlag)
               {
                  cout << endl
                       << "set      " << to_string(SetSearch) << ": ";
                  InitialFlag = false;
               }
               cout << "  " << IntegerTostring(CacheTag[SetSearch][AssocOrder], !true) << " ";

               if (CacheDirtyBit[SetSearch][AssocOrder])
                  cout << "D";
               else
                  cout << " ";
            }
         }
         if (SetSearch == NumberOfSets - 1)
         {
         }
         // cout << endl;
      }
   }

   bool PrefetchMiss(uint32_t TagData)
   {
      if (SIZE == 0)
         return true;

      array<uint32_t, 4> TagIndividualDataBits = GetTagParameters(TagData);
      uint32_t ReferenceTagAddress = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB]);

      /*for (uint32_t PrefetchN = 0; PrefetchN < PREF_N; PrefetchN++)
      {
         if (((ReferenceTagAddress - PrefetchTagAddress[PrefetchN][0]) < (PREF_M * TagAddressPrefetchOffset)) && (PrefetchValidBit[PrefetchN] == true))
            return false;
      }*/

      for (uint32_t PrefetchN = 0; PrefetchN < PREF_N; PrefetchN++)
      {
         for (uint32_t PrefetchM = 0; PrefetchM < PREF_M; PrefetchM++)
         {
            if (PrefetchTagAddress[PrefetchN][PrefetchM] == ReferenceTagAddress)
               return false;
         }
      }
      return true;
   }

   void UpdatePrefetchContents(uint32_t TagData, bool WriteFlag)
   {
      if ((PREF_N == 0) || (PREF_M == 0))
         return;

      array<uint32_t, 4> TagIndividualDataBits = GetTagParameters(TagData);
      uint32_t CurrentBlockAddress = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB], 0);
      //uint32_t NextBlockAddress = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB] + 1, 0);

      vector<uint32_t> PrefetchLruOrderSearch;
      PrefetchLruOrderSearch.resize(PREF_N);
      for (uint32_t i = 0; i < PREF_N; i++)
      {
         for (uint32_t ii = 0; ii < PREF_N; ii++)
         {
            if (PrefetchLruBit[ii] == i)
            {
               PrefetchLruOrderSearch[i] = ii;
               break;
            }
         }
      }

      for (uint32_t PrefetchNSearch = 0; PrefetchNSearch < PREF_N; PrefetchNSearch++)
      {
         uint32_t NSearchLru = PrefetchLruOrderSearch[PrefetchNSearch];

         // Prefetch Miss and invalid Prefetch Streams available
         if (!(PrefetchValidBit[NSearchLru]))
         {
            PrefetchValidBit[NSearchLru] = true;
            for (uint32_t PrefetchMSearch = 0; PrefetchMSearch < PREF_M; PrefetchMSearch++)
               PrefetchTagAddress[NSearchLru][PrefetchMSearch] = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB] + 1 + PrefetchMSearch);

            UpdatePrefetchLRU(TagData, PrefetchLruBit[NSearchLru]);
            PrefetchesCount += PREF_M;
            MemoryTraffic += PREF_M;
            return;
         }

         // Prefetch Stream Available. update Existing Stream
         for (uint32_t PrefetchMSearch = 0; PrefetchMSearch < PREF_M; PrefetchMSearch++)
         {
            // if ((PrefetchValidBit[NSearchLru]) && (((PrefetchTagAddress[NSearchLru][PrefetchMSearch]) == CurrentBlockAddress) || ((PrefetchTagAddress[NSearchLru][PrefetchMSearch]) == NextBlockAddress)))
            if ((PrefetchValidBit[NSearchLru]) && (((PrefetchTagAddress[NSearchLru][PrefetchMSearch]) == CurrentBlockAddress)))
            {
               for (uint32_t PrefetchMSearch2 = 0; PrefetchMSearch2 < PREF_M; PrefetchMSearch2++)
                  PrefetchTagAddress[NSearchLru][PrefetchMSearch2] = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB] + 1 + 0 + PrefetchMSearch2);

               UpdatePrefetchLRU(TagData, PrefetchLruBit[NSearchLru]);
               PrefetchesCount += (PrefetchMSearch + 1);
               MemoryTraffic += (PrefetchMSearch + 1);
               return;
            }
         }
      }

      // Prefetch Stream Miss. Adding new stream to LRU Stream
      for (uint32_t PrefetchNSearch = 0; PrefetchNSearch < PREF_N; PrefetchNSearch++)
      {
         uint32_t NSearchLru = PrefetchLruOrderSearch[PrefetchNSearch];
         if (PrefetchLruBit[NSearchLru] == PREF_N - 1)
         {
            for (uint32_t PrefetchMSearch = 0; PrefetchMSearch < PREF_M; PrefetchMSearch++)
               PrefetchTagAddress[NSearchLru][PrefetchMSearch] = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB] + 1 + PrefetchMSearch);
            UpdatePrefetchLRU(TagData, PrefetchLruBit[NSearchLru]);
            PrefetchesCount += PREF_M;
            MemoryTraffic += PREF_M;
            return;
         }
      }
   }

   bool UpdatePrefetchLRU(uint32_t TagData, uint32_t PrefetchLruReference = 0xffffffff)
   {
      if (PREF_N == 0)
         return false;

      array<uint32_t, 4> TagIndividualDataBits = GetTagParameters(TagData);
      //uint32_t CurrentBlockAddress = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB]);
      uint32_t NextBlockAddress = TagAddressCalculation(TagIndividualDataBits[TagB], TagIndividualDataBits[IndexB] + 1);

      if (PrefetchLruReference == 0xffffffff)
      {
         PrefetchLruReference = PREF_N - 1;
         for (uint32_t PrefetchNSearch = 0; PrefetchNSearch < PREF_N; PrefetchNSearch++)
         {
            if ((PrefetchValidBit[PrefetchNSearch]) && (((PrefetchTagAddress[PrefetchNSearch][0]) == NextBlockAddress)))
            {
               PrefetchLruReference = PrefetchNSearch;
               break;
            }
         }
      }

      for (uint32_t PrefetchSearchN = 0; PrefetchSearchN < PREF_N; PrefetchSearchN++)
      {
         if (PrefetchLruBit[PrefetchSearchN] < PrefetchLruReference)
            PrefetchLruBit[PrefetchSearchN]++;
         else if (PrefetchLruBit[PrefetchSearchN] == PrefetchLruReference)
            PrefetchLruBit[PrefetchSearchN] = 0;
      }
      return true;
   }

   void StreamBufferDisplay()
   {
      if (((PREF_N == 0) && (SIZE != 0)) || SIZE == 0)
      {
         cout << endl;
         return;
      }

      cout << endl
           << endl
           << "===== Stream Buffer(s) contents =====" << endl;

      vector<uint32_t> PrefetchLruOrderSearch;
      PrefetchLruOrderSearch.resize(PREF_N);
      for (uint32_t i = 0; i < PREF_N; i++)
      {
         for (uint32_t ii = 0; ii < PREF_N; ii++)
         {
            if (PrefetchLruBit[ii] == i)
            {
               PrefetchLruOrderSearch[i] = ii;
               break;
            }
         }
      }
      for (uint32_t PrefetchNSearch = 0; PrefetchNSearch < PREF_N; PrefetchNSearch++)
      {
         uint32_t LruOderSearch = PrefetchLruOrderSearch[PrefetchNSearch];
         if (PrefetchValidBit[LruOderSearch])
         {
            for (uint32_t PrefetchMSearch = 0; PrefetchMSearch < PREF_M; PrefetchMSearch++)
            {
               cout << " " << IntegerTostring(PrefetchTagAddress[LruOderSearch][PrefetchMSearch], !true) << " ";
            }
            cout << endl;
         }
      }
   }
};
