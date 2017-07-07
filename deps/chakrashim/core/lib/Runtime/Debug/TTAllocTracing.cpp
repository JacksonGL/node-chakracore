//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_ALLOC_TRACING

using namespace TTD;
namespace AllocTracing
{
    void AllocDataWriter::WriteChar_Internal(char16 c)
    {
        putchar((char)c);
    }

    void AllocDataWriter::WriteChars_Internal(const char* data, size_t length)
    {
        for(size_t i = 0; i < length; ++i)
        {
            putchar(data[i]);
        }
    }

    void AllocDataWriter::WriteChar16s_Internal(const char16* data, size_t length)
    {
        for(size_t i = 0; i < length; ++i)
        {
            putchar((char)data[i]);
        }
    }

    AllocDataWriter::AllocDataWriter()
    {
        ;
    }

    AllocDataWriter::~AllocDataWriter()
    {
        ;
    }

    void AllocDataWriter::WriteObjectId(Js::RecyclableObject* value)
    {
        char trgtBuff[64];
        int writtenChars = sprintf_s(trgtBuff, "\"*%I64u\"", reinterpret_cast<uint64>(value));
        TTDAssert(writtenChars != -1 && writtenChars < 64, "Formatting failed or result is too big.");

        this->WriteChars_Internal(trgtBuff, writtenChars);
    }

    void AllocDataWriter::WriteInt(int64 value)
    {
        char trgtBuff[64];
        int writtenChars = sprintf_s(trgtBuff, "%I64i", value);
        TTDAssert(writtenChars != -1 && writtenChars < 64, "Formatting failed or result is too big.");

        this->WriteChars_Internal(trgtBuff, writtenChars);
    }

    void AllocDataWriter::WriteChar(char16 c)
    {
        this->WriteChar_Internal(c);
    }

    void AllocDataWriter::WriteLiteralString(const char* str)
    {
        this->WriteChars_Internal(str, strlen(str));
    }

    void AllocDataWriter::WriteString(const char16* str, size_t length)
    {
        this->WriteChar16s_Internal(str, wcslen(str));
    }

    SourceLocation::SourceLocation(Js::FunctionBody* function, uint32 line, uint32 column)
        : m_function(function), m_line(line), m_column(column)
    {
        ;
    }

    bool SourceLocation::SameAsOtherLocation(const Js::FunctionBody* function, uint32 line, uint32 column) const
    {
        if((this->m_line != line) | (this->m_column != column))
        {
            return false;
        }

        return (wcscmp(this->m_function->GetSourceContextInfo()->url, function->GetSourceContextInfo()->url) == 0);
    }

    void SourceLocation::JSONWriteLocationData(AllocDataWriter& writer) const
    {
        writer.WriteLiteralString("\"src\": { ");
        writer.WriteLiteralString("\"function\": \"");
        writer.WriteString(this->m_function->GetDisplayName(), this->m_function->GetDisplayNameLength());
        writer.WriteLiteralString("\", \"line\": ");
        writer.WriteInt(this->m_line + 1);
        writer.WriteLiteralString(", \"column\": ");
        writer.WriteInt(this->m_column);
        writer.WriteLiteralString(" }");
    }


    FileSourceEntry::FileSourceEntry() {
        this->filename = nullptr;
        this->source = nullptr;
    }

