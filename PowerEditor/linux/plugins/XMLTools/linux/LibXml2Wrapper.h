// This file is part of the Linux port of XML Tools for Notepad++ (GPL v3, see LICENSE).
// The XML engine of XML Tools on Linux: libxml2 (and libxslt, loaded when it is installed) in the place of MSXML,
// behind the XmlWrapperInterface of the Windows version.

#pragma once

#include "XmlWrapperInterface.h"

#include <string>
#include <vector>

class LibXml2Wrapper : public XmlWrapperInterface {
public:
    LibXml2Wrapper(const char* xml, size_t size);
    ~LibXml2Wrapper() override;

    int getCapabilities() override;
    bool checkSyntax() override;
    bool checkValidity(std::wstring schemaFilename = L"", std::wstring validationNamespace = L"") override;
    std::vector<XPathResultEntryType> xpathEvaluate(std::wstring xpath, std::wstring ns = L"") override;
    bool xslTransform(std::wstring xslfile, XSLTransformResultType* out, std::wstring options = L"", UniMode srcEncoding = UniMode::uniEnd) override;

    // for the error handlers of libxml2
    void addError(int line, int column, const std::wstring& reason, bool positioned = true);
    void addMessage(const std::wstring& reason);

private:
    std::string m_xml;                   // the document (UTF-8, the text of Scintilla)
    std::vector<size_t> m_lineStarts;    // offsets of the lines, for the positions of the errors
    size_t filePosition(int line, int column) const;
};
