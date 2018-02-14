// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "db_file.h"
#include <zen/guid.h>
#include <zen/crc.h>
#include <wx+/zlib_wrap.h>


using namespace zen;
using namespace fff;


namespace
{
//-------------------------------------------------------------------------------------------------------------------------------
const char FILE_FORMAT_DESCR[] = "FreeFileSync";
const int DB_FORMAT_CONTAINER = 10; //since 2017-02-01
const int DB_FORMAT_STREAM    =  3; //
//-------------------------------------------------------------------------------------------------------------------------------

struct SessionData
{
    bool isLeadStream = false;
    ByteArray rawStream;
};
bool operator==(const SessionData& lhs, const SessionData& rhs) { return lhs.isLeadStream == rhs.isLeadStream && lhs.rawStream == rhs.rawStream; }

using UniqueId  = std::string;
using DbStreams = std::map<UniqueId, SessionData>; //list of streams ordered by session UUID

/*------------------------------------------------------------------------------
  | ensure 32/64 bit portability: use fixed size data types only e.g. uint32_t |
  ------------------------------------------------------------------------------*/

template <SelectedSide side> inline
AbstractPath getDatabaseFilePath(const BaseFolderPair& baseFolder, bool tempfile = false)
{
    //Linux and Windows builds are binary incompatible: different file id?, problem with case sensitivity?
    //precomposed/decomposed UTF? are UTC file times really compatible? what about endianess!?
    //however 32 and 64-bit FreeFileSync are designed to produce binary-identical db files!
    //Give db files different names.
    const Zstring dbName = Zstr(".sync"); //files beginning with dots are hidden e.g. in Nautilus
    Zstring dbFileName;
    if (tempfile) //generate (hopefully) unique file name to avoid clashing with some remnant ffs_tmp file
    {
        const Zstring shortGuid = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));
        dbFileName = dbName + Zchar('.') + shortGuid + AFS::TEMP_FILE_ENDING;
    }
    else
        dbFileName = dbName + SYNC_DB_FILE_ENDING;

    return AFS::appendRelPath(baseFolder.getAbstractPath<side>(), dbFileName);
}

//#######################################################################################################################################

void saveStreams(const DbStreams& streamList, const AbstractPath& dbPath, const IOCallback& notifyUnbufferedIO) //throw FileError
{
    const std::unique_ptr<AFS::OutputStream> fileStreamOut = AFS::getOutputStream(dbPath, //throw FileError
                                                                                  nullptr /*streamSize*/,
                                                                                  notifyUnbufferedIO /*throw X*/);
    //write FreeFileSync file identifier
    writeArray(*fileStreamOut, FILE_FORMAT_DESCR, sizeof(FILE_FORMAT_DESCR)); //throw FileError, X

    //save file format version
    writeNumber<int32_t>(*fileStreamOut, DB_FORMAT_CONTAINER); //throw FileError, X

    //write stream list
    writeNumber<uint32_t>(*fileStreamOut, static_cast<uint32_t>(streamList.size())); //throw FileError, X

    for (const auto& stream : streamList)
    {
        writeContainer<std::string>(*fileStreamOut, stream.first ); //throw FileError, X

        writeNumber   <int8_t   >(*fileStreamOut, stream.second.isLeadStream); //
        writeContainer<ByteArray>(*fileStreamOut, stream.second.rawStream); //
    }

    //commit and close stream:
    fileStreamOut->finalize(); //throw FileError, X

}


DbStreams loadStreams(const AbstractPath& dbPath, const IOCallback& notifyUnbufferedIO) //throw FileError, FileErrorDatabaseNotExisting
{
    try
    {
        const std::unique_ptr<AFS::InputStream> fileStreamIn = AFS::getInputStream(dbPath, notifyUnbufferedIO); //throw FileError, ErrorFileLocked, X

        //read FreeFileSync file identifier
        char formatDescr[sizeof(FILE_FORMAT_DESCR)] = {};
        readArray(*fileStreamIn, formatDescr, sizeof(formatDescr)); //throw FileError, ErrorFileLocked, X, UnexpectedEndOfStreamError

        if (!std::equal(FILE_FORMAT_DESCR, FILE_FORMAT_DESCR + sizeof(FILE_FORMAT_DESCR), formatDescr))
            throw FileError(replaceCpy(_("Database file %x is incompatible."), L"%x", fmtPath(AFS::getDisplayPath(dbPath))));

        const int version = readNumber<int32_t>(*fileStreamIn); //throw FileError, ErrorFileLocked, X, UnexpectedEndOfStreamError

        //TODO: remove migration code at some time! 2017-02-01
        if (version != 9 &&
            version != DB_FORMAT_CONTAINER) //read file format version number
            throw FileError(replaceCpy(_("Database file %x is incompatible."), L"%x", fmtPath(AFS::getDisplayPath(dbPath))));

        DbStreams output;

        //read stream list
        size_t dbCount = readNumber<uint32_t>(*fileStreamIn); //throw FileError, ErrorFileLocked, X, UnexpectedEndOfStreamError
        while (dbCount-- != 0)
        {
            //DB id of partner databases
            std::string sessionID = readContainer<std::string>(*fileStreamIn); //throw FileError, ErrorFileLocked, X, UnexpectedEndOfStreamError

            SessionData sessionData = {};

            //TODO: remove migration code at some time! 2017-02-01
            if (version == 9)
            {
                sessionData.rawStream = readContainer<ByteArray>(*fileStreamIn); //throw FileError, ErrorFileLocked, X, UnexpectedEndOfStreamError

                MemoryStreamIn<ByteArray> streamIn(sessionData.rawStream);
                const int streamVersion = readNumber<int32_t>(streamIn); //throw UnexpectedEndOfStreamError
                if (streamVersion != 2) //don't throw here due to old stream formats
                    continue;
                sessionData.isLeadStream = readNumber<int8_t>(streamIn) != 0; //throw FileError, X, UnexpectedEndOfStreamError
            }
            else
            {
                sessionData.isLeadStream = readNumber<int8_t>(*fileStreamIn) != 0;  //throw FileError, ErrorFileLocked, X, UnexpectedEndOfStreamError
                sessionData.rawStream    = readContainer<ByteArray>(*fileStreamIn); //throw FileError, ErrorFileLocked, X, UnexpectedEndOfStreamError
            }

            output[sessionID] = std::move(sessionData);
        }
        return output;
    }
    catch (FileError&)
    {
        bool dbNotYetExisting = false;
        try { dbNotYetExisting = !AFS::getItemTypeIfExists(dbPath); /*throw FileError*/ }
        catch (FileError&) {} //previous exception is more relevant

        if (dbNotYetExisting) //throw FileError
            throw FileErrorDatabaseNotExisting(_("Initial synchronization:") + L" \n" +
                                               replaceCpy(_("Database file %x does not yet exist."), L"%x", fmtPath(AFS::getDisplayPath(dbPath))));
        else
            throw;
    }
    catch (UnexpectedEndOfStreamError&)
    {
        throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(AFS::getDisplayPath(dbPath)), L"Unexpected end of stream.");
    }
    catch (const std::bad_alloc& e) //still required?
    {
        throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(AFS::getDisplayPath(dbPath)),
                        _("Out of memory.") + L" " + utfTo<std::wstring>(e.what()));
    }
}

