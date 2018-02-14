// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "xml_io.h"
#include "file_access.h"
#include "file_io.h"

using namespace zen;


XmlDoc zen::loadXmlDocument(const Zstring& filePath) //throw FileError
{
    FileInput fileIn(filePath, nullptr /*notifyUnbufferedIO*/); //throw FileError, ErrorFileLocked
    const size_t blockSize = fileIn.getBlockSize();
    const std::string xmlPrefix = "<?xml version=";
    bool xmlPrefixChecked = false;

    std::string buffer;
    for (;;)
    {
        buffer.resize(buffer.size() + blockSize);
        const size_t bytesRead = fileIn.read(&*(buffer.end() - blockSize), blockSize); //throw FileError, ErrorFileLocked, (X); return "bytesToRead" bytes unless end of stream!
        buffer.resize(buffer.size() - blockSize + bytesRead); //caveat: unsigned arithmetics

        //quick test whether input is an XML: avoid loading large binary files up front!
        if (!xmlPrefixChecked && buffer.size() >= xmlPrefix.size() + strLength(BYTE_ORDER_MARK_UTF8))
        {
            xmlPrefixChecked = true;
            if (!startsWith(buffer, xmlPrefix) &&
                !startsWith(buffer, BYTE_ORDER_MARK_UTF8 + xmlPrefix)) //allow BOM!
                throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));
        }

        if (bytesRead < blockSize) //end of file
            break;
    }

    try
    {
        return parse(buffer); //throw XmlParsingError
    }
    catch (const XmlParsingError& e)
    {
        throw FileError(
            replaceCpy(replaceCpy(replaceCpy(_("Error parsing file %x, row %y, column %z."),
                                             L"%x", fmtPath(filePath)),
                                  L"%y", numberTo<std::wstring>(e.row + 1)),
                       L"%z", numberTo<std::wstring>(e.col + 1)));
    }
}


void zen::saveXmlDocument(const XmlDoc& doc, const Zstring& filePath) //throw FileError
{
    const std::string stream = serialize(doc); //noexcept

    //only update xml file if there are real changes
    try
    {
        if (getFileSize(filePath) == stream.size()) //throw FileError
            if (loadBinContainer<std::string>(filePath, nullptr /*notifyUnbufferedIO*/) == stream) //throw FileError
                return;
    }
    catch (FileError&) {}

    saveBinContainer(filePath, stream, nullptr /*notifyUnbufferedIO*/); //throw FileError
}


void zen::checkForMappingErrors(const XmlIn& xmlInput, const Zstring& filePath) //throw FileError
{
    if (xmlInput.errorsOccured())
    {
        std::wstring msg = _("The following XML elements could not be read:") + L"\n";
        for (const std::wstring& elem : xmlInput.getErrorsAs<std::wstring>())
            msg += L"\n" + elem;

        throw FileError(replaceCpy(_("Configuration file %x is incomplete. The missing elements will be set to their default values."), L"%x", fmtPath(filePath)) + L"\n\n" + msg);
    }
}
