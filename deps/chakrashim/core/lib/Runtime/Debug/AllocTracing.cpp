//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_ALLOC_TRACING

namespace AllocTracing
{
    SourceLocation::SourceLocation(char16* file, uint32 line, uint32 column)
        : m_file(file), m_line(line), m_column(column)
    {
        ;
    }
    SourceLocation::~SourceLocation()
    {
        if(this->m_file != nullptr)
        {
            HeapDeleteArray(wcslen(this->m_file) + 1, this->m_file);
            this->m_file = nullptr;
        }
    }

    bool SourceLocation::SameAsOtherLocation(const char16* file, uint32 line, uint32 column) const
    {
        if(this->m_line != line || this->m_column != column)
        {
            return false;
        }

        return wcscmp(this->m_file, file) == 0;
    }

    void SourceLocation::PrettyPrint() const
    {
        printf("\"src\": { \"file\": \"%ls\", \"line\": %u, \"column\": %u }", this->m_file, this->m_line + 1, this->m_column); //add 1 for 0 vs 1 indexing in line numbers
    }

    AllocSiteStats::AllocSiteStats(ThreadContext* allocationContext)
        : m_threadContext(allocationContext), m_allocationCount(0), m_allocationLiveSet()
    {
        Recycler* recycler = this->m_threadContext->GetRecycler();
        this->m_allocationLiveSet.Root(RecyclerNew(recycler, AllocPinSet, recycler), recycler);
    }

    AllocSiteStats::~AllocSiteStats()
    {
        if(this->m_allocationLiveSet != nullptr)
        {
            this->m_allocationLiveSet.Unroot(this->m_threadContext->GetRecycler());
        }
    }

    void AllocSiteStats::AddAllocation(Js::RecyclableObject* obj)
    {
        AssertMsg(this->m_threadContext->GetRecycler()->IsAllocTrackable(obj), "Why is this not trackable in the recycler!!!");

        this->m_allocationCount++;
        this->m_allocationLiveSet->Add(obj, true);
    }

    void AllocSiteStats::ComputeMemoryInfo(size_t& liveCount, size_t& liveSize, MemoryAllocWarningFlag& dflag) const
    {
        MemoryAllocWarningFlag tflag = MemoryAllocWarningFlag::None;
        size_t regSize = 0;
        size_t flaggedSize = 0;
        this->m_allocationLiveSet->Map([&](Js::RecyclableObject* key, bool, const RecyclerWeakReference<Js::RecyclableObject>*)
        {
            MemoryAllocWarningFlag mflag = MemoryAllocWarningFlag::None;
            size_t osize = key->ComputeAllocTracingInfo(mflag);
            if(Js::DynamicType::Is(key->GetTypeId()))
            {
                osize += Js::DynamicObject::FromVar(key)->ComputeObjPropertyAllocTracingInfo(mflag);
            }

            if(mflag == MemoryAllocWarningFlag::None)
            {
                regSize += osize;
            }
            else
            {
                tflag |= mflag;
                flaggedSize += osize;
            }

            liveCount++;
            liveSize += osize;
        });

        if(flaggedSize >= regSize / 2)
        {
            dflag |= tflag;
        }
    }

    void AllocSiteStats::ForceData()
    {
        this->m_allocationLiveSet->Map([&](Js::RecyclableObject* key, bool, const RecyclerWeakReference<Js::RecyclableObject>*)
        {
            if(Js::JavascriptString::Is(key))
            {
                Js::JavascriptString::FromVar(key)->GetSz();
            }
        });
    }

    void AllocSiteStats::PrettyPrint() const
    {
        printf("\"site\": { ");

        printf("\"allocationCount\": %zd, ", this->m_allocationCount);

        size_t liveCount = 0;
        size_t liveSize = 0;
        MemoryAllocWarningFlag dflag = MemoryAllocWarningFlag::None;
        this->ComputeMemoryInfo(liveCount, liveSize, dflag);

        printf("\"liveCount\": %zd, \"liveSize\": %zd", liveCount, liveSize);
        if(dflag != MemoryAllocWarningFlag::None)
        {
            printf(", \"flags\": [ ");
            bool first = true;
            if((dflag & MemoryAllocWarningFlag::LowDataContentObject) != MemoryAllocWarningFlag::None)
            {
                if(!first)
                {
                    printf(", ");
                }
                first = false;

                printf("\"LowDataContentObject\"");
            }

            if((dflag & MemoryAllocWarningFlag::LowDataContentArrayObject) != MemoryAllocWarningFlag::None)
            {
                if(!first)
                {
                    printf(", ");
                }
                first = false;

                printf("\"LowDataArray\"");
            }

            if((dflag & MemoryAllocWarningFlag::SparseArrayObject) != MemoryAllocWarningFlag::None)
            {
                if(!first)
                {
                    printf(", ");
                }
                first = false;

                printf("\"SparseDataArray\"");
            }

            if((dflag & MemoryAllocWarningFlag::LowDataContentContainerObject) != MemoryAllocWarningFlag::None)
            {
                if(!first)
                {
                    printf(", ");
                }
                first = false;

                printf("\"LowDataSetOrMap\"");
            }

            printf(" ]");
        }

        printf(" }");
    }

