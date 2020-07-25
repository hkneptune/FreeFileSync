// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "db_file.h"
#include <zen/guid.h>
#include <zen/crc.h>
#include <zen/build_info.h>
#include <zen/zlib_wrap.h>
#include "../afs/concrete.h"
#include "status_handler_impl.h"


using namespace zen;
using namespace fff;


namespace
{
//-------------------------------------------------------------------------------------------------------------------------------
const char DB_FILE_DESCR[] = "FreeFileSync";
const int DB_FILE_VERSION   = 11; //2020-02-07
const int DB_STREAM_VERSION =  3; //2017-02-01
//-------------------------------------------------------------------------------------------------------------------------------

DEFINE_NEW_FILE_ERROR(FileErrorDatabaseNotExisting)

struct SessionData
{
    bool isLeadStream = false;
    std::string rawStream;
};
inline
bool operator==(const SessionData& lhs, const SessionData& rhs) { return lhs.isLeadStream == rhs.isLeadStream && lhs.rawStream == rhs.rawStream; }

using UniqueId  = std::string;
using DbStreams = std::unordered_map<UniqueId, SessionData>; //list of streams by session GUID

/*------------------------------------------------------------------------------
  | ensure 32/64 bit portability: use fixed size data types only e.g. uint32_t |
  ------------------------------------------------------------------------------*/

template <SelectedSide side> inline
AbstractPath getDatabaseFilePath(const BaseFolderPair& baseFolder)
{
    static_assert(usingLittleEndian());
    /* Windows, Linux, macOS considerations for uniform database format:
        - different file IDs: no, but the volume IDs are different!
        - problem with case sensitivity: no
        - are UTC file times identical: yes (at least with 1 sec precision)
        - endianess: FFS currently not running on any big-endian platform
        - precomposed/decomposed UTF: differences already ignored
        - 32 vs 64-bit: already handled

        => give db files different names:                   */
    const Zstring dbName = Zstr(".sync"); //files beginning with dots are hidden e.g. in Nautilus
    return AFS::appendRelPath(baseFolder.getAbstractPath<side>(), dbName + SYNC_DB_FILE_ENDING);
}

//#######################################################################################################################################

void saveStreams(const DbStreams& streamList, const AbstractPath& dbPath, const IOCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    MemoryStreamOut<std::string> memStreamOut;

    //write FreeFileSync file identifier
    writeArray(memStreamOut, DB_FILE_DESCR, sizeof(DB_FILE_DESCR));

    //save file format version
    writeNumber<int32_t>(memStreamOut, DB_FILE_VERSION);

    //write stream list
    writeNumber(memStreamOut, static_cast<uint32_t>(streamList.size()));

    for (const auto& [sessionID, sessionData] : streamList)
    {
        writeContainer<std::string>(memStreamOut, sessionID);

        writeNumber<int8_t>(memStreamOut, sessionData.isLeadStream);
        writeContainer     (memStreamOut, sessionData.rawStream);
    }

    writeNumber<uint32_t>(memStreamOut, getCrc32(memStreamOut.ref()));
    //------------------------------------------------------------------------------------------------------------------------

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    const std::unique_ptr<AFS::OutputStream> fileStreamOut = AFS::getOutputStream(dbPath, //throw FileError
                                                                                  memStreamOut.ref().size(),
                                                                                  std::nullopt /*modTime*/,
                                                                                  notifyUnbufferedIO /*throw X*/);
    fileStreamOut->write(memStreamOut.ref().c_str(), memStreamOut.ref().size()); //throw FileError, X
    fileStreamOut->finalize();                                                   //throw FileError, X

}


DbStreams loadStreams(const AbstractPath& dbPath, const IOCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, FileErrorDatabaseNotExisting, X
{
    std::string byteStream;
    try
    {
        const std::unique_ptr<AFS::InputStream> fileStreamIn = AFS::getInputStream(dbPath, notifyUnbufferedIO); //throw FileError, ErrorFileLocked
        byteStream = bufferedLoad<std::string>(*fileStreamIn); //throw FileError, ErrorFileLocked, X
    }
    catch (const FileError& e)
    {
        bool dbNotYetExisting = false;
        try { dbNotYetExisting = !AFS::itemStillExists(dbPath); /*throw FileError*/ }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        if (dbNotYetExisting) //throw FileError
            throw FileErrorDatabaseNotExisting(replaceCpy(_("Database file %x does not yet exist."), L"%x", fmtPath(AFS::getDisplayPath(dbPath))));
        else
            throw;
    }
    //------------------------------------------------------------------------------------------------------------------------
    try
    {
        MemoryStreamIn memStreamIn(byteStream);

        char formatDescr[sizeof(DB_FILE_DESCR)] = {};
        readArray(memStreamIn, formatDescr, sizeof(formatDescr)); //throw SysErrorUnexpectedEos

        if (!std::equal(DB_FILE_DESCR, DB_FILE_DESCR + sizeof(DB_FILE_DESCR), formatDescr))
            throw SysError(_("File content is corrupted.") + L" (invalid header)");

        const int version = readNumber<int32_t>(memStreamIn); //throw SysErrorUnexpectedEos
        if (version ==  9 || //TODO: remove migration code at some time!  v9 used until 2017-02-01
            version == 10)   //TODO: remove migration code at some time! v10 used until 2020-02-07
            ;
        else if (version == DB_FILE_VERSION)//catch data corruption ASAP + don't rely on std::bad_alloc for consistency checking
            // => only "partially" useful for container/stream metadata since the streams data is zlib-compressed
        {
            assert(byteStream.size() >= sizeof(uint32_t)); //obviously in this context!
            MemoryStreamOut<std::string> crcStreamOut;
            writeNumber<uint32_t>(crcStreamOut, getCrc32(byteStream.begin(), byteStream.end() - sizeof(uint32_t)));

            if (!endsWith(byteStream, crcStreamOut.ref()))
                throw SysError(_("File content is corrupted.") + L" (invalid checksum)");
        }
        else
            throw SysError(_("Unsupported data format.") + L' ' + replaceCpy(_("Version: %x"), L"%x", numberTo<std::wstring>(version)));

        DbStreams output;

        //read stream list
        size_t streamCount = readNumber<uint32_t>(memStreamIn); //throw SysErrorUnexpectedEos
        while (streamCount-- != 0)
        {
            std::string sessionID = readContainer<std::string>(memStreamIn); //throw SysErrorUnexpectedEos

            SessionData sessionData = {};

            if (version == 9) //TODO: remove migration code at some time! v9 used until 2017-02-01
            {
                sessionData.rawStream = readContainer<std::string>(memStreamIn); //throw SysErrorUnexpectedEos

                MemoryStreamIn streamIn(sessionData.rawStream);
                const int streamVersion = readNumber<int32_t>(streamIn); //throw SysErrorUnexpectedEos
                if (streamVersion != 2) //don't throw here due to old stream formats
                    continue;
                sessionData.isLeadStream = readNumber<int8_t>(streamIn) != 0; //throw SysErrorUnexpectedEos
            }
            else
            {
                sessionData.isLeadStream = readNumber   <int8_t     >(memStreamIn) != 0; //throw SysErrorUnexpectedEos
                sessionData.rawStream    = readContainer<std::string>(memStreamIn);      //
            }

            output[sessionID] = std::move(sessionData);
        }
        return output;
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Cannot read database file %x."), L"%x", fmtPath(AFS::getDisplayPath(dbPath))), e.toString());
    }
}

//#######################################################################################################################################

class StreamGenerator
{
public:
    static void execute(const InSyncFolder& dbFolder, //throw FileError
                        const std::wstring& displayFilePathL, //used for diagnostics only
                        const std::wstring& displayFilePathR,
                        std::string& streamL,
                        std::string& streamR)
    {
        MemoryStreamOut<std::string> outL;
        MemoryStreamOut<std::string> outR;
        //save format version
        writeNumber<int32_t>(outL, DB_STREAM_VERSION);
        writeNumber<int32_t>(outR, DB_STREAM_VERSION);

        auto compStream = [&](const std::string& stream) //throw FileError
        {
            try
            {
                /* Zlib: optimal level - test case 1 million files
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
                return compress(stream, 3 /*level*/); //throw SysError
            }
            catch (const SysError& e)
            {
                throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(displayFilePathL + L"/" + displayFilePathR)), e.toString());
            }
        };