//#######################################################################################################################################

class StreamGenerator
{
public:
    static void execute(const InSyncFolder& dbFolder, //throw FileError
                        const std::wstring& displayFilePathL, //used for diagnostics only
                        const std::wstring& displayFilePathR,
                        ByteArray& streamL,
                        ByteArray& streamR)
    {
        MemoryStreamOut<ByteArray> outL;
        MemoryStreamOut<ByteArray> outR;
        //save format version
        writeNumber<int32_t>(outL, DB_FORMAT_STREAM);
        writeNumber<int32_t>(outR, DB_FORMAT_STREAM);

        auto compStream = [&](const ByteArray& stream) -> ByteArray //throw FileError
        {
            try
            {
                /* Zlib: optimal level - testcase 1 million files
                level|size [MB]|time [ms]
                  0    49.54      272 (uncompressed)
                  1    14.53     1013
                  2    14.13     1106
                  3    13.76     1288 - best compromise between speed and compression
                  4    13.20     1526
                  5    12.73     1916
                  6    12.58     2765
                  7    12.54     3633
                  8    12.51     9032
                  9    12.50    19698 (maximal compression) */
                return compress(stream, 3); //throw ZlibInternalError
            }
            catch (ZlibInternalError&)
            {
                throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(displayFilePathL + L"/" + displayFilePathR)), L"zlib internal error");
            }
        };

        StreamGenerator generator;
        //PERF_START
        generator.recurse(dbFolder);
        //PERF_STOP

        const ByteArray bufText     = compStream(generator.streamOutText_    .ref());
        const ByteArray bufSmallNum = compStream(generator.streamOutSmallNum_.ref());
        const ByteArray bufBigNum   = compStream(generator.streamOutBigNum_  .ref());

        MemoryStreamOut<ByteArray> streamOut;
        writeContainer(streamOut, bufText);
        writeContainer(streamOut, bufSmallNum);
        writeContainer(streamOut, bufBigNum);

        const ByteArray& buf = streamOut.ref();

        //distribute "outputBoth" over left and right streams:
        const size_t size1stPart = buf.size() / 2;
        const size_t size2ndPart = buf.size() - size1stPart;

        writeNumber<uint64_t>(outL, size1stPart);
        writeNumber<uint64_t>(outR, size2ndPart);

        if (size1stPart > 0) writeArray(outL, &*buf.begin(), size1stPart);
        if (size2ndPart > 0) writeArray(outR, &*buf.begin() + size1stPart, size2ndPart);

        streamL = outL.ref();
        streamR = outR.ref();
    }

