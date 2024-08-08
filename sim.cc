#include "sim.h"
#include <iostream>
#include <array>
#include <string>
#include <fstream>
using namespace std;

string InputTraceFileNameString = "";
uint32_t InputBlockSize = 0;
uint32_t InputL1Size = 0;
uint32_t InputL1Assoc = 0;
uint32_t InputL2Size = 0;
uint32_t InputL2Assoc = 0;
uint32_t InputL1PrefetchN = 0;
uint32_t InputL1PrefetchM = 0;
uint32_t InputL2PrefetchN = 0;
uint32_t InputL2PrefetchM = 0;

int main(int ArgumentCount, char *ArgumentVariables[])
{
   FILE *TraceFilePointer;
   char TraceFileReadWrite;
   uint32_t AddressFromTrace;
   char *InputTraceFileName;

   if (ArgumentCount != 9)
   {
      cout << "Error: Expected 8 command-line arguments but was provided " << ArgumentCount - 1 << '\n';
      exit(EXIT_FAILURE);
   }

   InputBlockSize = (uint32_t)atoi(ArgumentVariables[1]);
   InputL1Size = (uint32_t)atoi(ArgumentVariables[2]);
   InputL1Assoc = (uint32_t)atoi(ArgumentVariables[3]);
   InputL2Size = (uint32_t)atoi(ArgumentVariables[4]);
   InputL2Assoc = (uint32_t)atoi(ArgumentVariables[5]);

   if (InputBlockSize % 2)
   {
      cout << "Error: Input Block Size not a power of 2 " << InputBlockSize << '\n';
      exit(EXIT_FAILURE);
   }

   if (InputL2Size == 0)
   {
      InputL1PrefetchN = (uint32_t)atoi(ArgumentVariables[6]);
      InputL1PrefetchM = (uint32_t)atoi(ArgumentVariables[7]);
   }
   else
   {
      InputL2PrefetchN = (uint32_t)atoi(ArgumentVariables[6]);
      InputL2PrefetchM = (uint32_t)atoi(ArgumentVariables[7]);
   }
   InputTraceFileName = ArgumentVariables[8];
   InputTraceFileNameString = ArgumentVariables[8];

   bool ReadWriteCacheSubroutine(uint32_t, bool, CacheModule &, CacheModule &, CacheModule &);
   void CacheSimulatorFinalData(CacheModule, CacheModule);

   CacheModule L1(InputBlockSize, InputL1Size, InputL1Assoc, InputL1PrefetchN, InputL1PrefetchM);
   CacheModule L2(InputBlockSize, InputL2Size, InputL2Assoc, InputL2PrefetchN, InputL2PrefetchM);
   CacheModule MEMORY(InputBlockSize, 0, 0, 0, 0);

   TraceFilePointer = fopen(InputTraceFileName, "r");
   if (TraceFilePointer == (FILE *)NULL)
      exit(EXIT_FAILURE);

   while (fscanf(TraceFilePointer, "%c %x\n", &TraceFileReadWrite, &AddressFromTrace) == 2)
   {
      if (TraceFileReadWrite == 'r')
         ReadWriteCacheSubroutine(AddressFromTrace, false, L1, L2, MEMORY);
      else if (TraceFileReadWrite == 'w')
         ReadWriteCacheSubroutine(AddressFromTrace, true, L1, L2, MEMORY);
      else
      {
         cout << "Error: Unknown request type" << TraceFileReadWrite;
         exit(EXIT_FAILURE);
      }
   }

   CacheSimulatorFinalData(L1, L2);
   return (0);
}

bool ReadWriteCacheSubroutine(uint32_t TagAddress, bool WriteFlag, CacheModule &L1, CacheModule &L2, CacheModule &MEMORY)
{
   if (WriteFlag)
      L1.WriteCount += 1;
   else
      L1.ReadCount += 1;

   if (L1.CacheMiss(TagAddress))
   {
      if (L1.PrefetchMiss(TagAddress))
      {
         if (WriteFlag)
            L1.WriteMissCount += 1;
         else
            L1.ReadMissCount += 1;

         if (L2.SIZE == 0)
         {
            L1.MemoryTraffic += 1;
            L1.CacheDirtyBitEviction(TagAddress, MEMORY, MEMORY);
            L1.UpdateCacheContents(TagAddress, WriteFlag, MEMORY, MEMORY, true); // L1 Scenario #1
         }
         else
         {
            L2.ReadCount += 1;
            if (L2.CacheMiss(TagAddress))
            {
               if (L2.PrefetchMiss(TagAddress))
               {
                  L2.ReadMissCount += 1;
                  L2.MemoryTraffic += 1;
                  L1.CacheDirtyBitEviction(TagAddress, L2, MEMORY);
                  L2.UpdateCacheContents(TagAddress, false, MEMORY, MEMORY, true); // L2 Scenario #1
                  L1.UpdateCacheContents(TagAddress, WriteFlag, L2, MEMORY, true); // L2 Scenario #1 OK
               }
               else
               {
                  // L2.PrefetchesCount += 1;
                  L1.CacheDirtyBitEviction(TagAddress, L2, MEMORY);
                  L2.UpdateCacheContents(TagAddress, false, MEMORY, MEMORY, true); // L2 Scenario #2
                  L1.UpdateCacheContents(TagAddress, WriteFlag, L2, MEMORY, true); // L2 Scenario #2 OK
               }
            }
            else // L2 Hit
            {
               if (L2.PrefetchMiss(TagAddress))
               {
                  L1.CacheDirtyBitEviction(TagAddress, L2, MEMORY);
                  L2.UpdateCacheContents(TagAddress, false, MEMORY, MEMORY, false); // L2 Scenario #3 OK
                  L1.UpdateCacheContents(TagAddress, WriteFlag, L2, MEMORY, true);  // L2 Scenario #3 OK
               }
               else
               {
                  L1.CacheDirtyBitEviction(TagAddress, L2, MEMORY);
                  L2.UpdateCacheContents(TagAddress, false, MEMORY, MEMORY, true); // L2 Scenario #4
                  L1.UpdateCacheContents(TagAddress, WriteFlag, L2, MEMORY, true); // L2 Scenario #4 OK
               }
            }
         }
      }
      else
      {
         // L1.PrefetchesCount += 1;
         L1.CacheDirtyBitEviction(TagAddress, L2, MEMORY);
         L1.UpdateCacheContents(TagAddress, WriteFlag, L2, MEMORY, true); // L1 Scenario #2
      }
   }
   else // L1 HIT
   {
      if (L1.PrefetchMiss(TagAddress))
         L1.UpdateCacheContents(TagAddress, WriteFlag, L2, MEMORY, false); // L1 Scenario #3 OK
      else
         L1.UpdateCacheContents(TagAddress, WriteFlag, L2, MEMORY, true); // L1 Scenario #4
   }
   return true;
}