    void AllocTracer::ConvertCallStackEntryToFileLineColumn(const AllocCallStackEntry& sentry, const char16** file, uint32* line, uint32* column)
    {
        *line = 0;
        *column = 0;

        *file = sentry.Function->GetUtf8SourceInfo()->GetSourceContextInfo()->url;
        if(*file == nullptr)
        {
            *file = _u("#internalcode#");
        }
        else
        {
            ULONG sline = 0;
            LONG scolumn = -1;

            int32 cIndex = sentry.Function->GetEnclosingStatementIndexFromByteCode(sentry.BytecodeIndex);
            uint32 startOffset = sentry.Function->GetStatementStartOffset(cIndex);
            sentry.Function->GetLineCharOffsetFromStartChar(startOffset, &sline, &scolumn);

            *line = (uint32)sline;
            *column = (uint32)scolumn;
        }
    }

    void AllocTracer::InitAllocStackEntrySourceLocation(const AllocCallStackEntry& sentry, AllocPathEntry* pentry)
    {
        const char16* efile = nullptr;
        uint32 line = 0;
        uint32 column = 0;
        AllocTracer::ConvertCallStackEntryToFileLineColumn(sentry, &efile, &line, &column);

        size_t efileLength = wcslen(efile);

        char16* file = HeapNewArray(char16, efileLength + 1);
        js_memcpy_s(file, efileLength * sizeof(char16), efile, efileLength * sizeof(char16));
        file[efileLength] = _u('\0');

        pentry->Location = HeapNew(SourceLocation, file, line, column);
    }

    AllocTracer::AllocPathEntry* AllocTracer::CreateTerminalAllocPathEntry(const AllocCallStackEntry& entry, ThreadContext* threadContext)
    {
        AllocPathEntry* res = HeapNewStruct(AllocPathEntry);
        AllocTracer::InitAllocStackEntrySourceLocation(entry, res);

        res->IsTerminalStatsEntry = TRUE;
        res->TerminalStats = HeapNew(AllocSiteStats, threadContext);

        return res;
    }

    AllocTracer::AllocPathEntry* AllocTracer::CreateNodeAllocPathEntry(const AllocCallStackEntry& entry)
    {
        AllocPathEntry* res = HeapNewStruct(AllocPathEntry);
        AllocTracer::InitAllocStackEntrySourceLocation(entry, res);

        res->IsTerminalStatsEntry = FALSE;
        res->CallerPaths = HeapNew(CallerPathList, &HeapAllocator::Instance);

        return res;
    }

    void AllocTracer::FreeAllocPathEntry(AllocPathEntry* entry)
    {
        HeapDelete(entry->Location);

        if(entry->IsTerminalStatsEntry)
        {
            HeapDelete(entry->TerminalStats);
        }
        else
        {
            HeapDelete(entry->CallerPaths);
        }

        HeapDelete(entry);
    }

    bool AllocTracer::IsPathInternalCode(const AllocPathEntry* root)
    {
        return root->Location->SameAsOtherLocation(_u("#internalcode#"), 0, 0);
    }

    AllocTracer::AllocPathEntry* AllocTracer::ExtendPathTreeForAllocation(const JsUtil::List<AllocCallStackEntry, HeapAllocator>& callStack, int32 position, CallerPathList* currentPaths, ThreadContext* threadContext)
    {
        const AllocCallStackEntry& currStack = callStack.Item(position);

        const char16* file = nullptr;
        uint32 line = 0;
        uint32 column = 0;
        AllocTracer::ConvertCallStackEntryToFileLineColumn(currStack, &file, &line, &column);

        //If we are at the top of the call stack (bottom of the allocation path tree) get the stats entry
        if(position == 0)
        {
            for(int32 i = 0; i < currentPaths->Count(); ++i)
            {
                AllocPathEntry* currPath = currentPaths->Item(i);
                if(currPath->IsTerminalStatsEntry && currPath->Location->SameAsOtherLocation(file, line, column))
                {
                    return currPath;
                }
            }

            //no suitable entry found so create a new one
            AllocPathEntry* tentry = AllocTracer::CreateTerminalAllocPathEntry(currStack, threadContext);
            currentPaths->Add(tentry);
            return tentry;
        }
        else
        {
            //find (or create) the path node and continue expanding the tree
            AllocPathEntry* eentry = nullptr;

            for(int32 i = 0; i < currentPaths->Count(); ++i)
            {
                AllocPathEntry* currPath = currentPaths->Item(i);
                if(!currPath->IsTerminalStatsEntry && currPath->Location->SameAsOtherLocation(file, line, column))
                {
                    eentry = currPath;
                    break;
                }
            }

            if(eentry == nullptr)
            {
                eentry = AllocTracer::CreateNodeAllocPathEntry(currStack);
                currentPaths->Add(eentry);
            }

            //make resucrsive call
            return AllocTracer::ExtendPathTreeForAllocation(callStack, position - 1, eentry->CallerPaths, threadContext);
        }
    }