private:
    void recurse(const InSyncFolder& container)
    {
        writeNumber<uint32_t>(streamOutSmallNum_, static_cast<uint32_t>(container.files.size()));
        for (const auto& dbFile : container.files)
        {
            writeUtf8(streamOutText_, dbFile.first);
            writeNumber(streamOutSmallNum_, static_cast<int32_t>(dbFile.second.cmpVar));
            writeNumber<uint64_t>(streamOutSmallNum_, dbFile.second.fileSize);

            writeFileDescr(streamOutBigNum_, dbFile.second.left);
            writeFileDescr(streamOutBigNum_, dbFile.second.right);
        }

        writeNumber<uint32_t>(streamOutSmallNum_, static_cast<uint32_t>(container.symlinks.size()));
        for (const auto& dbSymlink : container.symlinks)
        {
            writeUtf8(streamOutText_, dbSymlink.first);
            writeNumber(streamOutSmallNum_, static_cast<int32_t>(dbSymlink.second.cmpVar));

            writeLinkDescr(streamOutBigNum_, dbSymlink.second.left);
            writeLinkDescr(streamOutBigNum_, dbSymlink.second.right);
        }

        writeNumber<uint32_t>(streamOutSmallNum_, static_cast<uint32_t>(container.folders.size()));
        for (const auto& dbFolder : container.folders)
        {
            writeUtf8(streamOutText_, dbFolder.first);
            writeNumber<int32_t>(streamOutSmallNum_, dbFolder.second.status);

            recurse(dbFolder.second);
        }
    }

    static void writeUtf8(MemoryStreamOut<ByteArray>& streamOut, const Zstring& str) { writeContainer(streamOut, utfTo<Zbase<char>>(str)); }

    static void writeFileDescr(MemoryStreamOut<ByteArray>& streamOut, const InSyncDescrFile& descr)
    {
        writeNumber<std::int64_t>(streamOut, descr.modTime);
        writeContainer(streamOut, descr.fileId);
        static_assert(IsSameType<decltype(descr.fileId), Zbase<char>>::value, "");
    }

    static void writeLinkDescr(MemoryStreamOut<ByteArray>& streamOut, const InSyncDescrLink& descr)
    {
        writeNumber<int64_t>(streamOut, descr.modTime);
    }

    //maximize zlib compression by grouping similar data (=> 20% size reduction!)
    // -> further ~5% reduction possible by having one container per data type
    MemoryStreamOut<ByteArray> streamOutText_;     //
    MemoryStreamOut<ByteArray> streamOutSmallNum_; //data with bias to lead side (= always left in this context)
    MemoryStreamOut<ByteArray> streamOutBigNum_;   //
};


class StreamParser
{
public:
    static std::shared_ptr<InSyncFolder> execute(bool leadStreamLeft, //throw FileError
                                                 const ByteArray& streamL,
                                                 const ByteArray& streamR,
                                                 const std::wstring& displayFilePathL, //used for diagnostics only
                                                 const std::wstring& displayFilePathR)
    {
        auto decompStream = [&](const ByteArray& stream) -> ByteArray //throw FileError
        {
            try
            {
                return decompress(stream); //throw ZlibInternalError
            }
            catch (ZlibInternalError&)
            {
                throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(displayFilePathL + L"/" + displayFilePathR)), L"Zlib internal error");
            }
        };

        try
        {
            MemoryStreamIn<ByteArray> streamInL(streamL);
            MemoryStreamIn<ByteArray> streamInR(streamR);

            const int streamVersion  = readNumber<int32_t>(streamInL); //throw UnexpectedEndOfStreamError
            const int streamVersionR = readNumber<int32_t>(streamInR); //throw UnexpectedEndOfStreamError

            if (streamVersion != streamVersionR)
                throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(displayFilePathL) + L"\n" + fmtPath(displayFilePathR), L"Different stream formats");

            //TODO: remove migration code at some time! 2017-02-01
            if (streamVersion != 2 &&
                streamVersion != DB_FORMAT_STREAM)
                throw FileError(replaceCpy(_("Database file %x is incompatible."), L"%x", fmtPath(displayFilePathL)), L"Unknown stream format");

            //TODO: remove migration code at some time! 2017-02-01
            if (streamVersion == 2)
            {
                const bool has1stPartL = readNumber<int8_t>(streamInL) != 0; //throw UnexpectedEndOfStreamError
                const bool has1stPartR = readNumber<int8_t>(streamInR) != 0; //

                if (has1stPartL == has1stPartR)
                    throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(displayFilePathL) + L"\n" + fmtPath(displayFilePathR), L"Second stream part missing");
                if (has1stPartL != leadStreamLeft)
                    throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(displayFilePathL) + L"\n" + fmtPath(displayFilePathR), L"has1stPartL != leadStreamLeft");

                MemoryStreamIn<ByteArray>& in1stPart = leadStreamLeft ? streamInL : streamInR;
                MemoryStreamIn<ByteArray>& in2ndPart = leadStreamLeft ? streamInR : streamInL;

                const size_t size1stPart = static_cast<size_t>(readNumber<uint64_t>(in1stPart));
                const size_t size2ndPart = static_cast<size_t>(readNumber<uint64_t>(in2ndPart));

                ByteArray tmpB;
                tmpB.resize(size1stPart + size2ndPart); //throw bad_alloc
                readArray(in1stPart, &*tmpB.begin(),               size1stPart); //stream always non-empty
                readArray(in2ndPart, &*tmpB.begin() + size1stPart, size2ndPart); //throw UnexpectedEndOfStreamError

                const ByteArray tmpL = readContainer<ByteArray>(streamInL);
                const ByteArray tmpR = readContainer<ByteArray>(streamInR);

                auto output = std::make_shared<InSyncFolder>(InSyncFolder::DIR_STATUS_IN_SYNC);
                StreamParserV2 parser(decompStream(tmpL),
                                      decompStream(tmpR),
                                      decompStream(tmpB));
                parser.recurse(*output); //throw UnexpectedEndOfStreamError
                return output;
            }
            else
            {
                MemoryStreamIn<ByteArray>& streamInPart1 = leadStreamLeft ? streamInL : streamInR;
                MemoryStreamIn<ByteArray>& streamInPart2 = leadStreamLeft ? streamInR : streamInL;

                const size_t sizePart1 = static_cast<size_t>(readNumber<uint64_t>(streamInPart1));
                const size_t sizePart2 = static_cast<size_t>(readNumber<uint64_t>(streamInPart2));

                ByteArray buf;
                buf.resize(sizePart1 + sizePart2); //throw bad_alloc

                if (sizePart1 > 0) readArray(streamInPart1, &*buf.begin(),             sizePart1); //throw UnexpectedEndOfStreamError
                if (sizePart2 > 0) readArray(streamInPart2, &*buf.begin() + sizePart1, sizePart2); //

                MemoryStreamIn<ByteArray> streamIn(buf);
                const ByteArray bufText     = readContainer<ByteArray>(streamIn); //
                const ByteArray bufSmallNum = readContainer<ByteArray>(streamIn); //throw UnexpectedEndOfStreamError
                const ByteArray bufBigNum   = readContainer<ByteArray>(streamIn); //

                auto output = std::make_shared<InSyncFolder>(InSyncFolder::DIR_STATUS_IN_SYNC);
                StreamParser parser(streamVersion,
                                    decompStream(bufText),
                                    decompStream(bufSmallNum),
                                    decompStream(bufBigNum)); //throw FileError
                if (leadStreamLeft)
                    parser.recurse<LEFT_SIDE>(*output); //throw UnexpectedEndOfStreamError
                else
                    parser.recurse<RIGHT_SIDE>(*output); //throw UnexpectedEndOfStreamError
                return output;
            }
        }
        catch (const UnexpectedEndOfStreamError&)
        {
            throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(displayFilePathL) + L"\n" + fmtPath(displayFilePathR), L"Unexpected end of stream.");
        }
        catch (const std::bad_alloc& e)
        {
            throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(displayFilePathL) + L"\n" + fmtPath(displayFilePathR),
                            _("Out of memory.") + L" " + utfTo<std::wstring>(e.what()));
        }
    }