        StreamGenerator generator;
        //PERF_START
        generator.recurse(dbFolder);
        //PERF_STOP

        const std::string bufText     = compStream(generator.streamOutText_    .ref());
        const std::string bufSmallNum = compStream(generator.streamOutSmallNum_.ref());
        const std::string bufBigNum   = compStream(generator.streamOutBigNum_  .ref());

        MemoryStreamOut<std::string> streamOut;
        writeContainer(streamOut, bufText);
        writeContainer(streamOut, bufSmallNum);
        writeContainer(streamOut, bufBigNum);

        const std::string& buf = streamOut.ref();

        //distribute "outputBoth" over left and right streams:
        const size_t size1stPart = buf.size() / 2;
        const size_t size2ndPart = buf.size() - size1stPart;

        writeNumber<uint64_t>(outL, size1stPart);
        writeNumber<uint64_t>(outR, size2ndPart);

        if (size1stPart > 0) writeArray(outL, &buf[0], size1stPart);
        if (size2ndPart > 0) writeArray(outR, &buf[0] + size1stPart, size2ndPart);

        streamL = std::move(outL.ref());
        streamR = std::move(outR.ref());
    }

private:
    void recurse(const InSyncFolder& container)
    {
        writeNumber<uint32_t>(streamOutSmallNum_, static_cast<uint32_t>(container.files.size()));
        for (const auto& [itemName, inSyncData] : container.files)
        {
            writeItemName(itemName);
            writeNumber(streamOutSmallNum_, static_cast<int32_t>(inSyncData.cmpVar));
            writeNumber<uint64_t>(streamOutSmallNum_, inSyncData.fileSize);

            writeFileDescr(inSyncData.left);
            writeFileDescr(inSyncData.right);
        }

        writeNumber<uint32_t>(streamOutSmallNum_, static_cast<uint32_t>(container.symlinks.size()));
        for (const auto& [itemName, inSyncData] : container.symlinks)
        {
            writeItemName(itemName);
            writeNumber(streamOutSmallNum_, static_cast<int32_t>(inSyncData.cmpVar));

            writeLinkDescr(inSyncData.left);
            writeLinkDescr(inSyncData.right);
        }

        writeNumber<uint32_t>(streamOutSmallNum_, static_cast<uint32_t>(container.folders.size()));
        for (const auto& [itemName, inSyncData] : container.folders)
        {
            writeItemName(itemName);
            writeNumber<int32_t>(streamOutSmallNum_, inSyncData.status);

            recurse(inSyncData);
        }
    }

    void writeItemName(const Zstring& str) { writeContainer(streamOutText_, utfTo<std::string>(str)); }

    void writeFileDescr(const InSyncDescrFile& descr)
    {
        writeNumber<int64_t>(streamOutBigNum_, descr.modTime);
        writeContainer<std::string>(streamOutBigNum_, descr.fileId);
        static_assert(sizeof(descr.modTime) <= sizeof(int64_t)); //ensure cross-platform compatibility!
    }

    void writeLinkDescr(const InSyncDescrLink& descr) { writeNumber<int64_t>(streamOutBigNum_, descr.modTime); }

    /* maximize zlib compression by grouping similar data (=> 20% size reduction!)
         -> further ~5% reduction possible by having one container per data type

       other ideas: - avoid left/right side interleaving in writeFileDescr()              => pessimization!
                    - convert CompareVariant/InSyncStatus to "enum : unsigned char"       => only 0,4% size reduction!
                    - split up writeItemName() to use streamOutSmallNum_ + streamOutText_ => pessimization!
                    - use null-termination in writeItemName()                             => 5% size reduction (embedded zeros impossible?)
                    - use empty item name as sentinel                                     => only 0,17% size reduction!
                    - save fileSize using instreamOutBigNum_                              => pessimization!        */
    MemoryStreamOut<std::string> streamOutText_;     //
    MemoryStreamOut<std::string> streamOutSmallNum_; //data with bias to lead side (= always left in this context)
    MemoryStreamOut<std::string> streamOutBigNum_;   //
};