    FileSourceEntry::FileSourceEntry(const char16* filename, Js::Utf8SourceInfo* utf8SourceInfo) {
        // copy the file name
        if (filename != nullptr) { // filename could be nullptr
            char16* name = (char16*)malloc((wcslen(filename) + 1) * sizeof(char16));
            wcscpy_s(name, (wcslen(filename) + 1), filename);
            this->filename = name;
        }
        // copy the source code
        LPCUTF8 source = utf8SourceInfo->GetSource();
        int32 cchLength = utf8SourceInfo->GetCchLength();
        size_t cbLength = utf8SourceInfo->GetCbLength();
        char16* buffer = (char16*)malloc((cchLength + 1) * 4 * sizeof(char16));
        utf8::DecodeOptions options = utf8SourceInfo->IsCesu8() ? utf8::doAllowThreeByteSurrogates : utf8::doDefault;
        utf8::DecodeUnitsIntoAndNullTerminate(buffer, source, source + cbLength, options);
        this->source = buffer;
    }
    /*
    FileSourceEntry::~FileSourceEntry() {
        if (this->filename != nullptr) {
            delete[](this->filename);
        }
        if (this->source != nullptr) {
            delete[](this->source);
        }
    }
    */

    // intitialize the (static) file-to-source list variable
    JsUtil::List<FileSourceEntry, HeapAllocator> SourceLocation::m_file_to_source_list(&HeapAllocator::Instance);

    uint32 SourceLocation::addSourceItem(const char16* filename, Js::Utf8SourceInfo* utf8SourceInfo) {
        if (filename == nullptr) return 0;
        // first search for the entry
        int length = m_file_to_source_list.Count();
        for (int i = 0; i < length; i++) {
            FileSourceEntry* ptr = &(m_file_to_source_list.Item(i));
            // if (ptr->filename != filename || ptr->source != source) continue;
            if (memcmp(ptr->filename, filename, wcslen(filename) * 2)) continue;
            return i + 1;
        }
        // if not found, add it to the list
        FileSourceEntry entry(filename, utf8SourceInfo);
        SourceLocation::m_file_to_source_list.Add(entry);
        return length + 1;
    }

    void SourceLocation::clearSourceItems() {
        // first search for the entry
        m_file_to_source_list.Clear();
        // m_file_to_source_list.Reset();
    }

    void SourceLocation::JSONWriteFileToSourceList(TextFormatWriter& writer, NSTokens::Separator sep) {
        writer.WriteSequenceStartWithKey(NSTokens::Key::fileToSourceMap, sep);

        writer.AdjustIndent(1);
        int length = m_file_to_source_list.Count();
        for (int i = 0; i < length; i++) {
            if (i == 0) writer.WriteRecordStart();
            else writer.WriteRecordStart(NSTokens::Separator::CommaAndBigSpaceSeparator);

            FileSourceEntry* ptr = &(m_file_to_source_list.Item(i));

            writer.WriteUInt32(NSTokens::Key::fileId, i+1);
            writer.writeRawCharsWithKey(NSTokens::Key::filename, ptr->filename, NSTokens::Separator::CommaSeparator);
            // delete[](ptr->filename); // free the copied string

            // write the source code
            writer.writeRawCharsWithKey(NSTokens::Key::source, ptr->source, NSTokens::Separator::CommaSeparator);
            // delete[](ptr->source); // free the copied string

            writer.WriteRecordEnd();
        }
        writer.AdjustIndent(-1);
        writer.WriteSequenceEnd();
    }

    void SourceLocation::JSONWriteLocationDataTrimed(TextFormatWriter& writer) const
    {
        writer.WriteRecordStartWithKey(NSTokens::Key::src, NSTokens::Separator::BigSpaceSeparator);
        writer.writeRawCharsWithKey(NSTokens::Key::function, this->m_function->GetDisplayName(), NSTokens::Separator::NoSeparator);
        writer.WriteInt64(NSTokens::Key::line, this->m_line + 1, NSTokens::Separator::CommaSeparator);
        writer.WriteInt64(NSTokens::Key::column, this->m_column, NSTokens::Separator::CommaSeparator);
        // writer.writeRawCharsWithKey(NSTokens::Key::filename, this->m_function->GetSourceContextInfo()->url, NSTokens::Separator::CommaSeparator);
        uint32 fileId = this->addSourceItem(this->m_function->GetSourceContextInfo()->url, this->m_function->GetUtf8SourceInfo());
        writer.WriteUInt32(NSTokens::Key::fileId, fileId, NSTokens::Separator::CommaSeparator);
        writer.WriteRecordEnd();
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
        this->m_allocationCount++;
        this->m_allocationLiveSet->Add(obj, true);
    }