private:
    StreamParser(int streamVersion, const ByteArray& bufText, const ByteArray& bufSmallNumbers, const ByteArray& bufBigNumbers) :
        streamVersion_(streamVersion),
        streamInText_(bufText),
        streamInSmallNum_(bufSmallNumbers),
        streamInBigNum_(bufBigNumbers)
    {
        (void)streamVersion_; //clang: -Wunused-private-field
    }

    template <SelectedSide leadSide>
    void recurse(InSyncFolder& container) //throw UnexpectedEndOfStreamError
    {
        size_t fileCount = readNumber<uint32_t>(streamInSmallNum_);
        while (fileCount-- != 0)
        {
            const Zstring itemName = readUtf8(streamInText_);
            const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(streamInSmallNum_));
            const uint64_t fileSize = readNumber<uint64_t>(streamInSmallNum_);

            const InSyncDescrFile dataL = readFileDescr(streamInBigNum_);
            const InSyncDescrFile dataT = readFileDescr(streamInBigNum_);

            container.addFile(itemName,
                              SelectParam<leadSide>::ref(dataL, dataT),
                              SelectParam<leadSide>::ref(dataT, dataL), cmpVar, fileSize);
        }

        size_t linkCount = readNumber<uint32_t>(streamInSmallNum_);
        while (linkCount-- != 0)
        {
            const Zstring itemName = readUtf8(streamInText_);
            const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(streamInSmallNum_));

            const InSyncDescrLink dataL = readLinkDescr(streamInBigNum_);
            const InSyncDescrLink dataT = readLinkDescr(streamInBigNum_);

            container.addSymlink(itemName,
                                 SelectParam<leadSide>::ref(dataL, dataT),
                                 SelectParam<leadSide>::ref(dataT, dataL), cmpVar);
        }

        size_t dirCount = readNumber<uint32_t>(streamInSmallNum_);
        while (dirCount-- != 0)
        {
            const Zstring itemName = readUtf8(streamInText_);
            const auto status = static_cast<InSyncFolder::InSyncStatus>(readNumber<int32_t>(streamInSmallNum_));

            InSyncFolder& dbFolder = container.addFolder(itemName, status);
            recurse<leadSide>(dbFolder);
        }
    }

    static Zstring readUtf8(MemoryStreamIn<ByteArray>& streamIn) { return utfTo<Zstring>(readContainer<Zbase<char>>(streamIn)); } //throw UnexpectedEndOfStreamError
    //optional: use null-termiation: 5% overall size reduction
    //optional: split into streamInText_/streamInSmallNum_: overall size increase! (why?)

    static InSyncDescrFile readFileDescr(MemoryStreamIn<ByteArray>& streamIn) //throw UnexpectedEndOfStreamError
    {
        //attention: order of function argument evaluation is undefined! So do it one after the other...
        const auto modTime = readNumber<int64_t>(streamIn); //throw UnexpectedEndOfStreamError
        const AFS::FileId fileId = readContainer<Zbase<char>>(streamIn);

        return InSyncDescrFile(modTime, fileId);
    }

    static InSyncDescrLink readLinkDescr(MemoryStreamIn<ByteArray>& streamIn) //throw UnexpectedEndOfStreamError
    {
        const auto modTime = readNumber<int64_t>(streamIn);
        return InSyncDescrLink(modTime);
    }

    //TODO: remove migration code at some time! 2017-02-01
    class StreamParserV2
    {
    public:
        StreamParserV2(const ByteArray& bufferL,
                       const ByteArray& bufferR,
                       const ByteArray& bufferB) :
            inputLeft_ (bufferL),
            inputRight_(bufferR),
            inputBoth_ (bufferB) {}

        void recurse(InSyncFolder& container) //throw UnexpectedEndOfStreamError
        {
            size_t fileCount = readNumber<uint32_t>(inputBoth_);
            while (fileCount-- != 0)
            {
                const Zstring itemName = readUtf8(inputBoth_);
                const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(inputBoth_));
                const uint64_t fileSize = readNumber<uint64_t>(inputBoth_);
                const InSyncDescrFile dataL = readFileDescr(inputLeft_);
                const InSyncDescrFile dataR = readFileDescr(inputRight_);
                container.addFile(itemName, dataL, dataR, cmpVar, fileSize);
            }

            size_t linkCount = readNumber<uint32_t>(inputBoth_);
            while (linkCount-- != 0)
            {
                const Zstring itemName = readUtf8(inputBoth_);
                const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(inputBoth_));
                InSyncDescrLink dataL = readLinkDescr(inputLeft_);
                InSyncDescrLink dataR = readLinkDescr(inputRight_);
                container.addSymlink(itemName, dataL, dataR, cmpVar);
            }

            size_t dirCount = readNumber<uint32_t>(inputBoth_);
            while (dirCount-- != 0)
            {
                const Zstring itemName = readUtf8(inputBoth_);
                const auto status = static_cast<InSyncFolder::InSyncStatus>(readNumber<int32_t>(inputBoth_));

                InSyncFolder& dbFolder = container.addFolder(itemName, status);
                recurse(dbFolder);
            }
        }

    private:
        MemoryStreamIn<ByteArray> inputLeft_;  //data related to one side only
        MemoryStreamIn<ByteArray> inputRight_; //
        MemoryStreamIn<ByteArray> inputBoth_;  //data concerning both sides
    };

    const int streamVersion_;
    MemoryStreamIn<ByteArray> streamInText_;     //
    MemoryStreamIn<ByteArray> streamInSmallNum_; //data with bias to lead side
    MemoryStreamIn<ByteArray> streamInBigNum_;   //
};