class StreamParser
{
public:
    static SharedRef<InSyncFolder> execute(bool leadStreamLeft, //throw FileError
                                           const std::string& streamL,
                                           const std::string& streamR,
                                           const std::wstring& displayFilePathL, //for diagnostics only
                                           const std::wstring& displayFilePathR)
    {
        try
        {
            MemoryStreamIn streamInL(streamL);
            MemoryStreamIn streamInR(streamR);

            const int streamVersion  = readNumber<int32_t>(streamInL); //throw SysErrorUnexpectedEos
            const int streamVersionR = readNumber<int32_t>(streamInR); //

            if (streamVersion != streamVersionR)
                throw SysError(_("File content is corrupted.") + L" (different stream formats)");

            //TODO: remove migration code at some time! 2017-02-01
            if (streamVersion == 2)
            {
                const bool has1stPartL = readNumber<int8_t>(streamInL) != 0; //throw SysErrorUnexpectedEos
                const bool has1stPartR = readNumber<int8_t>(streamInR) != 0; //

                if (has1stPartL == has1stPartR)
                    throw SysError(_("File content is corrupted.") + L" (second stream part missing)");
                if (has1stPartL != leadStreamLeft)
                    throw SysError(_("File content is corrupted.") + L" (has1stPartL != leadStreamLeft)");

                MemoryStreamIn<std::string>& in1stPart = leadStreamLeft ? streamInL : streamInR;
                MemoryStreamIn<std::string>& in2ndPart = leadStreamLeft ? streamInR : streamInL;

                const size_t size1stPart = static_cast<size_t>(readNumber<uint64_t>(in1stPart));
                const size_t size2ndPart = static_cast<size_t>(readNumber<uint64_t>(in2ndPart));

                std::string tmpB(size1stPart + size2ndPart, '\0'); //throw std::bad_alloc
                readArray(in1stPart, &tmpB[0],               size1stPart); //stream always non-empty
                readArray(in2ndPart, &tmpB[0] + size1stPart, size2ndPart); //throw SysErrorUnexpectedEos

                const std::string tmpL = readContainer<std::string>(streamInL);
                const std::string tmpR = readContainer<std::string>(streamInR);

                auto output = makeSharedRef<InSyncFolder>(InSyncFolder::DIR_STATUS_IN_SYNC);
                StreamParserV2 parser(decompress(tmpL),  //
                                      decompress(tmpR),  //throw SysError
                                      decompress(tmpB)); //
                parser.recurse(output.ref()); //throw SysError
                return output;
            }
            else if (streamVersion == DB_STREAM_VERSION)
            {
                MemoryStreamIn<std::string>& streamInPart1 = leadStreamLeft ? streamInL : streamInR;
                MemoryStreamIn<std::string>& streamInPart2 = leadStreamLeft ? streamInR : streamInL;

                const size_t sizePart1 = static_cast<size_t>(readNumber<uint64_t>(streamInPart1));
                const size_t sizePart2 = static_cast<size_t>(readNumber<uint64_t>(streamInPart2));

                std::string buf(sizePart1 + sizePart2, '\0');
                if (sizePart1 > 0) readArray(streamInPart1, &buf[0],             sizePart1); //throw SysErrorUnexpectedEos
                if (sizePart2 > 0) readArray(streamInPart2, &buf[0] + sizePart1, sizePart2); //

                MemoryStreamIn streamIn(buf);
                const std::string bufText     = readContainer<std::string>(streamIn); //
                const std::string bufSmallNum = readContainer<std::string>(streamIn); //throw SysErrorUnexpectedEos
                const std::string bufBigNum   = readContainer<std::string>(streamIn); //

                auto output = makeSharedRef<InSyncFolder>(InSyncFolder::DIR_STATUS_IN_SYNC);
                StreamParser parser(streamVersion,
                                    decompress(bufText),     //
                                    decompress(bufSmallNum), //throw SysError
                                    decompress(bufBigNum));  //
                if (leadStreamLeft)
                    parser.recurse<LEFT_SIDE>(output.ref()); //throw SysError
                else
                    parser.recurse<RIGHT_SIDE>(output.ref()); //throw SysError
                return output;
            }
            else
                throw SysError(_("Unsupported data format.") + L' ' + replaceCpy(_("Version: %x"), L"%x", numberTo<std::wstring>(streamVersion)));
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read database file %x."), L"%x", fmtPath(displayFilePathL) + L", " + fmtPath(displayFilePathR)), e.toString());
        }
    }

private:
    StreamParser(int streamVersion, const std::string& bufText, const std::string& bufSmallNumbers, const std::string& bufBigNumbers) :
        streamVersion_(streamVersion),
        streamInText_(bufText),
        streamInSmallNum_(bufSmallNumbers),
        streamInBigNum_(bufBigNumbers)
    {
        (void)streamVersion_; //clang: -Wunused-private-field
    }

