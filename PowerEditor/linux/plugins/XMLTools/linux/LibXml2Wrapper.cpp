// This file is part of the Linux port of XML Tools for Notepad++ (GPL v3, see LICENSE).
// libxml2 in the place of MSXML (MSXMLWrapper.cpp): the syntax check, the validation (DTD, the XML schemas of
// xsi:schemaLocation / xsi:noNamespaceSchemaLocation, or a schema chosen by the user), XPath and the XSL
// transformations. libxslt is loaded at run time (dlopen): without it, the transformations report so.

#include "StdAfx.h"

#include "LibXml2Wrapper.h"
#include "Config.h"
#include "Report.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <dlfcn.h>
#include <iconv.h>
#include <unistd.h>

#include <QDir>
#include <QRegularExpression>
#include <QString>

#include <cstdarg>
#include <cstring>

namespace {
    constexpr size_t kMaxErrors = 1000;

    std::wstring wide(const xmlChar* s) {
        return s ? QString::fromUtf8(reinterpret_cast<const char*>(s)).toStdWString() : std::wstring();
    }

    std::wstring wide(const char* s) {
        return s ? QString::fromUtf8(s).toStdWString() : std::wstring();
    }

    std::string utf8(const std::wstring& s) {
        return QString::fromStdWString(s).toStdString();
    }

    // the options of the parser: Prevent XXE (no entities from outside, no network), Allow huge files
    int parserOptions(bool loadDtd) {
        int options = XML_PARSE_IGNORE_ENC; // the text of Scintilla is UTF-8, whatever the declaration says
        if (config.doPreventXXE)
            options |= XML_PARSE_NONET;
        else
            options |= XML_PARSE_NOENT;
        if (loadDtd)
            options |= XML_PARSE_DTDLOAD | XML_PARSE_DTDATTR;
        if (config.doAllowHuge)
            options |= XML_PARSE_HUGE;
        return options;
    }

    void structuredError(void* data, const xmlError* error) {
        if (!data || !error || error->level < XML_ERR_ERROR)
            return;
        auto* wrapper = static_cast<LibXml2Wrapper*>(data);
        std::wstring reason = wide(error->message);
        while (!reason.empty() && (reason.back() == L'\n' || reason.back() == L'\r'))
            reason.pop_back();
        int line = error->line;
        if (line <= 0 && error->node)
            line = static_cast<int>(xmlGetLineNo(static_cast<xmlNodePtr>(error->node)));
        if (line > 0)
            wrapper->addError(line, error->int2 > 0 ? error->int2 : 1, reason);
        else
            wrapper->addMessage(reason);
    }

    // parses the document; nullptr and errors when it is not well-formed
    xmlDocPtr parseDocument(LibXml2Wrapper* wrapper, const std::string& xml, bool loadDtd, bool validateDtd, bool* valid = nullptr) {
        xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
        if (!ctxt)
            return nullptr;
        xmlCtxtSetErrorHandler(ctxt, structuredError, wrapper);
        int options = parserOptions(loadDtd || validateDtd);
        if (validateDtd)
            options |= XML_PARSE_DTDVALID;
        // the base: the current folder (the folder of the document), for the relative DTD and entities
        const std::string base = QDir::currentPath().toStdString() + "/";
        xmlDocPtr doc = xmlCtxtReadMemory(ctxt, xml.data(), static_cast<int>(xml.size()), base.c_str(), "UTF-8", options);
        if (valid)
            *valid = ctxt->valid != 0;
        xmlFreeParserCtxt(ctxt);
        return doc;
    }

    // "xmlns:a='uri' xmlns:b="uri"" (the SelectionNamespaces of MSXML)
    void registerNamespaces(xmlXPathContextPtr ctxt, const std::wstring& ns) {
        static const QRegularExpression re(QStringLiteral("xmlns:([A-Za-z_][\\w.\\-]*)\\s*=\\s*(['\"])(.*?)\\2"));
        auto it = re.globalMatch(QString::fromStdWString(ns));
        while (it.hasNext()) {
            const auto m = it.next();
            const QByteArray prefix = m.captured(1).toUtf8();
            const QByteArray uri = m.captured(3).toUtf8();
            xmlXPathRegisterNs(ctxt, reinterpret_cast<const xmlChar*>(prefix.constData()), reinterpret_cast<const xmlChar*>(uri.constData()));
        }
    }