//#######################################################################################################################################

class LastSynchronousStateUpdater
{
    /*
    1. filter by file name does *not* create a new hierarchy, but merely gives a different *view* on the existing file hierarchy
        => only update database entries matching this view!
    2. Symlink handling *does* create a new (asymmetric) hierarchy during comparison
        => update all database entries!
    */
public:
    static void execute(const BaseFolderPair& baseFolder, InSyncFolder& dbFolder)
    {
        LastSynchronousStateUpdater updater(baseFolder.getCompVariant(), baseFolder.getFilter());
        updater.recurse(baseFolder, dbFolder);
    }

private:
    LastSynchronousStateUpdater(CompareVariant activeCmpVar, const HardFilter& filter) :
        filter_(filter),
        activeCmpVar_(activeCmpVar) {}

    void recurse(const ContainerObject& hierObj, InSyncFolder& dbFolder)
    {
        process(hierObj.refSubFiles  (), hierObj.getPairRelativePath(), dbFolder.files);
        process(hierObj.refSubLinks  (), hierObj.getPairRelativePath(), dbFolder.symlinks);
        process(hierObj.refSubFolders(), hierObj.getPairRelativePath(), dbFolder.folders);
    }

    template <class M, class V>
    static V& mapAddOrUpdate(M& map, const Zstring& key, V&& value)
    {
        auto rv = map.emplace(key, value); //C++11 emplace will move r-value arguments => don't use std::forward!
        if (rv.second)
            return rv.first->second;

        return rv.first->second = std::forward<V>(value);
    }

    void process(const ContainerObject::FileList& currentFiles, const Zstring& parentRelPath, InSyncFolder::FileList& dbFiles)
    {
        std::unordered_set<const InSyncFile*> toPreserve; //referencing fixed-in-memory std::map elements

        for (const FilePair& file : currentFiles)
            if (!file.isPairEmpty())
            {
                if (file.getCategory() == FILE_EQUAL) //data in sync: write current state
                {
                    //Caveat: If FILE_EQUAL, we *implicitly* assume equal left and right short names matching case: InSyncFolder's mapping tables use short name as a key!
                    //This makes us silently dependent from code in algorithm.h!!!
                    assert(file.getItemName<LEFT_SIDE>() == file.getItemName<RIGHT_SIDE>());
                    //this should be taken for granted:
                    assert(file.getFileSize<LEFT_SIDE>() == file.getFileSize<RIGHT_SIDE>());

                    //create or update new "in-sync" state
                    InSyncFile& dbFile = mapAddOrUpdate(dbFiles, file.getPairItemName(),
                                                        InSyncFile(InSyncDescrFile(file.getLastWriteTime< LEFT_SIDE>(),
                                                                                   file.getFileId       < LEFT_SIDE>()),
                                                                   InSyncDescrFile(file.getLastWriteTime<RIGHT_SIDE>(),
                                                                                   file.getFileId       <RIGHT_SIDE>()),
                                                                   activeCmpVar_,
                                                                   file.getFileSize<LEFT_SIDE>()));
                    toPreserve.insert(&dbFile);
                }
                else //not in sync: preserve last synchronous state
                {
                    auto it = dbFiles.find(file.getPairItemName());
                    if (it != dbFiles.end())
                        toPreserve.insert(&it->second);
                }
            }

        //delete removed items (= "in-sync") from database
        erase_if(dbFiles, [&](const InSyncFolder::FileList::value_type& v) -> bool
        {
            if (toPreserve.find(&v.second) != toPreserve.end())
                return false;
            //all items not existing in "currentFiles" have either been deleted meanwhile or been excluded via filter:
            const Zstring& itemRelPath = AFS::appendPaths(parentRelPath, v.first, FILE_NAME_SEPARATOR);
            return filter_.passFileFilter(itemRelPath);
            //note: items subject to traveral errors are also excluded by this file filter here! see comparison.cpp, modified file filter for read errors
        });
    }