    template <SelectedSide leadSide>
    void recurse(InSyncFolder& container) //throw SysError
    {
        size_t fileCount = readNumber<uint32_t>(streamInSmallNum_);
        while (fileCount-- != 0)
        {
            const Zstring itemName = readItemName();
            const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(streamInSmallNum_));
            const uint64_t fileSize = readNumber<uint64_t>(streamInSmallNum_);

            const InSyncDescrFile dataL = readFileDescr();
            const InSyncDescrFile dataT = readFileDescr();

            container.addFile(itemName,
                              SelectParam<leadSide>::ref(dataL, dataT),
                              SelectParam<leadSide>::ref(dataT, dataL), cmpVar, fileSize);
        }

        size_t linkCount = readNumber<uint32_t>(streamInSmallNum_);
        while (linkCount-- != 0)
        {
            const Zstring itemName = readItemName();
            const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(streamInSmallNum_));

            const InSyncDescrLink dataL = readLinkDescr();
            const InSyncDescrLink dataT = readLinkDescr();

            container.addSymlink(itemName,
                                 SelectParam<leadSide>::ref(dataL, dataT),
                                 SelectParam<leadSide>::ref(dataT, dataL), cmpVar);
        }

        size_t dirCount = readNumber<uint32_t>(streamInSmallNum_);
        while (dirCount-- != 0)
        {
            const Zstring itemName = readItemName();
            const auto status = static_cast<InSyncFolder::InSyncStatus>(readNumber<int32_t>(streamInSmallNum_));

            InSyncFolder& dbFolder = container.addFolder(itemName, status);
            recurse<leadSide>(dbFolder);
        }
    }

    Zstring readItemName() { return utfTo<Zstring>(readContainer<std::string>(streamInText_)); } //throw SysErrorUnexpectedEos

    InSyncDescrFile readFileDescr() //throw SysErrorUnexpectedEos
    {
        //attention: order of function argument evaluation is undefined! So do it one after the other...
        const auto modTime = readNumber<int64_t>(streamInBigNum_);       //throw SysErrorUnexpectedEos
        const auto fileId = readContainer<std::string>(streamInBigNum_); //

        return InSyncDescrFile(modTime, fileId);
    }

    InSyncDescrLink readLinkDescr() //throw SysErrorUnexpectedEos
    {
        const auto modTime = readNumber<int64_t>(streamInBigNum_); //throw SysErrorUnexpectedEos
        return InSyncDescrLink(modTime);
    }

    //TODO: remove migration code at some time! 2017-02-01
    class StreamParserV2
    {
    public:
        StreamParserV2(const std::string& bufferL,
                       const std::string& bufferR,
                       const std::string& bufferB) :
            inputLeft_ (bufferL),
            inputRight_(bufferR),
            inputBoth_ (bufferB) {}

        void recurse(InSyncFolder& container) //throw SysError
        {
            size_t fileCount = readNumber<uint32_t>(inputBoth_);
            while (fileCount-- != 0)
            {
                const Zstring itemName = utfTo<Zstring>(readContainer<std::string>(inputBoth_));
                const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(inputBoth_));
                const uint64_t fileSize = readNumber<uint64_t>(inputBoth_);
                const auto modTimeL = readNumber<int64_t>(inputLeft_);
                const auto fileIdL = readContainer<std::string>(inputLeft_);
                const auto modTimeR = readNumber<int64_t>(inputRight_);
                const auto fileIdR = readContainer<std::string>(inputRight_);
                container.addFile(itemName, InSyncDescrFile(modTimeL, fileIdL), InSyncDescrFile(modTimeR, fileIdR), cmpVar, fileSize);
            }

            size_t linkCount = readNumber<uint32_t>(inputBoth_);
            while (linkCount-- != 0)
            {
                const Zstring itemName = utfTo<Zstring>(readContainer<std::string>(inputBoth_));
                const auto cmpVar = static_cast<CompareVariant>(readNumber<int32_t>(inputBoth_));
                const auto modTimeL = readNumber<int64_t>(inputLeft_);
                const auto modTimeR = readNumber<int64_t>(inputRight_);
                container.addSymlink(itemName, InSyncDescrLink(modTimeL), InSyncDescrLink(modTimeR), cmpVar);
            }

            size_t dirCount = readNumber<uint32_t>(inputBoth_);
            while (dirCount-- != 0)
            {
                const Zstring itemName = utfTo<Zstring>(readContainer<std::string>(inputBoth_));
                const auto status = static_cast<InSyncFolder::InSyncStatus>(readNumber<int32_t>(inputBoth_));

                InSyncFolder& dbFolder = container.addFolder(itemName, status);
                recurse(dbFolder);
            }
        }

    private:
        MemoryStreamIn<std::string> inputLeft_;  //data related to one side only
        MemoryStreamIn<std::string> inputRight_; //
        MemoryStreamIn<std::string> inputBoth_;  //data concerning both sides
    };

    const int streamVersion_;
    MemoryStreamIn<std::string> streamInText_;     //
    MemoryStreamIn<std::string> streamInSmallNum_; //data with bias to lead side
    MemoryStreamIn<std::string> streamInBigNum_;   //
};