    std::wstring qualifiedName(xmlNodePtr node) {
        std::wstring name = wide(node->name);
        if (node->ns && node->ns->prefix)
            name = wide(node->ns->prefix) + L":" + name;
        return name;
    }

    // the nodeTypeString, nodeName and value of MSXML (an element: its direct text children only)
    XPathResultEntryType describeNode(xmlNodePtr node) {
        XPathResultEntryType e;
        auto content = [](xmlNodePtr n) {
            xmlChar* c = xmlNodeGetContent(n);
            std::wstring v = wide(c);
            xmlFree(c);
            return v;
        };
        switch (node->type) {
            case XML_ELEMENT_NODE:
                e.type = L"element";
                e.name = qualifiedName(node);
                for (xmlNodePtr child = node->children; child; child = child->next) {
                    if (child->type == XML_TEXT_NODE)
                        e.value += wide(child->content);
                }
                break;
            case XML_ATTRIBUTE_NODE:
                e.type = L"attribute";
                e.name = qualifiedName(node);
                e.value = content(node);
                break;
            case XML_TEXT_NODE:
                e.type = L"text";
                e.name = L"#text";
                e.value = wide(node->content);
                break;
            case XML_CDATA_SECTION_NODE:
                e.type = L"cdatasection";
                e.name = L"#cdata-section";
                e.value = wide(node->content);
                break;
            case XML_COMMENT_NODE:
                e.type = L"comment";
                e.name = L"#comment";
                e.value = wide(node->content);
                break;
            case XML_PI_NODE:
                e.type = L"processinginstruction";
                e.name = wide(node->name);
                e.value = wide(node->content);
                break;
            case XML_DOCUMENT_NODE:
            case XML_HTML_DOCUMENT_NODE:
                e.type = L"document";
                e.name = L"#document";
                break;
            case XML_ENTITY_REF_NODE:
                e.type = L"entityreference";
                e.name = wide(node->name);
                break;
            case XML_NAMESPACE_DECL: {
                auto* ns = reinterpret_cast<xmlNsPtr>(node);
                e.type = L"namespace";
                e.name = ns->prefix ? L"xmlns:" + wide(ns->prefix) : std::wstring(L"xmlns");
                e.value = wide(ns->href);
                break;
            }
            default:
                e.type = L"node";
                e.name = wide(node->name);
                e.value = content(node);
                break;
        }
        return e;
    }

    // ---- libxslt, loaded when it is there -----------------------------------------------------------------
    struct XsltApi {
        void* (*parseStylesheetDoc)(xmlDocPtr) = nullptr;
        xmlDocPtr (*applyStylesheet)(void*, xmlDocPtr, const char**) = nullptr;
        int (*saveResultToString)(xmlChar**, int*, xmlDocPtr, void*) = nullptr;
        void (*freeStylesheet)(void*) = nullptr;
        void (*setGenericErrorFunc)(void*, xmlGenericErrorFunc) = nullptr;
        bool ok() const { return parseStylesheetDoc && applyStylesheet && saveResultToString && freeStylesheet; }
    };

    const XsltApi& xslt() {
        static XsltApi api = [] {
            XsltApi a;
            void* lib = dlopen("libxslt.so.1", RTLD_NOW);
            if (!lib)
                return a;
            a.parseStylesheetDoc = reinterpret_cast<void* (*)(xmlDocPtr)>(dlsym(lib, "xsltParseStylesheetDoc"));
            a.applyStylesheet = reinterpret_cast<xmlDocPtr (*)(void*, xmlDocPtr, const char**)>(dlsym(lib, "xsltApplyStylesheet"));
            a.saveResultToString = reinterpret_cast<int (*)(xmlChar**, int*, xmlDocPtr, void*)>(dlsym(lib, "xsltSaveResultToString"));
            a.freeStylesheet = reinterpret_cast<void (*)(void*)>(dlsym(lib, "xsltFreeStylesheet"));
            a.setGenericErrorFunc = reinterpret_cast<void (*)(void*, xmlGenericErrorFunc)>(dlsym(lib, "xsltSetGenericErrorFunc"));
            // the EXSLT extensions (str:, math:, date:...), as MSXML has its own
            if (void* exslt = dlopen("libexslt.so.0", RTLD_NOW)) {
                if (auto registerAll = reinterpret_cast<void (*)()>(dlsym(exslt, "exsltRegisterAll")))
                    registerAll();
            }
            return a;
        }();
        return api;
    }