    void process(const ContainerObject::SymlinkList& currentSymlinks, const Zstring& parentRelPath, InSyncFolder::SymlinkList& dbSymlinks)
    {
        std::unordered_set<const InSyncSymlink*> toPreserve;

        for (const SymlinkPair& symlink : currentSymlinks)
            if (!symlink.isPairEmpty())
            {
                if (symlink.getLinkCategory() == SYMLINK_EQUAL) //data in sync: write current state
                {
                    assert(symlink.getItemName<LEFT_SIDE>() == symlink.getItemName<RIGHT_SIDE>());

                    //create or update new "in-sync" state
                    InSyncSymlink& dbSymlink = mapAddOrUpdate(dbSymlinks, symlink.getPairItemName(),
                                                              InSyncSymlink(InSyncDescrLink(symlink.getLastWriteTime<LEFT_SIDE>()),
                                                                            InSyncDescrLink(symlink.getLastWriteTime<RIGHT_SIDE>()),
                                                                            activeCmpVar_));
                    toPreserve.insert(&dbSymlink);
                }
                else //not in sync: preserve last synchronous state
                {
                    auto it = dbSymlinks.find(symlink.getPairItemName());
                    if (it != dbSymlinks.end())
                        toPreserve.insert(&it->second);
                }
            }

        //delete removed items (= "in-sync") from database
        erase_if(dbSymlinks, [&](const InSyncFolder::SymlinkList::value_type& v) -> bool
        {
            if (toPreserve.find(&v.second) != toPreserve.end())
                return false;
            //all items not existing in "currentSymlinks" have either been deleted meanwhile or been excluded via filter:
            const Zstring& itemRelPath = AFS::appendPaths(parentRelPath, v.first, FILE_NAME_SEPARATOR);
            return filter_.passFileFilter(itemRelPath);
        });
    }

    void process(const ContainerObject::FolderList& currentFolders, const Zstring& parentRelPath, InSyncFolder::FolderList& dbFolders)
    {
        std::unordered_set<const InSyncFolder*> toPreserve;

        for (const FolderPair& folder : currentFolders)
            if (!folder.isPairEmpty())
                switch (folder.getDirCategory())
                {
                    case DIR_EQUAL:
                    {
                        assert(folder.getItemName<LEFT_SIDE>() == folder.getItemName<RIGHT_SIDE>());

                        //update directory entry only (shallow), but do *not touch* exising child elements!!!
                        const Zstring& key = folder.getPairItemName();
                        auto insertResult = dbFolders.emplace(key, InSyncFolder(InSyncFolder::DIR_STATUS_IN_SYNC)); //get or create
                        auto it = insertResult.first;

                        InSyncFolder& dbFolder = it->second;
                        dbFolder.status = InSyncFolder::DIR_STATUS_IN_SYNC; //update immediate directory entry
                        toPreserve.insert(&dbFolder);
                        recurse(folder, dbFolder);
                    }
                    break;

                    case DIR_CONFLICT:
                    case DIR_DIFFERENT_METADATA:
                        //if DIR_DIFFERENT_METADATA and no old database entry yet: we have to insert a placeholder database entry:
                        //we cannot simply skip the whole directory, since sub-items might be in sync!
                        //Example: directories on left and right differ in case while sub-files are equal
                    {
                        //reuse last "in-sync" if available or insert strawman entry (do not try to update and thereby remove child elements!!!)
                        InSyncFolder& dbFolder = dbFolders.emplace(folder.getPairItemName(), InSyncFolder(InSyncFolder::DIR_STATUS_STRAW_MAN)).first->second;
                        toPreserve.insert(&dbFolder);
                        recurse(folder, dbFolder); //unconditional recursion without filter check! => no problem since "childItemMightMatch" is optional!!!
                    }
                    break;

                    //not in sync: reuse last synchronous state:
                    case DIR_LEFT_SIDE_ONLY:
                    case DIR_RIGHT_SIDE_ONLY:
                    {
                        auto it = dbFolders.find(folder.getPairItemName());
                        if (it != dbFolders.end())
                        {
                            toPreserve.insert(&it->second);
                            recurse(folder, it->second); //although existing sub-items cannot be in sync, items deleted on both sides *are* in-sync!!!
                        }
                    }
                    break;
                }

        //delete removed items (= "in-sync") from database
        erase_if(dbFolders, [&](InSyncFolder::FolderList::value_type& v) -> bool
        {
            if (toPreserve.find(&v.second) != toPreserve.end())
                return false;

            const Zstring& itemRelPath = AFS::appendPaths(parentRelPath, v.first, FILE_NAME_SEPARATOR);
            //if directory is not included in "currentDirs", it is either not existing anymore, in which case it should be deleted from database
            //or it was excluded via filter and the database entry should be preserved

            bool childItemMightMatch = true;
            const bool passFilter = filter_.passDirFilter(itemRelPath, &childItemMightMatch);
            if (!passFilter && childItemMightMatch)
                dbSetEmptyState(v.second, appendSeparator(itemRelPath)); //child items might match, e.g. *.txt include filter!
            return passFilter;
        });
    }

    //delete all entries for removed folder (= "in-sync") from database
    void dbSetEmptyState(InSyncFolder& dbFolder, const Zstring& parentRelPathPf)
    {
        erase_if(dbFolder.files,    [&](const InSyncFolder::FileList   ::value_type& v) { return filter_.passFileFilter(parentRelPathPf + v.first); });
        erase_if(dbFolder.symlinks, [&](const InSyncFolder::SymlinkList::value_type& v) { return filter_.passFileFilter(parentRelPathPf + v.first); });

        erase_if(dbFolder.folders, [&](InSyncFolder::FolderList::value_type& v)
        {
            const Zstring& itemRelPath = parentRelPathPf + v.first;

            bool childItemMightMatch = true;
            const bool passFilter = filter_.passDirFilter(itemRelPath, &childItemMightMatch);
            if (!passFilter && childItemMightMatch)
                dbSetEmptyState(v.second, appendSeparator(itemRelPath));
            return passFilter;
        });
    }