    void AllocSiteStats::ForceData()
    {
        this->m_allocationLiveSet->Map([&](Js::RecyclableObject* key, bool, const RecyclerWeakReference<Js::RecyclableObject>*)
        {
            if(Js::JavascriptString::Is(key))
            {
                //Force the string to serialize to a flat representation so we can easily measure how much memory it uses.
                Js::JavascriptString::FromVar(key)->GetSz();
            }
        });
    }

    void AllocSiteStats::EstimateMemoryUseInfo(size_t& liveCount, size_t& liveSize) const
    {
        this->m_allocationLiveSet->Map([&](Js::RecyclableObject* key, bool, const RecyclerWeakReference<Js::RecyclableObject>*)
        {
            size_t osize = 0;
            Js::TypeId tid = key->GetTypeId();
            if(Js::StaticType::Is(tid))
            {
                osize = ALLOC_TRACING_STATIC_SIZE_DEFAULT;
                if(tid == Js::TypeIds_String)
                {
                    osize += Js::JavascriptString::FromVar(key)->GetLength() * sizeof(char16);
                }
            }
            else
            {
                osize = ALLOC_TRACING_DYNAMIC_SIZE_DEFAULT + (key->GetPropertyCount() * ALLOC_TRACING_DYNAMIC_ENTRY_SIZE);

                //TODO: add v-call for arrays etc to add to the size estimate
            }

            liveCount++;
            liveSize += osize;
        });
    }

    void AllocSiteStats::JSONWriteSiteData(AllocDataWriter& writer) const
    {
        bool first = true;
        writer.WriteLiteralString("\"objectIds\": [ ");
        this->m_allocationLiveSet->Map([&](Js::RecyclableObject* key, bool, const RecyclerWeakReference<Js::RecyclableObject>*)
        {
            if(!first)
            {
                writer.WriteLiteralString(", ");
            }
            first = false;
            writer.WriteObjectId(key);
        });
        writer.WriteLiteralString(" ]");
    }

    void AllocSiteStats::JSONWriteSiteDataTrimed(TTD::TextFormatWriter& writer) const
    {
        bool first = true;
        writer.WriteSequenceStartWithKey(NSTokens::Key::objectIds, NSTokens::Separator::CommaAndBigSpaceSeparator);
        this->m_allocationLiveSet->Map([&](Js::RecyclableObject* key, bool, const RecyclerWeakReference<Js::RecyclableObject>*)
        {
            if (!first)
            {
                writer.WriteSeperator(NSTokens::Separator::CommaSeparator);
            }
            first = false;
            writer.WriteNakedAddrAsInt64(TTD_CONVERT_VAR_TO_PTR_ID(key));
            // writer.WriteNakedInt64(reinterpret_cast<uint64>(key));
        });
        writer.WriteSequenceEnd();
    }

    bool AllocTracer::IsInternalLocation(const AllocCallStackEntry& callEntry)
    {
        if(callEntry.Function->GetSourceContextInfo() == nullptr || callEntry.Function->GetSourceContextInfo()->url == nullptr)
        {
            return true;
        }

        const char16* url = callEntry.Function->GetSourceContextInfo()->url;
#ifdef _WIN32
        return (wcslen(url) <= 1 || (url[0] != _u('\\') && url[1] != _u(':')));
#else
        return (wcslen(url) <= 1 || (url[0] != _u('/') && url[1] != _u(':')));
#endif
    }