    void xsltError(void* data, const char* format, ...) {
        if (!data)
            return;
        char buf[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        std::wstring reason = wide(buf);
        while (!reason.empty() && (reason.back() == L'\n' || reason.back() == L'\r'))
            reason.pop_back();
        if (!reason.empty())
            static_cast<LibXml2Wrapper*>(data)->addMessage(reason);
    }

    // the "key=value key2='value 2'" parameters of the XSLT dialog (getNextParam of MSXMLWrapper.cpp)
    std::string::size_type getNextParam(std::wstring& str, std::string::size_type startpos, std::wstring* key, std::wstring* val) {
        std::string::size_type len = str.length();
        if (startpos == std::string::npos || startpos >= len || !key || !val) return std::string::npos;
        std::string::size_type keypos = str.find_first_not_of(L" \t\r\n", startpos);
        if (keypos == std::string::npos || keypos >= len) return std::string::npos;
        if (str.at(keypos) == '=') return std::string::npos;
        std::string::size_type valpos = str.find_first_of(L"=", keypos + 1);
        if (valpos == std::string::npos) return std::string::npos;
        valpos = str.find_last_not_of(L" =", valpos);
        *key = str.substr(keypos, valpos - keypos + 1);
        valpos = 1 + str.find_first_of(L"=", valpos + 1);
        if (valpos >= len) return std::string::npos;
        if (str.at(valpos) == ' ') valpos = str.find_first_not_of(L" ", valpos);
        if (valpos == std::string::npos || valpos >= len) return std::string::npos;
        std::string::size_type valendpos = valpos;
        if (str.at(valendpos) == '\'') {
            valendpos = str.find_first_of(L"\'", valendpos + 1);
            if (valendpos == std::string::npos) valendpos = len;
            *val = str.substr(valpos + 1, valendpos - valpos - 1);
        }
        else {
            valendpos = str.find_first_of(L" \t\r\n", valendpos);
            if (valendpos == std::string::npos) valendpos = len;
            *val = str.substr(valpos, valendpos - valpos);
        }
        return valendpos;
    }

    // a string as an XPath literal (the parameters of libxslt are XPath expressions)
    std::string xpathLiteral(const std::wstring& value) {
        const std::string v = utf8(value);
        if (v.find('\'') == std::string::npos)
            return "'" + v + "'";
        if (v.find('"') == std::string::npos)
            return "\"" + v + "\"";
        // both quotes: concat('...', "'", '...')
        std::string r = "concat('";
        for (char c : v) {
            if (c == '\'')
                r += "', \"'\", '";
            else
                r += c;
        }
        return r + "')";
    }

    std::string readFile(const std::wstring& path, bool* ok) {
        std::string data;
        *ok = false;
        FILE* f = std::fopen(npp_path(path.c_str()).c_str(), "rb");
        if (!f)
            return data;
        char buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
            data.append(buf, n);
        std::fclose(f);
        *ok = true;
        return data;
    }

    std::string toUtf8(const std::string& data, const std::string& encoding) {
        if (encoding.empty() || !strcasecmp(encoding.c_str(), "UTF-8"))
            return data;
        iconv_t cd = iconv_open("UTF-8", encoding.c_str());
        if (cd == reinterpret_cast<iconv_t>(-1))
            return data;
        std::string out(data.size() * 4 + 16, '\0');
        char* in = const_cast<char*>(data.data());
        size_t inLeft = data.size();
        char* o = out.data();
        size_t outLeft = out.size();
        iconv(cd, &in, &inLeft, &o, &outLeft);
        iconv_close(cd);
        out.resize(out.size() - outLeft);
        return out;
    }
}

LibXml2Wrapper::LibXml2Wrapper(const char* xml, size_t size) : m_xml(xml ? std::string(xml, size) : std::string()) {
    xmlInitParser();
    m_lineStarts.push_back(0);
    for (size_t i = 0; i < m_xml.size(); ++i) {
        if (m_xml[i] == '\n')
            m_lineStarts.push_back(i + 1);
    }
}

LibXml2Wrapper::~LibXml2Wrapper() {
    this->resetErrors();
}

int LibXml2Wrapper::getCapabilities() {
    int c = XmlCapabilityType::GET_ERROR_DETAILS | XmlCapabilityType::CHECK_SYNTAX | XmlCapabilityType::CHECK_VALIDITY | XmlCapabilityType::EVALUATE_XPATH;
    if (xslt().ok())
        c |= XmlCapabilityType::XSL_TRANSFORM;
    return c;
}

size_t LibXml2Wrapper::filePosition(int line, int column) const {
    if (line <= 0 || static_cast<size_t>(line) > m_lineStarts.size())
        return 0;
    const size_t start = m_lineStarts[static_cast<size_t>(line) - 1];
    const size_t end = static_cast<size_t>(line) < m_lineStarts.size() ? m_lineStarts[static_cast<size_t>(line)] : m_xml.size();
    return std::min(start + static_cast<size_t>(std::max(column - 1, 0)), end) + 1; // 1-based, like MSXMLWrapper
}

void LibXml2Wrapper::addError(int line, int column, const std::wstring& reason, bool positioned) {
    if (this->errors.size() >= kMaxErrors)
        return;
    ErrorEntryType e;
    e.positioned = positioned;
    e.line = positioned ? static_cast<size_t>(line) : 0;
    e.linepos = positioned ? static_cast<size_t>(column) : 0;
    e.filepos = positioned ? filePosition(line, column) : 0;
    e.reason = reason;
    this->errors.push_back(e);
}

void LibXml2Wrapper::addMessage(const std::wstring& reason) {
    addError(0, 0, reason, false);
}

bool LibXml2Wrapper::checkSyntax() {
    this->resetErrors();
    xmlDocPtr doc = parseDocument(this, m_xml, false, false);
    if (!doc) {
        if (this->errors.empty())
            addMessage(L"The document is not well-formed XML.");
        return false;
    }
    xmlFreeDoc(doc);
    return this->errors.empty();
}

bool LibXml2Wrapper::checkValidity(std::wstring schemaFilename, std::wstring validationNamespace) {
    Q_UNUSED(validationNamespace); // the target namespace of the schema is the one of its file
    this->resetErrors();
    xmlDocPtr doc = parseDocument(this, m_xml, true, false);
    if (!doc) {
        if (this->errors.empty())
            addMessage(L"The document is not well-formed XML.");
        return false;
    }
    bool res = true;

    // 1. the DTD of the DOCTYPE
    if (schemaFilename.empty() && (doc->intSubset || doc->extSubset)) {
        bool valid = true;
        xmlDocPtr checked = parseDocument(this, m_xml, true, true, &valid);
        if (checked)
            xmlFreeDoc(checked);
        res = valid && this->errors.empty();
    }

    // 2. the XML schema: the one chosen, else the ones of xsi:schemaLocation / xsi:noNamespaceSchemaLocation
    std::string schemaXml;
    std::string schemaFile;
    if (!schemaFilename.empty()) {
        schemaFile = npp_path(schemaFilename.c_str());
    }
    else if (xmlNodePtr root = xmlDocGetRootElement(doc)) {
        static const xmlChar* xsi = reinterpret_cast<const xmlChar*>("http://www.w3.org/2001/XMLSchema-instance");
        xmlChar* noNs = xmlGetNsProp(root, reinterpret_cast<const xmlChar*>("noNamespaceSchemaLocation"), xsi);
        xmlChar* withNs = xmlGetNsProp(root, reinterpret_cast<const xmlChar*>("schemaLocation"), xsi);
        if (withNs || noNs) {
            // one schema that imports them all, so that each namespace gets its own
            schemaXml = "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">";
            if (withNs) {
                const QStringList parts = QString::fromUtf8(reinterpret_cast<const char*>(withNs)).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                for (int i = 0; i + 1 < parts.size(); i += 2) {
                    schemaXml += "<xs:import namespace=\"" + parts[i].toHtmlEscaped().toStdString() + "\" schemaLocation=\""
                        + parts[i + 1].toHtmlEscaped().toStdString() + "\"/>";
                }
            }
            if (noNs)
                schemaXml += "<xs:include schemaLocation=\"" + QString::fromUtf8(reinterpret_cast<const char*>(noNs)).trimmed().toHtmlEscaped().toStdString() + "\"/>";
            schemaXml += "</xs:schema>";
            if (noNs && !withNs) {
                // a single schema without namespace: its own file, its own errors
                schemaFile = QString::fromUtf8(reinterpret_cast<const char*>(noNs)).trimmed().toStdString();
                schemaXml.clear();
            }
        }
        xmlFree(noNs);
        xmlFree(withNs);
    }

    if (res && (!schemaFile.empty() || !schemaXml.empty())) {
        xmlSchemaParserCtxtPtr pctxt = nullptr;
        xmlDocPtr schemaDoc = nullptr;
        if (!schemaFile.empty()) {
            pctxt = xmlSchemaNewParserCtxt(schemaFile.c_str());
        }
        else {
            const std::string base = QDir::currentPath().toStdString() + "/xmltools-schema.xsd";
            schemaDoc = xmlReadMemory(schemaXml.data(), static_cast<int>(schemaXml.size()), base.c_str(), "UTF-8", parserOptions(false));
            if (schemaDoc)
                pctxt = xmlSchemaNewDocParserCtxt(schemaDoc);
        }
        xmlSchemaPtr schema = nullptr;
        if (pctxt) {
            xmlSchemaSetParserStructuredErrors(pctxt, structuredError, this);
            schema = xmlSchemaParse(pctxt);
            xmlSchemaFreeParserCtxt(pctxt);
        }
        if (!schema) {
            this->errors.clear(); // the positions would be the ones of the schema
            addMessage(L"The referenced schema is detected as being invalid. Please fix it before using it as validation schema.");
            res = false;
        }
        else {
            xmlSchemaValidCtxtPtr vctxt = xmlSchemaNewValidCtxt(schema);
            if (vctxt) {
                xmlSchemaSetValidStructuredErrors(vctxt, structuredError, this);
                if (xmlSchemaValidateDoc(vctxt, doc) != 0)
                    res = false;
                xmlSchemaFreeValidCtxt(vctxt);
            }
            xmlSchemaFree(schema);
        }
        if (schemaDoc)
            xmlFreeDoc(schemaDoc);
    }
    xmlFreeDoc(doc);
    if (!res && this->errors.empty())
        addMessage(L"The document is not valid.");
    return res && this->errors.empty();
}

std::vector<XPathResultEntryType> LibXml2Wrapper::xpathEvaluate(std::wstring xpath, std::wstring ns) {
    std::vector<XPathResultEntryType> res;
    this->resetErrors();
    xmlDocPtr doc = parseDocument(this, m_xml, false, false);
    if (!doc) {
        if (this->errors.empty())
            addMessage(L"The document is not well-formed XML.");
        return res;
    }
    xmlXPathContextPtr ctxt = xmlXPathNewContext(doc);
    if (ctxt) {
        registerNamespaces(ctxt, ns);
        xmlXPathSetErrorHandler(ctxt, structuredError, this);
        const std::string expr = utf8(xpath);
        xmlXPathObjectPtr obj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), ctxt);
        if (!obj) {
            if (this->errors.empty())
                addMessage(L"Error: error on XPath expression, or missing namespace definition.");
        }
        else {
            switch (obj->type) {
                case XPATH_NODESET:
                    for (int i = 0; obj->nodesetval && i < obj->nodesetval->nodeNr; ++i)
                        res.push_back(describeNode(obj->nodesetval->nodeTab[i]));
                    break;
                case XPATH_BOOLEAN:
                    res.push_back({L"boolean", L"", obj->boolval ? L"true" : L"false"});
                    break;
                case XPATH_NUMBER: {
                    xmlChar* s = xmlXPathCastNumberToString(obj->floatval);
                    res.push_back({L"number", L"", wide(s)});
                    xmlFree(s);
                    break;
                }
                case XPATH_STRING:
                    res.push_back({L"string", L"", wide(obj->stringval)});
                    break;
                default:
                    break;
            }
            xmlXPathFreeObject(obj);
            // the errors of an expression that gave a result anyway are not errors
            if (!res.empty())
                this->resetErrors();
        }
        xmlXPathFreeContext(ctxt);
    }
    xmlFreeDoc(doc);
    return res;
}