//#######################################################################################################################################

class LastSynchronousStateUpdater
{
    /* 1. filter by file name does *not* create a new hierarchy, but merely gives a different *view* on the existing file hierarchy
          => only update database entries matching this view!
       2. Symlink handling *does* create a new (asymmetric) hierarchy during comparison
          => update all database entries!                                           */
public:
    static void execute(const BaseFolderPair& baseFolder, InSyncFolder& dbFolder)
    {
        LastSynchronousStateUpdater updater(baseFolder.getCompVariant(), baseFolder.getFilter());
        updater.recurse(baseFolder, dbFolder);
    }

private:
    LastSynchronousStateUpdater(CompareVariant activeCmpVar, const PathFilter& filter) :
        filter_(filter),
        activeCmpVar_(activeCmpVar) {}

    void recurse(const ContainerObject& hierObj, InSyncFolder& dbFolder)
    {
        process(hierObj.refSubFiles  (), hierObj.getRelativePathAny(), dbFolder.files);
        process(hierObj.refSubLinks  (), hierObj.getRelativePathAny(), dbFolder.symlinks);
        process(hierObj.refSubFolders(), hierObj.getRelativePathAny(), dbFolder.folders);
    }

    void process(const ContainerObject::FileList& currentFiles, const Zstring& parentRelPath, InSyncFolder::FileList& dbFiles)
    {
        std::set<Zstring, LessUnicodeNormal> toPreserve;

        for (const FilePair& file : currentFiles)
            if (!file.isPairEmpty())
            {
                if (file.getCategory() == FILE_EQUAL) //data in sync: write current state
                {
                    //Caveat: If FILE_EQUAL, we *implicitly* assume equal left and right short names matching case: InSyncFolder's mapping tables use short name as a key!
                    //This makes us silently dependent from code in algorithm.h!!!
                    assert(getUnicodeNormalForm(file.getItemName<LEFT_SIDE>()) == getUnicodeNormalForm(file.getItemName<RIGHT_SIDE>()));
                    assert(file.getFileSize<LEFT_SIDE>() == file.getFileSize<RIGHT_SIDE>());

                    //create or update new "in-sync" state
                    dbFiles.insert_or_assign(file.getItemNameAny(),
                                             InSyncFile(InSyncDescrFile(file.getLastWriteTime< LEFT_SIDE>(),
                                                                        file.getFileId       < LEFT_SIDE>()),
                                                        InSyncDescrFile(file.getLastWriteTime<RIGHT_SIDE>(),
                                                                        file.getFileId       <RIGHT_SIDE>()),
                                                        activeCmpVar_,
                                                        file.getFileSize<LEFT_SIDE>()));
                    toPreserve.insert(file.getItemNameAny());
                }
                else //not in sync: preserve last synchronous state
                {
                    toPreserve.insert(file.getItemName< LEFT_SIDE>()); //left/right may differ in case!
                    toPreserve.insert(file.getItemName<RIGHT_SIDE>()); //
                }
            }

        //delete removed items (= "in-sync") from database
        std::erase_if(dbFiles, [&](const InSyncFolder::FileList::value_type& v)
        {
            if (contains(toPreserve, v.first))
                return false;
            //all items not existing in "currentFiles" have either been deleted meanwhile or been excluded via filter:
            const Zstring& itemRelPath = nativeAppendPaths(parentRelPath, v.first);
            return filter_.passFileFilter(itemRelPath);
            //note: items subject to traveral errors are also excluded by this file filter here! see comparison.cpp, modified file filter for read errors
        });
    }

    void process(const ContainerObject::SymlinkList& currentSymlinks, const Zstring& parentRelPath, InSyncFolder::SymlinkList& dbSymlinks)
    {
        std::set<Zstring, LessUnicodeNormal> toPreserve;

        for (const SymlinkPair& symlink : currentSymlinks)
            if (!symlink.isPairEmpty())
            {
                if (symlink.getLinkCategory() == SYMLINK_EQUAL) //data in sync: write current state
                {
                    assert(getUnicodeNormalForm(symlink.getItemName<LEFT_SIDE>()) == getUnicodeNormalForm(symlink.getItemName<RIGHT_SIDE>()));

                    //create or update new "in-sync" state
                    dbSymlinks.insert_or_assign(symlink.getItemNameAny(),
                                                InSyncSymlink(InSyncDescrLink(symlink.getLastWriteTime< LEFT_SIDE>()),
                                                              InSyncDescrLink(symlink.getLastWriteTime<RIGHT_SIDE>()),
                                                              activeCmpVar_));
                    toPreserve.insert(symlink.getItemNameAny());
                }
                else //not in sync: preserve last synchronous state
                {
                    toPreserve.insert(symlink.getItemName< LEFT_SIDE>()); //left/right may differ in case!
                    toPreserve.insert(symlink.getItemName<RIGHT_SIDE>()); //
                }
            }

        //delete removed items (= "in-sync") from database
        std::erase_if(dbSymlinks, [&](const InSyncFolder::SymlinkList::value_type& v)
        {
            if (contains(toPreserve, v.first))
                return false;
            //all items not existing in "currentSymlinks" have either been deleted meanwhile or been excluded via filter:
            const Zstring& itemRelPath = nativeAppendPaths(parentRelPath, v.first);
            return filter_.passFileFilter(itemRelPath);
        });
    }