    void AllocTracer::ExtractLineColumn(const AllocCallStackEntry& sentry, uint32* line, uint32* column)
    {
        *line = 0;
        *column = 0;

        if(sentry.Function->GetUtf8SourceInfo()->GetSourceContextInfo()->url != nullptr)
        {
            ULONG sline = 0;
            LONG scolumn = 0;

            int32 cIndex = sentry.Function->GetEnclosingStatementIndexFromByteCode(sentry.BytecodeIndex);
            uint32 startOffset = sentry.Function->GetStatementStartOffset(cIndex);
            sentry.Function->GetLineCharOffsetFromStartChar(startOffset, &sline, &scolumn);

            *line = (uint32)sline;
            *column = (uint32)scolumn;
        }
    }

    void AllocTracer::InitAllocStackEntrySourceLocation(const AllocCallStackEntry& sentry, AllocPathEntry* pentry)
    {
        uint32 line = 0;
        uint32 column = 0;
        AllocTracer::ExtractLineColumn(sentry, &line, &column);

        pentry->Location = HeapNew(SourceLocation, sentry.Function, line, column);
    }

    AllocTracer::AllocPathEntry* AllocTracer::CreateTerminalAllocPathEntry(const AllocCallStackEntry& entry, ThreadContext* threadContext)
    {
        AllocPathEntry* res = HeapNewStruct(AllocPathEntry);
        AllocTracer::InitAllocStackEntrySourceLocation(entry, res);

        res->LiveCount = 0;
        res->LiveSizeEstimate = 0;
        res->IsInterestingSite = FALSE; 

        res->IsTerminalStatsEntry = TRUE;
        res->TerminalStats = HeapNew(AllocSiteStats, threadContext);

        return res;
    }