void CacheSimulatorFinalData(CacheModule L1, CacheModule L2)
{
   cout << "===== Simulator configuration =====" << endl;
   cout << "BLOCKSIZE:  " << to_string(L1.BLOCKSIZE) << endl;
   cout << "L1_SIZE:    " << to_string(L1.SIZE) << endl;
   cout << "L1_ASSOC:   " << to_string(L1.ASSOC) << endl;
   cout << "L2_SIZE:    " << to_string(L2.SIZE) << endl;
   cout << "L2_ASSOC:   " << to_string(L2.ASSOC) << endl;
   cout << "PREF_N:     " << to_string(L1.PREF_N + L2.PREF_N) << endl;
   cout << "PREF_M:     " << to_string(L1.PREF_M + L2.PREF_M) << endl;

   cout << "trace_file: " + InputTraceFileNameString << endl;

   L1.CacheOutputDisplay("L1");
   L1.StreamBufferDisplay();
   L2.CacheOutputDisplay("L2");
   L2.StreamBufferDisplay();

   // if ((L2.SIZE != 0) && L2.PREF_N == 0)
   if (L2.SIZE != 0)
      cout << endl;

   cout << "===== Measurements =====" << endl;
   cout << "a. L1 reads:                   " << to_string(L1.ReadCount) << endl;
   cout << "b. L1 read misses:             " << to_string(L1.ReadMissCount) << endl;
   cout << "c. L1 writes:                  " << to_string(L1.WriteCount) << endl;
   cout << "d. L1 write misses:            " << to_string(L1.WriteMissCount) << endl;
   cout << "e. L1 miss rate:               " << fixed << setprecision(4) << (double(L1.ReadMissCount + L1.WriteMissCount) / double(L1.ReadCount + L1.WriteCount)) << endl;
   // printf("e. L1 miss rate:               %.4f\n", (double(L1.ReadMissCount + L1.WriteMissCount) / double(L1.ReadCount + L1.WriteCount)));
   cout << "f. L1 writebacks:              " << to_string(L1.WriteBackCount) << endl;
   cout << "g. L1 prefetches:              " << to_string(L1.PrefetchesCount) << endl;

   cout << "h. L2 reads (demand):          " << to_string(L2.ReadCount) << endl;
   cout << "i. L2 read misses (demand):    " << to_string(L2.ReadMissCount) << endl;
   cout << "j. L2 reads (prefetch):        " << to_string(L2.ReadPrefetchCount) << endl;
   cout << "k. L2 read misses (prefetch):  " << to_string(L2.ReadMissPrefetchCount) << endl;
   cout << "l. L2 writes:                  " << to_string(L2.WriteCount) << endl;
   cout << "m. L2 write misses:            " << to_string(L2.WriteMissCount) << endl;
   if ((L2.ReadCount) == 0)
   {
      cout << "n. L2 miss rate:               " << fixed << setprecision(4) << (double(0)) << endl;
      // printf("e. L2 miss rate:               %.4f\n", (double(0)));
   }
   else
   {
      cout << "n. L2 miss rate:               " << fixed << setprecision(4) << ((double(L2.ReadMissCount) / double(L2.ReadCount))) << endl;
      // printf("e. L2 miss rate:               %.4f\n", ((double(L2.ReadMissCount) / double(L2.ReadCount))));
   }
   cout << "o. L2 writebacks:              " << to_string(L2.WriteBackCount) << endl;
   cout << "p. L2 prefetches:              " << to_string(L2.PrefetchesCount) << endl;
   cout << "q. memory traffic:             " << to_string(L1.MemoryTraffic + L2.MemoryTraffic) << endl;
}
