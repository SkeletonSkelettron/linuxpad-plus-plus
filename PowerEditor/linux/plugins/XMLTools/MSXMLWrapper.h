#ifdef NPP_PLUGIN_SDK_LINUX
// Linux: libxml2 (and libxslt) in the place of MSXML, behind the same interface
#pragma once
#include "linux/LibXml2Wrapper.h"
using MSXMLWrapper = LibXml2Wrapper;
#else
#pragma once

#include "XmlWrapperInterface.h"
#include "MSXMLHelper.h"

class MSXMLWrapper : public XmlWrapperInterface {
	CComBSTR m_sXml;
	void addErrorToVector(IXMLDOMParseError2* pXMLErr, const wchar_t* szDesc = L"An unexpected error occurred");
	void buildErrorsVector(IXMLDOMParseError2* pXMLErr, const wchar_t* szDesc = L"An unexpected error occurred");

public:
	MSXMLWrapper(const char* xml, size_t size);
	~MSXMLWrapper();

	int getCapabilities();
	bool checkSyntax();
	bool checkValidity(std::wstring schemaFilename = L"", std::wstring validationNamespace = L"");
	std::vector<XPathResultEntryType> xpathEvaluate(std::wstring xpath, std::wstring ns = L"");
	bool xslTransform(std::wstring xslfile, XSLTransformResultType* out, std::wstring options = L"", UniMode srcEncoding = UniMode::uniEnd);
};
#endif // NPP_PLUGIN_SDK_LINUX
