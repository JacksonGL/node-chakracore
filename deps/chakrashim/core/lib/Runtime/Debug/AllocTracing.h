//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_ALLOC_TRACING

#define ALLOC_TRACING_APPROX_STATIC_DATA_SIZE 8

namespace AllocTracing
{
    typedef JsUtil::WeaklyReferencedKeyDictionary<Js::RecyclableObject, bool, RecyclerPointerComparer<const Js::RecyclableObject*>> AllocPinSet;

    //A class that represents a source location -- either an allocation line or a call site in the code
    class SourceLocation
    {
    private:
        char16* m_file;
        uint32 m_line;
        uint32 m_column;

    public:
        SourceLocation(char16* file, uint32 line, uint32 column);
        ~SourceLocation();

        bool SameAsOtherLocation(const char16* file, uint32 line, uint32 column) const;

        //Temp use till we wire in a host provided emitter
        void PrettyPrint() const;
    };

    //A class associated with a single allocation site that contains the statistics for it -- this holds a weak set of all objects allocated at the site
    class AllocSiteStats
    {
    private:
        ThreadContext* m_threadContext;

        size_t m_allocationCount;
        RecyclerRootPtr<AllocPinSet> m_allocationLiveSet;
    public:
        AllocSiteStats(ThreadContext* allocationContext);
        ~AllocSiteStats();

        //When we allocate an object pass it in and update the stats accordingly
        void AddAllocation(Js::RecyclableObject* obj);

        //Simple compute of the memory from this allocation site
        void ComputeMemoryInfo(size_t& liveCount, size_t& liveSize, MemoryAllocWarningFlag& dflag) const;

        void ForceData();

        //Temp use till we wire in a host provided emitter
        void PrettyPrint() const;
    };

    class AllocTracer
    {
    private:
        //A struct that represents a single call on our AllocTracer call stack
        struct AllocCallStackEntry
        {
            Js::FunctionBody* Function;
            uint32 BytecodeIndex;
        };

        JsUtil::List<AllocCallStackEntry, HeapAllocator> m_callStack;

        //A struct that represents a Node in our allocation path tree
        struct AllocPathEntry;
        typedef JsUtil::List<AllocTracer::AllocPathEntry*, HeapAllocator> CallerPathList;

        struct AllocPathEntry
        {
            BOOL IsTerminalStatsEntry;
            SourceLocation* Location;

            union 
            {
                AllocSiteStats* TerminalStats;
                CallerPathList* CallerPaths;
            };
        };

        static void ConvertCallStackEntryToFileLineColumn(const AllocCallStackEntry& sentry, const char16** file, uint32* line, uint32* column);

        static void InitAllocStackEntrySourceLocation(const AllocCallStackEntry& sentry, AllocPathEntry* pentry);

        static AllocPathEntry* CreateTerminalAllocPathEntry(const AllocCallStackEntry& entry, ThreadContext* threadContext);
        static AllocPathEntry* CreateNodeAllocPathEntry(const AllocCallStackEntry& entry);
        static void FreeAllocPathEntry(AllocPathEntry* entry);

        //The roots (starting at the line with the allocation) for the caller trees or each allocation
        JsUtil::List<AllocPathEntry*, HeapAllocator> m_allocPathRoots;

        static bool IsPathInternalCode(const AllocPathEntry* root);

        static AllocPathEntry* ExtendPathTreeForAllocation(const JsUtil::List<AllocCallStackEntry, HeapAllocator>& callStack, int32 position, CallerPathList* currentPaths, ThreadContext* threadContext);
        static void FreeAllocPathTree(AllocPathEntry* root);

        static void ForceData(AllocPathEntry* root);

        static void PrettyPrintIndent(uint32 depth);
        static void PrettyPrintPathEntry(const AllocPathEntry* root, uint32 depth);

    public:
        AllocTracer();
        ~AllocTracer();

        void PushCallStackEntry(Js::FunctionBody* body);
        void PopCallStackEntry();

        void UpdateBytecodeIndex(uint32 index);

        //For convinence we are using the TTD call-stack but later we probably want to use the builtin stack walker
        void AddAllocation(Js::RecyclableObject* obj);

        //Temp use till we wire in a host provided emitter
        void PrettyPrint(ThreadContext* threadContext) const;
    };

    //A class to ensure that even when exceptions are thrown the pop action for the AllocSite call stack is executed
    class AllocSiteExceptionFramePopper
    {
    private:
        AllocTracer* m_tracer;

    public:
        AllocSiteExceptionFramePopper()
            : m_tracer(nullptr)
        {
            ;
        }

        ~AllocSiteExceptionFramePopper()
        {
            //we didn't clear this so an exception was thrown and we are propagating
            if(this->m_tracer != nullptr)
            {
                this->m_tracer->PopCallStackEntry();
            }
        }

        void PushInfo(AllocTracer* tracer)
        {
            this->m_tracer = tracer;
        }
    };
}

#endif