    void process(const ContainerObject::FolderList& currentFolders, const Zstring& parentRelPath, InSyncFolder::FolderList& dbFolders)
    {
        std::map<Zstring, const FolderPair*, LessUnicodeNormal> toPreserve;

        for (const FolderPair& folder : currentFolders)
            if (!folder.isPairEmpty())
            {
                if (folder.getDirCategory() == DIR_EQUAL)
                {
                    assert(getUnicodeNormalForm(folder.getItemName<LEFT_SIDE>()) == getUnicodeNormalForm(folder.getItemName<RIGHT_SIDE>()));

                    //update directory entry only (shallow), but do *not touch* existing child elements!!!
                    InSyncFolder& dbFolder = dbFolders.emplace(folder.getItemNameAny(), InSyncFolder(InSyncFolder::DIR_STATUS_IN_SYNC)).first->second; //get or create
                    dbFolder.status = InSyncFolder::DIR_STATUS_IN_SYNC;

                    toPreserve.emplace(folder.getItemNameAny(), &folder);
                }
                else //not in sync: preserve last synchronous state
                {
                    toPreserve.emplace(folder.getItemName< LEFT_SIDE>(), &folder); //names differing in case? => treat like any other folder rename
                    toPreserve.emplace(folder.getItemName<RIGHT_SIDE>(), &folder); //=> no *new* database entries even if child items are in sync
                }
            }

        //delete removed items (= "in-sync") from database
        std::erase_if(dbFolders, [&](InSyncFolder::FolderList::value_type& v)
        {
            if (auto it = toPreserve.find(v.first); it != toPreserve.end())
            {
                recurse(*(it->second), v.second); //required even if e.g. DIR_LEFT_SIDE_ONLY:
                //existing child-items may not be in sync, but items deleted on both sides *are* in-sync!!!
                return false;
            }

            const Zstring& itemRelPath = nativeAppendPaths(parentRelPath, v.first);
            //if folder is not included in "current folders", it is either not existing anymore, in which case it should be deleted from database
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
        std::erase_if(dbFolder.files,    [&](const InSyncFolder::FileList   ::value_type& v) { return filter_.passFileFilter(parentRelPathPf + v.first); });
        std::erase_if(dbFolder.symlinks, [&](const InSyncFolder::SymlinkList::value_type& v) { return filter_.passFileFilter(parentRelPathPf + v.first); });

        std::erase_if(dbFolder.folders, [&](InSyncFolder::FolderList::value_type& v)
        {
            const Zstring& itemRelPath = parentRelPathPf + v.first;

            bool childItemMightMatch = true;
            const bool passFilter = filter_.passDirFilter(itemRelPath, &childItemMightMatch);
            if (!passFilter && childItemMightMatch)
                dbSetEmptyState(v.second, appendSeparator(itemRelPath));
            return passFilter;
        });
    }

    const PathFilter& filter_; //filter used while scanning directory: generates view on actual files!
    const CompareVariant activeCmpVar_;
};


struct StreamStatusNotifier
{
    StreamStatusNotifier(const std::wstring& msgPrefix, AsyncCallback& acb /*throw ThreadInterruption*/) :
        msgPrefix_(msgPrefix), acb_(acb) {}

    void operator()(int64_t bytesDelta) //throw ThreadInterruption
    {
        bytesTotal_ += bytesDelta;
        acb_.updateStatus(msgPrefix_ + L" (" + formatFilesizeShort(bytesTotal_) + L')'); //throw ThreadInterruption
    }

private:
    const std::wstring msgPrefix_;
    int64_t bytesTotal_ = 0;
    AsyncCallback& acb_;
};


std::pair<DbStreams::const_iterator,
    DbStreams::const_iterator> findCommonSession(const DbStreams& streamsLeft, const DbStreams& streamsRight, //throw FileError
                                                 const std::wstring& displayFilePathL, //used for diagnostics only
                                                 const std::wstring& displayFilePathR)
{
    auto itCommonL = streamsLeft .end();
    auto itCommonR = streamsRight.end();

    for (auto itL = streamsLeft.begin(); itL != streamsLeft.end(); ++itL)
    {
        auto itR = streamsRight.find(itL->first);
        if (itR != streamsRight.end())
            /* handle case when db file is loaded together with a (former) copy of itself:
                - some streams may have been updated in the meantime => must not discard either db file!
                - since db file was copied, multiple streams may have matching sessionID
                   => IGNORE all of them: one of them may be used later against other sync targets!       */
            if (itL->second.isLeadStream != itR->second.isLeadStream)
            {
                if (itCommonL != streamsLeft.end()) //should not be possible!
                    throw FileError(replaceCpy(_("Cannot read database file %x."), L"%x", fmtPath(displayFilePathL) + L", " + fmtPath(displayFilePathR)),
                                    _("File content is corrupted.") + L" (multiple common sessions found)");
                itCommonL = itL;
                itCommonR = itR;
            }
    }

    return { itCommonL, itCommonR };
}
}

//#######################################################################################################################################

std::unordered_map<const BaseFolderPair*, SharedRef<const InSyncFolder>> fff::loadLastSynchronousState(const std::vector<const BaseFolderPair*>& baseFolders,
                                                                      PhaseCallback& callback /*throw X*/) //throw X
{
    std::set<AbstractPath> dbFilePaths;

    for (const BaseFolderPair* baseFolder : baseFolders)
        //avoid race condition with directory existence check: reading sync.ffs_db may succeed although first dir check had failed => conflicts!
        if (baseFolder->isAvailable< LEFT_SIDE>() &&
            baseFolder->isAvailable<RIGHT_SIDE>())
        {
            dbFilePaths.insert(getDatabaseFilePath< LEFT_SIDE>(*baseFolder));
            dbFilePaths.insert(getDatabaseFilePath<RIGHT_SIDE>(*baseFolder));
        }
    //else: ignore; there's no value in reporting it other than to confuse users

    std::map<AbstractPath, DbStreams> dbStreamsByPath;
    //------------ (try to) load DB files in parallel -------------------------
    {
        Protected<std::map<AbstractPath, DbStreams>&> dbStreamsByPathShared(dbStreamsByPath);
        std::vector<std::pair<AbstractPath, ParallelWorkItem>> parallelWorkload;

        for (const AbstractPath& dbPath : dbFilePaths)
            parallelWorkload.emplace_back(dbPath, [&dbStreamsByPathShared](ParallelContext& ctx) //throw ThreadInterruption
        {
            StreamStatusNotifier notifyLoad(replaceCpy(_("Loading file %x..."), L"%x", fmtPath(AFS::getDisplayPath(ctx.itemPath))), ctx.acb);

            tryReportingError([&] //throw ThreadInterruption
            {
                try
                {
                    DbStreams dbStreams = ::loadStreams(ctx.itemPath, notifyLoad); //throw FileError, FileErrorDatabaseNotExisting, ThreadInterruption

                    dbStreamsByPathShared.access([&](auto& dbStreamsByPath2) { dbStreamsByPath2.emplace(ctx.itemPath, std::move(dbStreams)); });
                }
                catch (const FileErrorDatabaseNotExisting&) {} //redundant info => no reportInfo()
            }, ctx.acb);
        });

        massParallelExecute(parallelWorkload,
                            "Load sync.ffs_db", callback /*throw X*/); //throw X
    }
    //----------------------------------------------------------------

    std::unordered_map<const BaseFolderPair*, SharedRef<const InSyncFolder>> output;

    for (const BaseFolderPair* baseFolder : baseFolders)
        if (baseFolder->isAvailable< LEFT_SIDE>() &&
            baseFolder->isAvailable<RIGHT_SIDE>())
        {
            const AbstractPath dbPathL = getDatabaseFilePath< LEFT_SIDE>(*baseFolder);
            const AbstractPath dbPathR = getDatabaseFilePath<RIGHT_SIDE>(*baseFolder);

            auto itL = dbStreamsByPath.find(dbPathL);
            auto itR = dbStreamsByPath.find(dbPathR);

            if (itL != dbStreamsByPath.end() &&
                itR != dbStreamsByPath.end())
                try
                {
                    const DbStreams& streamsL = itL->second;
                    const DbStreams& streamsR = itR->second;

                    //find associated session: there can be at most one session within intersection of left and right IDs
                    const auto [itStreamL, itStreamR] = findCommonSession(streamsL, streamsR,
                                                                          AFS::getDisplayPath(dbPathL),
                                                                          AFS::getDisplayPath(dbPathR)); //throw FileError
                    if (itStreamL != streamsL.end())
                    {
                        assert(itStreamL->second.isLeadStream != itStreamR->second.isLeadStream);
                        SharedRef<InSyncFolder> lastSyncState = StreamParser::execute(itStreamL->second.isLeadStream,
                                                                                      itStreamL->second.rawStream,
                                                                                      itStreamR->second.rawStream,
                                                                                      AFS::getDisplayPath(dbPathL),
                                                                                      AFS::getDisplayPath(dbPathR)); //throw FileError
                        output.emplace(baseFolder, lastSyncState);
                    }
                }
                catch (const FileError& e) { callback.reportFatalError(e.toString()); } //throw X
        }

    return output;
}


void fff::saveLastSynchronousState(const BaseFolderPair& baseFolder, bool transactionalCopy,
                                   PhaseCallback& callback /*throw X*/) //throw X
{
    const AbstractPath dbPathL = getDatabaseFilePath< LEFT_SIDE>(baseFolder);
    const AbstractPath dbPathR = getDatabaseFilePath<RIGHT_SIDE>(baseFolder);

    //------------ (try to) load DB files in parallel -------------------------
    DbStreams streamsL; //list of session ID + DirInfo-stream
    DbStreams streamsR; //
    {
        bool loadSuccessL = false;
        bool loadSuccessR = false;
        std::vector<std::pair<AbstractPath, ParallelWorkItem>> parallelWorkload;

        for (const auto& [dbPath, streamsOut, loadSuccess] :
             {
                 std::tuple(dbPathL, &streamsL, &loadSuccessL),
                 std::tuple(dbPathR, &streamsR, &loadSuccessR)
             })
            parallelWorkload.emplace_back(dbPath, [&streamsOut = *streamsOut, &loadSuccess = *loadSuccess](ParallelContext& ctx) //throw ThreadInterruption
        {
            StreamStatusNotifier notifyLoad(replaceCpy(_("Loading file %x..."), L"%x", fmtPath(AFS::getDisplayPath(ctx.itemPath))), ctx.acb);

            const std::wstring errMsg = tryReportingError([&] //throw ThreadInterruption
            {
                try { streamsOut = ::loadStreams(ctx.itemPath, notifyLoad); } //throw FileError, FileErrorDatabaseNotExisting, ThreadInterruption
                catch (FileErrorDatabaseNotExisting&) {}
            }, ctx.acb);
            loadSuccess = errMsg.empty();
        });

        massParallelExecute(parallelWorkload,
                            "Load sync.ffs_db", callback /*throw X*/); //throw X

        if (!loadSuccessL || !loadSuccessR)
            return; /* don't continue when one of the two files failed to load (e.g. network drop):
                       no common session would be found, (although it may exist!) =>
                           a) if file also fails to save: new orphan session in the other file created
                           b) if file saves successfully: previous stream sessions lost + old session in other file not cleaned up (orphan)       */
    }
    //----------------------------------------------------------------

    //load last synchrounous state
    auto itStreamOldL = streamsL.cend();
    auto itStreamOldR = streamsR.cend();
    InSyncFolder lastSyncState(InSyncFolder::DIR_STATUS_IN_SYNC);
    try
    {
        //find associated session: there can be at most one session within intersection of left and right IDs
        std::tie(itStreamOldL, itStreamOldR) = findCommonSession(streamsL, streamsR,
                                                                 AFS::getDisplayPath(dbPathL),
                                                                 AFS::getDisplayPath(dbPathR)); //throw FileError
        if (itStreamOldL != streamsL.end())
            lastSyncState = std::move(StreamParser::execute(itStreamOldL->second.isLeadStream /*leadStreamLeft*/,
                                                            itStreamOldL->second.rawStream,
                                                            itStreamOldR->second.rawStream,
                                                            AFS::getDisplayPath(dbPathL),
                                                            AFS::getDisplayPath(dbPathR)).ref()); //throw FileError
    }
    catch (const FileError& e) { callback.reportFatalError(e.toString()); } //throw X
    //if database files are corrupted: just overwrite! User is already informed about errors right after comparing!

    //update last synchrounous state
    LastSynchronousStateUpdater::execute(baseFolder, lastSyncState);

    //serialize again
    SessionData sessionDataL = {};
    SessionData sessionDataR = {};
    sessionDataL.isLeadStream = true;
    sessionDataR.isLeadStream = false;

    if (const std::wstring errMsg = tryReportingError([&] //throw X
{
    StreamGenerator::execute(lastSyncState, //throw FileError
                             AFS::getDisplayPath(dbPathL),
                             AFS::getDisplayPath(dbPathR),
                             sessionDataL.rawStream,
                             sessionDataR.rawStream);
    }, callback /*throw X*/); !errMsg.empty())
    return;

    //check if there is some work to do at all
    if (itStreamOldL != streamsL.end() && itStreamOldL->second == sessionDataL &&
        itStreamOldR != streamsR.end() && itStreamOldR->second == sessionDataR)
        return; //some users monitor the *.ffs_db file with RTS => don't touch the file if it isnt't strictly needed

    //erase old session data
    if (itStreamOldL != streamsL.end())
        streamsL.erase(itStreamOldL);
    if (itStreamOldR != streamsR.end())
        streamsR.erase(itStreamOldR);

    //create new session data
    const std::string sessionID = zen::generateGUID();

    streamsL[sessionID] = std::move(sessionDataL);
    streamsR[sessionID] = std::move(sessionDataR);

    //------------ save DB files in parallel -------------------------
    {
        std::vector<std::pair<AbstractPath, ParallelWorkItem>> parallelWorkload;

        for (const auto& [dbPath, streams] :
             {
                 std::pair(dbPathL, &streamsL),
                 std::pair(dbPathR, &streamsR)
             })
            parallelWorkload.emplace_back(dbPath, [&streams = *streams, transactionalCopy](ParallelContext& ctx) //throw ThreadInterruption
        {
            tryReportingError([&] //throw ThreadInterruption
            {
                StreamStatusNotifier notifySave(replaceCpy(_("Saving file %x..."), L"%x", fmtPath(AFS::getDisplayPath(ctx.itemPath))), ctx.acb);

                if (transactionalCopy && !AFS::hasNativeTransactionalCopy(ctx.itemPath))
                {
                    //write (temp-) files as a transaction
                    const Zstring shortGuid = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));
                    const AbstractPath dbPathTmp = AFS::appendRelPath(*AFS::getParentPath(ctx.itemPath), AFS::getItemName(ctx.itemPath) + Zstr('.') + shortGuid + AFS::TEMP_FILE_ENDING);

                    saveStreams(streams, dbPathTmp, notifySave); //throw FileError, ThreadInterruption
                    ZEN_ON_SCOPE_FAIL(try { AFS::removeFilePlain(dbPathTmp); }
                    catch (FileError&) {});

                    //operation finished: rename temp file -> this should work (almost) transactionally:
                    //if there were no write access, creation of temp file would have failed
                    AFS::removeFileIfExists(ctx.itemPath);           //throw FileError
                    AFS::moveAndRenameItem(dbPathTmp, ctx.itemPath); //throw FileError, (ErrorMoveUnsupported)
                }
                else //some MTP devices don't even allow renaming files: https://freefilesync.org/forum/viewtopic.php?t=6531
                {
                    AFS::removeFileIfExists(ctx.itemPath);          //throw FileError
                    saveStreams(streams, ctx.itemPath, notifySave); //throw FileError, ThreadInterruption
                }
            }, ctx.acb);
        });

        massParallelExecute(parallelWorkload,
                            "Save sync.ffs_db", callback /*throw X*/); //throw X
    }
    //----------------------------------------------------------------
}