    void AllocTracer::FreeAllocPathTree(AllocPathEntry* root)
    {
        if(!root->IsTerminalStatsEntry)
        {
            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocTracer::FreeAllocPathTree(root->CallerPaths->Item(i));
            }
            root->CallerPaths->Clear();
        }

        AllocTracer::FreeAllocPathEntry(root);
    }

    void AllocTracer::ForceData(AllocPathEntry* root)
    {
        if(root->IsTerminalStatsEntry)
        {
            root->TerminalStats->ForceData();
        }
        else
        {
            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocPathEntry* curr = root->CallerPaths->Item(i);
                AllocTracer::ForceData(curr);
            }
        }
    }

    void AllocTracer::PrettyPrintIndent(uint32 depth)
    {
        for(uint32 i = 0; i < depth; ++i)
        {
            printf("  ");
        }
    }

    void AllocTracer::PrettyPrintPathEntry(const AllocPathEntry* root, uint32 depth)
    {
        AllocTracer::PrettyPrintIndent(depth);
        printf("{ ");
        root->Location->PrettyPrint();
        printf(", ");

        if(root->IsTerminalStatsEntry)
        {
            root->TerminalStats->PrettyPrint();
            printf(" }");
        }
        else
        {
            printf("\"callPaths\": [\n");

            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                if(i != 0)
                {
                    printf(",");
                    printf("\n");
                }

                const AllocPathEntry* curr = root->CallerPaths->Item(i);
                AllocTracer::PrettyPrintPathEntry(curr, depth + 1);
            }

            printf("\n");
            AllocTracer::PrettyPrintIndent(depth + 1);
            printf("]");

            printf("\n");
            AllocTracer::PrettyPrintIndent(depth);
            printf("}");
        }
    }

    AllocTracer::AllocTracer()
        : m_callStack(&HeapAllocator::Instance), m_allocPathRoots(&HeapAllocator::Instance)
    {
        ;
    }

    AllocTracer::~AllocTracer()
    {
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocTracer::FreeAllocPathTree(this->m_allocPathRoots.Item(i));
        }
        this->m_allocPathRoots.Clear();
    }

    void AllocTracer::PushCallStackEntry(Js::FunctionBody* body)
    {
        AllocCallStackEntry entry{ body, 0 };
        this->m_callStack.Add(entry);
    }

    void AllocTracer::PopCallStackEntry()
    {
        AssertMsg(this->m_callStack.Count() != 0, "Underflow");

        this->m_callStack.RemoveAtEnd();
    }

    void AllocTracer::UpdateBytecodeIndex(uint32 index)
    {
        AssertMsg(this->m_callStack.Count() != 0, "Underflow");

        this->m_callStack.Last().BytecodeIndex = index;
    }

    void AllocTracer::AddAllocation(Js::RecyclableObject* obj)
    {
        //This code is being driven by the host so (at least for now) we will ignore the allocation -- maybe add a special host category later
        if(this->m_callStack.Count() == 0)
        {
            return;
        }

        if(!obj->GetScriptContext()->GetRecycler()->IsAllocTrackable(obj))
        {
            return;
        }

        AllocPathEntry* tentry = AllocTracer::ExtendPathTreeForAllocation(this->m_callStack, this->m_callStack.Count() - 1, &this->m_allocPathRoots, obj->GetScriptContext()->GetThreadContext());
        AssertMsg(tentry->IsTerminalStatsEntry, "Something went wrong in the tree expansion");

        tentry->TerminalStats->AddAllocation(obj);
    }

    //Temp use till we wire in a host provided emitter
    void AllocTracer::PrettyPrint(ThreadContext* threadContext) const
    {
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocPathEntry* pentry = this->m_allocPathRoots.Item(i);
            AllocTracer::ForceData(pentry);
        }

        //Is this the best to ensure our weak sets are cleaned before we count below???
        threadContext->GetRecycler()->CollectNow<CollectNowExhaustive>();

        printf("[ ");
        bool first = true;
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocPathEntry* pentry = this->m_allocPathRoots.Item(i);
            if(!AllocTracer::IsPathInternalCode(pentry))
            {
                if(!first)
                {
                    printf(", ");
                }
                first = false;

                printf("\n");

                AllocTracer::PrettyPrintPathEntry(pentry, 1);
            }
        }
        printf(" ]");
    }
}

#endif
