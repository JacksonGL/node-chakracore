//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_ALLOC_TRACING
#define DO_REPLAY_ALLOC_TRACE(CTX, ALLOC) \
if((CTX)->ShouldPerformReplayAction() && (CTX)->GetThreadContext()->AllocSiteTracer != nullptr) \
{\
    (CTX)->GetThreadContext()->AllocSiteTracer->AddAllocation(ALLOC);\
}
#else
#define DO_REPLAY_ALLOC_TRACE(CTX, ALLOC)
#endif

#if ENABLE_ALLOC_TRACING

#define ALLOC_TRACING_STATIC_SIZE_DEFAULT 8
#define ALLOC_TRACING_DYNAMIC_SIZE_DEFAULT 32
#define ALLOC_TRACING_DYNAMIC_ENTRY_SIZE sizeof(Js::Var)

#define ALLOC_TRACING_INTERESTING_LOCATION_COUNT_THRESHOLD 0.01
#define ALLOC_TRACING_INTERESTING_LOCATION_SIZE_THRESHOLD 0.01

namespace AllocTracing
{
    //A class for JSON emitting our alloc tracing data
    class AllocDataWriter
    {
    private:
        void WriteChar_Internal(char16 c);
        void WriteChars_Internal(const char* data, size_t length);
        void WriteChar16s_Internal(const char16* data, size_t length);

    public:
        AllocDataWriter();
        ~AllocDataWriter();

        void WriteObjectId(Js::RecyclableObject* value);
        void WriteInt(int64 value);
        void WriteChar(char16 c);
        void WriteLiteralString(const char* str);
        void WriteString(const char16* str, size_t length);
    };

    typedef JsUtil::WeaklyReferencedKeyDictionary<Js::RecyclableObject, bool, RecyclerPointerComparer<const Js::RecyclableObject*>> AllocPinSet;

    //A class that represents a source location -- either an allocation line or a call site in the code
    class SourceLocation
    {
    private:
        Js::FunctionBody* m_function;
        uint32 m_line;
        uint32 m_column;

    public:
        SourceLocation(Js::FunctionBody* function, uint32 line, uint32 column);

        bool SameAsOtherLocation(const Js::FunctionBody* function, uint32 line, uint32 column) const;

        void JSONWriteLocationData(AllocDataWriter& writer) const;
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

        void AddAllocation(Js::RecyclableObject* obj);

        void ForceData();
        void EstimateMemoryUseInfo(size_t& liveCount, size_t& liveSize) const;

        void JSONWriteSiteData(AllocDataWriter& writer) const;
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

        static bool IsInternalLocation(const AllocCallStackEntry& callEntry);

        JsUtil::List<AllocCallStackEntry, HeapAllocator> m_callStack;
        JsUtil::List<AllocCallStackEntry, HeapAllocator> m_prunedCallStack;

        //A struct that represents a Node in our allocation path tree
        struct AllocPathEntry;
        typedef JsUtil::List<AllocTracer::AllocPathEntry*, HeapAllocator> CallerPathList;

        struct AllocPathEntry
        {
            BOOL IsTerminalStatsEntry;
            SourceLocation* Location;

            size_t LiveCount;
            size_t LiveSizeEstimate;
            BOOL IsInterestingSite;

            union
            {
                AllocSiteStats* TerminalStats;
                CallerPathList* CallerPaths;
            };
        };

        static void ExtractLineColumn(const AllocCallStackEntry& sentry, uint32* line, uint32* column);

        static void InitAllocStackEntrySourceLocation(const AllocCallStackEntry& sentry, AllocPathEntry* pentry);

        static AllocPathEntry* CreateTerminalAllocPathEntry(const AllocCallStackEntry& entry, ThreadContext* threadContext);
        static AllocPathEntry* CreateNodeAllocPathEntry(const AllocCallStackEntry& entry);
        static void FreeAllocPathEntry(AllocPathEntry* entry);

        //The roots (starting at the line with the allocation) for the caller trees or each allocation
        JsUtil::List<AllocPathEntry*, HeapAllocator> m_allocPathRoots;

        static AllocPathEntry* ExtendPathTreeForAllocation(const JsUtil::List<AllocCallStackEntry, HeapAllocator>& callStack, int32 position, CallerPathList* currentPaths, ThreadContext* threadContext);
        static void FreeAllocPathTree(AllocPathEntry* root);

        static void ForceAllData(AllocPathEntry* root);
        static void EstimateMemoryUseInfo(AllocPathEntry* root);
        static void FlagInterestingSites(AllocPathEntry* root, size_t countThreshold, size_t estimatedSizeThreshold);

        static void JSONWriteDataIndent(AllocDataWriter& writer, uint32 depth);
        static void JSONWriteDataPathEntry(AllocDataWriter& writer, const AllocPathEntry* root, uint32 depth);

    public:
        AllocTracer();
        ~AllocTracer();

        void PushCallStackEntry(Js::FunctionBody* body);
        void PopCallStackEntry();

        void UpdateBytecodeIndex(uint32 index);

        void AddAllocation(Js::RecyclableObject* obj);

        void ForceAllData();
        void JSONWriteData(AllocDataWriter& writer) const;
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