    AllocTracer::AllocPathEntry* AllocTracer::CreateNodeAllocPathEntry(const AllocCallStackEntry& entry)
    {
        AllocPathEntry* res = HeapNewStruct(AllocPathEntry);
        AllocTracer::InitAllocStackEntrySourceLocation(entry, res);

        res->LiveCount = 0;
        res->LiveSizeEstimate = 0;
        res->IsInterestingSite = FALSE;

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

    AllocTracer::AllocPathEntry* AllocTracer::ExtendPathTreeForAllocation(const JsUtil::List<AllocCallStackEntry, HeapAllocator>& callStack, int32 position, CallerPathList* currentPaths, ThreadContext* threadContext)
    {
        const AllocCallStackEntry& currStack = callStack.Item(position);
        const Js::FunctionBody* cbody = currStack.Function;

        uint32 line = 0;
        uint32 column = 0;
        AllocTracer::ExtractLineColumn(currStack, &line, &column);

        // only create node for the top of the call stack
        /*
        for (int32 i = 0; i < currentPaths->Count(); ++i)
        {
            AllocPathEntry* currPath = currentPaths->Item(i);
            if (currPath->IsTerminalStatsEntry && currPath->Location->SameAsOtherLocation(cbody, line, column))
            {
                return currPath;
            }
        }
        */
        int64 key = (int64)(cbody + column * 1000000 + line);
        if (currentPaths->ContainsKey(key)) {
            return currentPaths->Item(key);
        }

        //no suitable entry found so create a new one
        AllocPathEntry* tentry = AllocTracer::CreateTerminalAllocPathEntry(currStack, threadContext);
        currentPaths->Add(key, tentry);
        return tentry;

        /*
        //If we are at the top of the call stack (bottom of the allocation path tree) get the stats entry
        if(position == 0)
        {
            for(int32 i = 0; i < currentPaths->Count(); ++i)
            {
                AllocPathEntry* currPath = currentPaths->Item(i);
                if(currPath->IsTerminalStatsEntry && currPath->Location->SameAsOtherLocation(cbody, line, column))
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
                if(!currPath->IsTerminalStatsEntry && currPath->Location->SameAsOtherLocation(cbody, line, column))
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
        */
    }

    void AllocTracer::FreeAllocPathTree(AllocPathEntry* root)
    {
        if(!root->IsTerminalStatsEntry)
        {
            /*
            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocTracer::FreeAllocPathTree(root->CallerPaths->Item(i));
            }
            root->CallerPaths->Clear();
            */

            root->CallerPaths->EachValue([](AllocPathEntry* entry)
            {
                AllocTracer::FreeAllocPathTree(entry);
            });
            root->CallerPaths->Clear();
        }

        AllocTracer::FreeAllocPathEntry(root);
    }

    void AllocTracer::ForceAllData(AllocPathEntry* root)
    {
        if(root->IsTerminalStatsEntry)
        {
            root->TerminalStats->ForceData();
        }
        else
        {
            /*
            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocPathEntry* cpe = root->CallerPaths->Item(i);
                AllocTracer::ForceAllData(cpe);
            }
            */
            root->CallerPaths->EachValue([](AllocPathEntry* entry)
            {
                AllocTracer::ForceAllData(entry);
            });
        }
    }

    void AllocTracer::EstimateMemoryUseInfo(AllocPathEntry* root)
    {
        if(root->IsTerminalStatsEntry)
        {
            root->TerminalStats->EstimateMemoryUseInfo(root->LiveCount, root->LiveSizeEstimate);
        }
        else
        {
            /*
            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocPathEntry* cpe = root->CallerPaths->Item(i);
                AllocTracer::EstimateMemoryUseInfo(cpe);

                root->LiveCount += cpe->LiveCount;
                root->LiveSizeEstimate += cpe->LiveSizeEstimate;
            }
            */
            root->CallerPaths->EachValue([&root](AllocPathEntry* cpe)
            {
                AllocTracer::EstimateMemoryUseInfo(cpe);

                root->LiveCount += cpe->LiveCount;
                root->LiveSizeEstimate += cpe->LiveSizeEstimate;
            });
        }
    }

    void AllocTracer::FlagInterestingSites(AllocPathEntry* root, size_t countThreshold, size_t estimatedSizeThreshold)
    {
        if(root->IsTerminalStatsEntry)
        {
            root->IsInterestingSite = (root->LiveCount >= countThreshold) | (root->LiveSizeEstimate >= estimatedSizeThreshold);
        }
        else
        {
            /*
            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocPathEntry* cpe = root->CallerPaths->Item(i);
                AllocTracer::FlagInterestingSites(cpe, countThreshold, estimatedSizeThreshold);

                root->IsInterestingSite |= cpe->IsInterestingSite;
            }
            */
            root->CallerPaths->EachValue([&countThreshold, &estimatedSizeThreshold, &root](AllocPathEntry* cpe)
            {
                AllocTracer::FlagInterestingSites(cpe, countThreshold, estimatedSizeThreshold);

                root->IsInterestingSite |= cpe->IsInterestingSite;
            });
        }
    }

    void AllocTracer::JSONWriteDataIndent(AllocDataWriter& writer, uint32 depth)
    {
        for(uint32 i = 0; i < depth; ++i)
        {
            writer.WriteChar(' ');
            writer.WriteChar(' ');
        }
    }

    void AllocTracer::JSONWriteDataPathEntry(AllocDataWriter& writer, const AllocPathEntry* root, uint32 depth)
    {
        AssertMsg(root->IsInterestingSite, "Check this before trying to write!!!");
        uint32 localdepth = depth + 1;

        AllocTracer::JSONWriteDataIndent(writer, depth);
        writer.WriteLiteralString("{\n");

        AllocTracer::JSONWriteDataIndent(writer, localdepth);
        root->Location->JSONWriteLocationData(writer);
        writer.WriteLiteralString(",\n");

        AllocTracer::JSONWriteDataIndent(writer, localdepth);
        writer.WriteLiteralString("\"allocInfo\": { \"count\": ");
        writer.WriteInt(root->LiveCount);
        writer.WriteLiteralString(", \"estimatedSize\": ");
        writer.WriteInt(root->LiveSizeEstimate);
        writer.WriteLiteralString(" },\n");

        AllocTracer::JSONWriteDataIndent(writer, localdepth);
        if(root->IsTerminalStatsEntry)
        {
            root->TerminalStats->JSONWriteSiteData(writer);
        }
        else
        {
            writer.WriteLiteralString("\"subPaths\": [");

            bool first = true;
            uint32 nesteddepth = localdepth + 1;
            /*
            for(int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocPathEntry* cpe = root->CallerPaths->Item(i);
                if(cpe->IsInterestingSite)
                {
                    if(!first)
                    {
                        writer.WriteChar(',');
                    }
                    first = false;

                    writer.WriteChar('\n');
                    AllocTracer::JSONWriteDataPathEntry(writer, cpe, nesteddepth);
                }
            }
            */

            root->CallerPaths->EachValue([&first, &writer, &nesteddepth](AllocPathEntry* cpe)
            {
                if (cpe->IsInterestingSite)
                {
                    if (!first)
                    {
                        writer.WriteChar(',');
                    }
                    first = false;

                    writer.WriteChar('\n');
                    AllocTracer::JSONWriteDataPathEntry(writer, cpe, nesteddepth);
                }
            });

            writer.WriteChar('\n');
            AllocTracer::JSONWriteDataIndent(writer, localdepth);
            writer.WriteChar(']');
        }
        writer.WriteChar('\n');
        AllocTracer::JSONWriteDataIndent(writer, depth);
        writer.WriteLiteralString("}");
    }


    void AllocTracer::JSONWriteDataPathEntryTrimed(TextFormatWriter& writer, const AllocPathEntry* root, uint32 depth)
    {
        AssertMsg(root->IsInterestingSite, "Check this before trying to write!!!");
        if (root->LiveCount <= 0) return;
        uint32 localdepth = depth + 1;

        writer.WriteRecordStart();
        root->Location->JSONWriteLocationDataTrimed(writer);
        writer.WriteSeperator(NSTokens::Separator::CommaAndBigSpaceSeparator);

        writer.AdjustIndent(1);
        writer.WriteRecordStartWithKey(NSTokens::Key::allocInfo);
        writer.WriteInt64(NSTokens::Key::count, root->LiveCount);
        writer.WriteInt64(NSTokens::Key::estimatedSize, root->LiveSizeEstimate, NSTokens::Separator::CommaSeparator);
        writer.WriteRecordEnd();
        writer.AdjustIndent(-1);
        if (root->IsTerminalStatsEntry)
        {
            root->TerminalStats->JSONWriteSiteDataTrimed(writer);
        }
        else
        {
            writer.WriteSequenceStartWithKey(NSTokens::Key::subPaths, NSTokens::Separator::CommaAndBigSpaceSeparator);
            writer.AdjustIndent(1);

            bool first = true;
            uint32 nesteddepth = localdepth + 1;
            /*
            for (int32 i = 0; i < root->CallerPaths->Count(); ++i)
            {
                AllocPathEntry* cpe = root->CallerPaths->Item(i);
                if (cpe->IsInterestingSite)
                {
                    if (!first)
                    {
                        writer.WriteSeperator(NSTokens::Separator::CommaSeparator);
                    }
                    first = false;

                    writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
                    AllocTracer::JSONWriteDataPathEntryTrimed(writer, cpe, nesteddepth);
                }
            }
            */

            root->CallerPaths->EachValue([&first, &writer, &nesteddepth](AllocPathEntry* cpe)
            {
                if (cpe->IsInterestingSite)
                {
                    if (!first)
                    {
                        writer.WriteSeperator(NSTokens::Separator::CommaSeparator);
                    }
                    first = false;

                    writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
                    AllocTracer::JSONWriteDataPathEntryTrimed(writer, cpe, nesteddepth);
                }
            });
            writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);

            writer.AdjustIndent(-1);
            writer.WriteSequenceEnd();
        }
        writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
        writer.WriteRecordEnd();
    }