    const HardFilter& filter_; //filter used while scanning directory: generates view on actual files!
    const CompareVariant activeCmpVar_;
};


struct StreamStatusNotifier
{
    StreamStatusNotifier(const std::wstring& msgPrefix, const std::function<void(const std::wstring& statusMsg)>& notifyStatus) :
        msgPrefix_(msgPrefix), notifyStatus_(notifyStatus) {}

    void operator()(int64_t bytesDelta) //throw X
    {
        bytesTotal_ += bytesDelta;
        if (notifyStatus_) notifyStatus_(msgPrefix_ + L" (" + formatFilesizeShort(bytesTotal_) + L")"); //throw X
    }

private:
    const std::wstring msgPrefix_;
    int64_t bytesTotal_ = 0;
    const std::function<void(const std::wstring& statusMsg)> notifyStatus_;
};


std::pair<DbStreams::const_iterator,
    DbStreams::const_iterator> getCommonSession(const DbStreams& streamsLeft, const DbStreams& streamsRight, //throw FileError, FileErrorDatabaseNotExisting
                                                const std::wstring& displayFilePathL, //used for diagnostics only
                                                const std::wstring& displayFilePathR)
{
    auto itCommonL = streamsLeft .cend();
    auto itCommonR = streamsRight.cend();

    for (auto itL = streamsLeft.begin(); itL != streamsLeft.end(); ++itL)
    {
        auto itR = streamsRight.find(itL->first);
        if (itR != streamsRight.end())
            /* handle case when db file is loaded together with a former copy of itself:
                - some streams may have been updated in the meantime => must not discard either db file!
                - since db file was copied, multiple streams may have matching sessionID
                   => IGNORE all of them: one of them may be used later against other sync targets!
            */
            if (itL->second.isLeadStream != itR->second.isLeadStream)
            {
                if (itCommonL != streamsLeft.end())
                    throw FileError(_("Database file is corrupted:") + L"\n" + fmtPath(displayFilePathL) + L"\n" + fmtPath(displayFilePathR),
                                    L"Multiple common sessions found."); //should not be possible (anymore)
                itCommonL = itL;
                itCommonR = itR;
            }
    }

    if (itCommonL == streamsLeft.end())
        throw FileErrorDatabaseNotExisting(_("Initial synchronization:") + L" \n" +
                                           _("The database files do not yet contain information about the last synchronization."));

    return { itCommonL, itCommonR };
}
}

//#######################################################################################################################################

std::shared_ptr<InSyncFolder> fff::loadLastSynchronousState(const BaseFolderPair& baseFolder, //throw FileError, FileErrorDatabaseNotExisting -> return value always bound!
                                                            const std::function<void(const std::wstring& statusMsg)>& notifyStatus)
{
    const AbstractPath dbPathLeft  = getDatabaseFilePath< LEFT_SIDE>(baseFolder);
    const AbstractPath dbPathRight = getDatabaseFilePath<RIGHT_SIDE>(baseFolder);

    if (!baseFolder.isAvailable< LEFT_SIDE>() ||
        !baseFolder.isAvailable<RIGHT_SIDE>())
    {
        //avoid race condition with directory existence check: reading sync.ffs_db may succeed although first dir check had failed => conflicts!
        //https://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3531351&group_id=234430
        const AbstractPath filePath = !baseFolder.isAvailable<LEFT_SIDE>() ? dbPathLeft : dbPathRight;
        throw FileErrorDatabaseNotExisting(_("Initial synchronization:") + L" \n" + //it could be due to a to-be-created target directory not yet existing => FileErrorDatabaseNotExisting
                                           replaceCpy(_("Database file %x does not yet exist."), L"%x", fmtPath(AFS::getDisplayPath(filePath))));
    }

    StreamStatusNotifier notifyLoadL(replaceCpy(_("Loading file %x..."), L"%x", fmtPath(AFS::getDisplayPath(dbPathLeft) )), notifyStatus);
    StreamStatusNotifier notifyLoadR(replaceCpy(_("Loading file %x..."), L"%x", fmtPath(AFS::getDisplayPath(dbPathRight))), notifyStatus);

    //read file data: list of session ID + DirInfo-stream
    const DbStreams streamsLeft  = ::loadStreams(dbPathLeft,  notifyLoadL); //throw FileError, FileErrorDatabaseNotExisting, X
    const DbStreams streamsRight = ::loadStreams(dbPathRight, notifyLoadR); //

    //find associated session: there can be at most one session within intersection of left and right ids
    std::pair<DbStreams::const_iterator,
        DbStreams::const_iterator> session = getCommonSession(streamsLeft, streamsRight, //throw FileError, FileErrorDatabaseNotExisting
                                                              AFS::getDisplayPath(dbPathLeft),
                                                              AFS::getDisplayPath(dbPathRight));

    const bool leadStreamLeft = session.first->second.isLeadStream;
    assert(session.first->second.isLeadStream != session.second->second.isLeadStream);
    const ByteArray& streamL = session.first ->second.rawStream;
    const ByteArray& streamR = session.second->second.rawStream;

    return StreamParser::execute(leadStreamLeft, streamL, streamR, //throw FileError
                                 AFS::getDisplayPath(dbPathLeft),
                                 AFS::getDisplayPath(dbPathRight));
}