bool LibXml2Wrapper::xslTransform(std::wstring xslfile, XSLTransformResultType* out, std::wstring options, UniMode srcEncoding) {
    this->resetErrors();
    const XsltApi& api = xslt();
    if (!api.ok()) {
        addMessage(L"XSL transformations need libxslt (libxslt.so.1), which is not installed.");
        return false;
    }
    xmlDocPtr current = parseDocument(this, m_xml, false, false);
    if (!current) {
        if (this->errors.empty())
            addMessage(L"An error occurred during current source loading. Please check source validity. Transformation aborted.");
        return false;
    }
    // the active document may be the stylesheet: then the file is the XML
    bool currentIsXml = true;
    if (xmlNodePtr root = xmlDocGetRootElement(current)) {
        if (root->ns && root->ns->href && !xmlStrcmp(root->ns->href, reinterpret_cast<const xmlChar*>("http://www.w3.org/1999/XSL/Transform"))
            && (!xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("stylesheet")) || !xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("transform"))))
            currentIsXml = false;
    }
    bool readOk = false;
    const std::string other = readFile(xslfile, &readOk);
    if (!readOk) {
        xmlFreeDoc(current);
        addMessage(L"The file " + xslfile + L" cannot be read.");
        return false;
    }
    xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
    xmlCtxtSetErrorHandler(ctxt, structuredError, this);
    const std::string otherUrl = npp_path(xslfile.c_str());
    xmlDocPtr otherDoc = xmlCtxtReadMemory(ctxt, other.data(), static_cast<int>(other.size()), otherUrl.c_str(), nullptr, parserOptions(false) & ~XML_PARSE_IGNORE_ENC);
    xmlFreeParserCtxt(ctxt);
    if (!otherDoc) {
        xmlFreeDoc(current);
        this->errors.clear(); // the positions are the ones of the other file
        addMessage(currentIsXml ? L"An error occurred during XSL loading. Please check XSL validity. Transformation aborted."
            : L"An error occurred during XML loading. Please check XML validity. Transformation aborted.");
        return false;
    }
    xmlDocPtr xml = currentIsXml ? current : otherDoc;
    xmlDocPtr xsl = currentIsXml ? otherDoc : current;

    // the output encoding: xsl:output/@encoding, else the one of the source
    std::string encoding;
    if (xmlXPathContextPtr xc = xmlXPathNewContext(xsl)) {
        xmlXPathRegisterNs(xc, reinterpret_cast<const xmlChar*>("xsl"), reinterpret_cast<const xmlChar*>("http://www.w3.org/1999/XSL/Transform"));
        if (xmlXPathObjectPtr o = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("string(/*/xsl:output/@encoding)"), xc)) {
            if (o->type == XPATH_STRING && o->stringval)
                encoding = reinterpret_cast<const char*>(o->stringval);
            xmlXPathFreeObject(o);
        }
        xmlXPathFreeContext(xc);
    }
    out->encoding = encoding.empty() ? srcEncoding : Report::getEncoding(const_cast<BSTR>(QString::fromStdString(encoding).toStdWString().c_str()));

    if (api.setGenericErrorFunc)
        api.setGenericErrorFunc(this, xsltError);
    void* style = api.parseStylesheetDoc(xsl); // owns xsl from here
    bool res = false;
    if (!style) {
        xmlFreeDoc(xsl);
        if (this->errors.empty())
            addMessage(L"The XSL stylesheet is not valid. Transformation aborted.");
    }
    else {
        // the parameters: key=value, as XPath string literals
        std::vector<std::string> params;
        std::string::size_type i = std::string::npos;
        std::wstring key, val;
        while ((i = getNextParam(options, i + 1, &key, &val)) != std::string::npos) {
            params.push_back(utf8(key));
            params.push_back(xpathLiteral(val));
        }
        std::vector<const char*> paramPtrs;
        for (const std::string& p : params)
            paramPtrs.push_back(p.c_str());
        paramPtrs.push_back(nullptr);
        xmlDocPtr result = api.applyStylesheet(style, xml, paramPtrs.data());
        if (result) {
            xmlChar* text = nullptr;
            int length = 0;
            if (api.saveResultToString(&text, &length, result, style) == 0) {
                // the text of Scintilla is UTF-8: the bytes of the output encoding are converted to it
                out->data = toUtf8(std::string(text ? reinterpret_cast<const char*>(text) : "", static_cast<size_t>(std::max(length, 0))), encoding);
                res = true;
            }
            xmlFree(text);
            xmlFreeDoc(result);
        }
        if (!res && this->errors.empty())
            addMessage(L"An error occurred during XSL transformation");
        api.freeStylesheet(style);
    }
    if (api.setGenericErrorFunc)
        api.setGenericErrorFunc(nullptr, nullptr);
    xmlFreeDoc(xml);
    if (res)
        this->resetErrors(); // messages of xsl:message and warnings
    return res;
}