    AllocTracer::AllocTracer()
        : m_callStack(&HeapAllocator::Instance), m_prunedCallStack(&HeapAllocator::Instance), m_allocPathRoots(&HeapAllocator::Instance)
    {
        ;
    }

    AllocTracer::~AllocTracer()
    {
        /*
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocTracer::FreeAllocPathTree(this->m_allocPathRoots.Item(i));
        }
        this->m_allocPathRoots.Clear();
        */
        
        this->m_allocPathRoots.EachValue([](AllocPathEntry* entry)
        {
            AllocTracer::FreeAllocPathTree(entry);
        });
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

    int AllocTracer::count = 0;

    void AllocTracer::AddAllocation(Js::RecyclableObject* obj)
    {
        /*
        for(int32 i = 0; i < this->m_callStack.Count(); ++i)
        {
            const AllocCallStackEntry& ase = this->m_callStack.Item(i);
            if(!AllocTracer::IsInternalLocation(ase))
            {
                this->m_prunedCallStack.Add(ase);
            }
        }
        */
        for (int32 i = this->m_callStack.Count() - 1; i >=0 ; --i)
        {
            const AllocCallStackEntry& ase = this->m_callStack.Item(i);
            if (!AllocTracer::IsInternalLocation(ase))
            {
                this->m_prunedCallStack.Add(ase);
                break;
            }
        }
        //For now we skip Host driven allocations
        if(this->m_prunedCallStack.Count() == 0)
        {
            return;
        }

        AllocPathEntry* tentry = AllocTracer::ExtendPathTreeForAllocation(this->m_prunedCallStack, this->m_prunedCallStack.Count() - 1, &this->m_allocPathRoots, obj->GetScriptContext()->GetThreadContext());
        AssertMsg(tentry->IsTerminalStatsEntry, "Something went wrong in the tree expansion");

        tentry->TerminalStats->AddAllocation(obj);

        this->m_prunedCallStack.Clear();
        this->m_prunedCallStack.Reset();
    }

    void AllocTracer::ForceAllData()
    {
        /*
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocPathEntry* cpe = this->m_allocPathRoots.Item(i);
            AllocTracer::ForceAllData(cpe);
        }
        */
        this->m_allocPathRoots.EachValue([this](AllocPathEntry* entry)
        {
            AllocTracer::ForceAllData(entry);
        });
    }

    void AllocTracer::EmitTrimedAllocTrace(int64 snapId, ThreadContext* threadContext) const
    {
        char asciiResourceName[64];
        sprintf_s(asciiResourceName, 64, "allocTracing_%I64i.json", snapId);

        TTD::TTDataIOInfo& iofp = threadContext->TTDContext->TTDataIOInfo;
        TTD::JsTTDStreamHandle traceHandle = iofp.pfOpenResourceStream(iofp.ActiveTTUriLength, iofp.ActiveTTUri, strlen(asciiResourceName), asciiResourceName, false, true);
        TTDAssert(traceHandle != nullptr, "Failed to open snapshot resource stream for writing.");

        TextFormatWriter writer(traceHandle, iofp.pfWriteBytesToStream, iofp.pfFlushAndCloseStream);
        writer.setQuotedKey(true);

        size_t totalLive = 0;
        size_t totalSizeEstimate = 0;
        /*
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
        AllocPathEntry* cpe = this->m_allocPathRoots.Item(i);
        AllocTracer::EstimateMemoryUseInfo(cpe);

        totalLive += cpe->LiveCount;
        totalSizeEstimate += cpe->LiveSizeEstimate;
        }
        */

        this->m_allocPathRoots.EachValue([&totalLive, &totalSizeEstimate](AllocPathEntry* cpe)
        {
            AllocTracer::EstimateMemoryUseInfo(cpe);

            totalLive += cpe->LiveCount;
            totalSizeEstimate += cpe->LiveSizeEstimate;
        });

        size_t countThreshold = (size_t)(totalLive * ALLOC_TRACING_INTERESTING_LOCATION_COUNT_THRESHOLD);
        size_t estimatedSizeThreshold = (size_t)(totalSizeEstimate * ALLOC_TRACING_INTERESTING_LOCATION_SIZE_THRESHOLD);

        /*
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
        AllocPathEntry* cpe = this->m_allocPathRoots.Item(i);
        AllocTracer::FlagInterestingSites(cpe, countThreshold, estimatedSizeThreshold);
        }
        */

        this->m_allocPathRoots.EachValue([&countThreshold, &estimatedSizeThreshold](AllocPathEntry* cpe)
        {
            AllocTracer::FlagInterestingSites(cpe, countThreshold, estimatedSizeThreshold);
        });

        bool first = true;
        writer.WriteRecordStart();
        writer.AdjustIndent(1);
        writer.WriteSequenceStartWithKey(NSTokens::Key::allocations, NSTokens::Separator::BigSpaceSeparator);

        writer.AdjustIndent(1);

        /*
        for(int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
        AllocPathEntry* cpe = this->m_allocPathRoots.Item(i);
        if(cpe->IsInterestingSite && cpe->LiveCount > 0)
        {
        if(!first)
        {
        writer.WriteSeperator(NSTokens::Separator::CommaSeparator);
        }
        first = false;
        writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
        AllocTracer::JSONWriteDataPathEntryTrimed(writer, cpe, 1);
        }
        }
        */
        this->m_allocPathRoots.EachValue([&first, &writer](AllocPathEntry* cpe)
        {
            if (cpe->IsInterestingSite && cpe->LiveCount > 0)
            {
                if (!first)
                {
                    writer.WriteSeperator(NSTokens::Separator::CommaSeparator);
                }
                first = false;
                writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
                AllocTracer::JSONWriteDataPathEntryTrimed(writer, cpe, 1);
            }
        });

        writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
        writer.AdjustIndent(-1);
        writer.WriteSequenceEnd();

        SourceLocation::JSONWriteFileToSourceList(writer, NSTokens::Separator::CommaAndBigSpaceSeparator);
        SourceLocation::clearSourceItems();

        writer.AdjustIndent(-1);
        writer.WriteRecordEnd();

        writer.FlushAndClose();
    }


    void AllocTracer::JSONWriteData(AllocDataWriter& writer) const
    {
        /*
        size_t totalLive = 0;
        size_t totalSizeEstimate = 0;
        for (int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocPathEntry* cpe = this->m_allocPathRoots.Item(i);
            AllocTracer::EstimateMemoryUseInfo(cpe);

            totalLive += cpe->LiveCount;
            totalSizeEstimate += cpe->LiveSizeEstimate;
        }

        size_t countThreshold = (size_t)(totalLive * ALLOC_TRACING_INTERESTING_LOCATION_COUNT_THRESHOLD);
        size_t estimatedSizeThreshold = (size_t)(totalSizeEstimate * ALLOC_TRACING_INTERESTING_LOCATION_SIZE_THRESHOLD);
        for (int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocPathEntry* cpe = this->m_allocPathRoots.Item(i);
            AllocTracer::FlagInterestingSites(cpe, countThreshold, estimatedSizeThreshold);
        }

        bool first = true;
        writer.WriteChar('[');
        for (int32 i = 0; i < this->m_allocPathRoots.Count(); ++i)
        {
            AllocPathEntry* cpe = this->m_allocPathRoots.Item(i);
            if (cpe->IsInterestingSite)
            {
                if (!first)
                {
                    writer.WriteChar(',');
                }
                first = false;

                writer.WriteChar('\n');
                AllocTracer::JSONWriteDataPathEntry(writer, cpe, 1);
            }
        }
        writer.WriteChar('\n');
        writer.WriteChar(']');
        */
    }
}

#endif