void fff::saveLastSynchronousState(const BaseFolderPair& baseFolder, const std::function<void(const std::wstring& statusMsg)>& notifyStatus) //throw FileError
{
    //transactional behaviour! write to tmp files first
    const AbstractPath dbPathLeft  = getDatabaseFilePath< LEFT_SIDE>(baseFolder);
    const AbstractPath dbPathRight = getDatabaseFilePath<RIGHT_SIDE>(baseFolder);

    const AbstractPath dbPathLeftTmp  = getDatabaseFilePath< LEFT_SIDE>(baseFolder, true /*tempfile*/);
    const AbstractPath dbPathRightTmp = getDatabaseFilePath<RIGHT_SIDE>(baseFolder, true /*tempfile*/);

    StreamStatusNotifier notifyLoadL(replaceCpy(_("Loading file %x..."), L"%x", fmtPath(AFS::getDisplayPath(dbPathLeft) )), notifyStatus);
    StreamStatusNotifier notifyLoadR(replaceCpy(_("Loading file %x..."), L"%x", fmtPath(AFS::getDisplayPath(dbPathRight))), notifyStatus);

    StreamStatusNotifier notifySaveL(replaceCpy(_("Saving file %x..."), L"%x", fmtPath(AFS::getDisplayPath(dbPathLeft) )), notifyStatus);
    StreamStatusNotifier notifySaveR(replaceCpy(_("Saving file %x..."), L"%x", fmtPath(AFS::getDisplayPath(dbPathRight))), notifyStatus);

    //(try to) load old database files...
    DbStreams streamsLeft; //list of session ID + DirInfo-stream
    DbStreams streamsRight;

    try { streamsLeft  = ::loadStreams(dbPathLeft, notifyLoadL); }
    catch (FileError&) {}
    try { streamsRight = ::loadStreams(dbPathRight, notifyLoadR); }
    catch (FileError&) {}
    //if error occurs: just overwrite old file! User is already informed about issues right after comparing!

    auto lastSyncState = std::make_shared<InSyncFolder>(InSyncFolder::DIR_STATUS_IN_SYNC);
    auto itStreamOldL = streamsLeft .cend();
    auto itStreamOldR = streamsRight.cend();
    try
    {
        //find associated session: there can be at most one session within intersection of left and right ids
        std::tie(itStreamOldL, itStreamOldR) = getCommonSession(streamsLeft, streamsRight, //throw FileError, FileErrorDatabaseNotExisting
                                                                AFS::getDisplayPath(dbPathLeft),
                                                                AFS::getDisplayPath(dbPathRight));

        const bool leadStreamLeft = itStreamOldL->second.isLeadStream;

        //load last synchrounous state
        lastSyncState = StreamParser::execute(leadStreamLeft,
                                              itStreamOldL->second.rawStream, //throw FileError
                                              itStreamOldR->second.rawStream,
                                              AFS::getDisplayPath(dbPathLeft),
                                              AFS::getDisplayPath(dbPathRight));
    }
    catch (FileError&) {} //if error occurs: just overwrite old file! User is already informed about issues right after comparing!

    //update last synchrounous state
    LastSynchronousStateUpdater::execute(baseFolder, *lastSyncState);

    //serialize again
    SessionData sessionDataL = {};
    SessionData sessionDataR = {};
    sessionDataL.isLeadStream = true;
    sessionDataR.isLeadStream = false;

    StreamGenerator::execute(*lastSyncState, //throw FileError
                             AFS::getDisplayPath(dbPathLeft),
                             AFS::getDisplayPath(dbPathRight),
                             sessionDataL.rawStream,
                             sessionDataR.rawStream);

    //check if there is some work to do at all
    if (itStreamOldL != streamsLeft .end() && itStreamOldL->second == sessionDataL &&
        itStreamOldR != streamsRight.end() && itStreamOldR->second == sessionDataR)
        return; //some users monitor the *.ffs_db file with RTS => don't touch the file if it isnt't strictly needed

    //erase old session data
    if (itStreamOldL != streamsLeft.end())
        streamsLeft.erase(itStreamOldL);
    if (itStreamOldR != streamsRight.end())
        streamsRight.erase(itStreamOldR);

    //create new session data
    const std::string sessionID = zen::generateGUID();

    streamsLeft [sessionID] = std::move(sessionDataL);
    streamsRight[sessionID] = std::move(sessionDataR);

    //write (temp-) files as a transaction
    saveStreams(streamsLeft,  dbPathLeftTmp,  notifySaveL); //throw FileError
    auto guardTmpL = makeGuard<ScopeGuardRunMode::ON_FAIL>([&] { try { AFS::removeFilePlain(dbPathLeftTmp); } catch (FileError&) {} });
    saveStreams(streamsRight, dbPathRightTmp, notifySaveR); //
    auto guardTmpR = makeGuard<ScopeGuardRunMode::ON_FAIL>([&] { try { AFS::removeFilePlain(dbPathRightTmp); } catch (FileError&) {} });

    //operation finished: rename temp files -> this should work (almost) transactionally:
    //if there were no write access, creation of temp files would have failed
    AFS::removeFileIfExists(dbPathLeft);        //throw FileError
    AFS::renameItem(dbPathLeftTmp, dbPathLeft); //throw FileError, (ErrorDifferentVolume)
    guardTmpL.dismiss();

    AFS::removeFileIfExists(dbPathRight);         //
    AFS::renameItem(dbPathRightTmp, dbPathRight); //
    guardTmpR.dismiss();
}
