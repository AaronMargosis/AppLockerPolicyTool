//TODO: get a real XML parser library and use that.

#include "AppLockerXmlParser.h"

/// L"AppLockerPolicy" - the root element of an AppLocker policy XML document 
//static
const wchar_t* const AppLockerXmlParser::szPolicyRootTagname = L"AppLockerPolicy";

/// <summary>
/// Given AppLocker policy XML, extract out each RuleCollection 
/// </summary>
/// <param name="sPolicyXml">Input: string representing the full AppLocker policy XML</param>
/// <param name="sExePolicy">Output: the XML representing the EXE rule collection</param>
/// <param name="sDllPolicy">Output: the XML representing the DLL rule collection</param>
/// <param name="sMsiPolicy">Output: the XML representing the MSI rule collection</param>
/// <param name="sScriptPolicy">Output: the XML representing the Script rule collection</param>
/// <param name="sAppxPolicy">Output: the XML representing the Appx ("Store apps," "Packaged apps") rule collection</param>
/// <returns>true if successful, false on any parsing error</returns>
bool AppLockerXmlParser::ParseRuleCollections(const std::wstring& sPolicyXml, std::wstring& sExePolicy, std::wstring& sDllPolicy, std::wstring& sMsiPolicy, std::wstring& sScriptPolicy, std::wstring& sAppxPolicy)
{
    // NOT a robust XML parse here; mostly assumes well-formed AppLocker policy XML.
    sExePolicy.clear();
    sDllPolicy.clear();
    sMsiPolicy.clear();
    sScriptPolicy.clear();
    sAppxPolicy.clear();

    // Verify that the AppLocker policy root element appears to be in this string.
    // It's not invalid to have that and then no rule collections. But have to have that root element.
    if (std::wstring::npos == sPolicyXml.find(std::wstring(L"<") + szPolicyRootTagname))
        return false;

    // Limited parsing validation
    bool bParseOK = true;
    // ixRC is index pointing to the next rule collection or the starting point for the next search.
    size_t ixRC = 0;
    // Quit when there are no more rule collections or we hit a parsing problem.
    while (std::wstring::npos != ixRC && bParseOK)
    {
        // Find the next rule collection
        ixRC = sPolicyXml.find(L"<RuleCollection ", ixRC);
        if (std::wstring::npos != ixRC)
        {
            // Determine what rule collection this is - get the opening dquote following the next "Type" attribute
            bParseOK = false;
            size_t ixType = sPolicyXml.find(L"Type", ixRC);
            if (std::wstring::npos != ixType)
            {
                size_t ixDQ = sPolicyXml.find(L'"', ixType);
                if (std::wstring::npos != ixDQ)
                {
                    // Get three characters of the "Type" attribute's text.
                    // (Could get the entire text up to the next dquote, but this will work well enough)
                    std::wstring sType = sPolicyXml.substr(ixDQ + 1, 3);
                    // Find the rule collection's ending element
                    // The RuleCollection element can have child elements representing one or more rules,
                    // or it might be empty. So the RuleCollection can end with "</RuleCollection>" or it
                    // might end with "/>". In the latter case, we'd find a "/>" before we see another "<".
                    // Look for that first, and if we don't find that, then look for the full closing tag.
                    std::wstring sEndRC = L"/>";
                    size_t ixNextLT = sPolicyXml.find(L"<", ixDQ);
                    size_t ixEndRC = sPolicyXml.find(sEndRC, ixDQ);
                    if (std::wstring::npos == ixEndRC || (std::wstring::npos != ixNextLT && ixNextLT < ixEndRC))
                    {
                        sEndRC = L"</RuleCollection>";
                        ixEndRC = sPolicyXml.find(sEndRC, ixDQ);
                    }
                    if (std::wstring::npos != ixEndRC)
                    {
                        bParseOK = true;
                        // Write the rule collection substring to the output parameter corresponding to
                        // the collection type. Note that we're looking at only three characters of the
                        // type name in this case (e.g., "Scr" instead of "Script").
                        size_t substrLen = ixEndRC + sEndRC.length() - ixRC;
                        if (sType == L"Exe") { sExePolicy = sPolicyXml.substr(ixRC, substrLen); }
                        else if (sType == L"Dll") { sDllPolicy = sPolicyXml.substr(ixRC, substrLen); }
                        else if (sType == L"Msi") { sMsiPolicy = sPolicyXml.substr(ixRC, substrLen); }
                        else if (sType == L"Scr") { sScriptPolicy = sPolicyXml.substr(ixRC, substrLen); }
                        else if (sType == L"App") { sAppxPolicy = sPolicyXml.substr(ixRC, substrLen); }
                        else {
                            bParseOK = false;
                        }
                        // Move up to begin search for next rule collection.
                        ixRC += substrLen;
                    }
                }
            }
        }
    }

    return bParseOK;
}

/// <summary>
/// Given a RuleCollection XML, extract the separate rules into a RuleInfoCollection_t.
/// </summary>
/// <param name="sRuleCollectionXml">Input: the XML representing a rule collection</param>
/// <param name="dwEnforcementMode">Output: an integer corresponding to the collection's enforcement mode: 1 for Enforce, 0 for Audit</param>
/// <param name="extensions">Output: values corresponding to optional RuleCollectionExtensions element</param>
/// <param name="rules">A collection of structures containing the XML for each rule and the GUID ID.
/// Note: returns rule info only if enforcement mode is Enforce or Audit; returns nothing if enforcement mode is "not configured".</param>
/// <returns>true if successful (even if no rules returned), false on any parsing error.</returns>
bool AppLockerXmlParser::ParseRuleCollection(
    const std::wstring& sRuleCollectionXml, 
    unsigned long& dwEnforcementMode, 
    RuleCollectionExtensions_t& extensions,
    RuleInfoCollection_t& rules)
{
    // Initialize output parameters
    dwEnforcementMode = 0;
    extensions.Clear();
    rules.clear();

    // Looking at attributes within the opening RuleCollection element - look for closing '>'
    size_t ixFirstGT = sRuleCollectionXml.find(L'>');
    if (std::wstring::npos == ixFirstGT)
        return false;

    std::wstring sRuleCollectionElement = sRuleCollectionXml.substr(0, ixFirstGT);
    // Find the EnforcementMode attribute within the RuleCollection element
    size_t ixEnforce = sRuleCollectionElement.find(L"EnforcementMode");
    // Error if not found
    if (std::wstring::npos == ixEnforce)
        return false;
    // Search for subsequent text for usual values:
    // OK if Not Configured, but quit - not returning rules in this case
    if (std::wstring::npos != sRuleCollectionElement.find(L"NotConfigured", ixEnforce))
        return true;
    // "Enabled" means Enforce mode
    if (std::wstring::npos != sRuleCollectionElement.find(L"Enabled", ixEnforce))
        dwEnforcementMode = 1;
    else if (std::wstring::npos != sRuleCollectionElement.find(L"AuditOnly", ixEnforce))
        dwEnforcementMode = 0;
    else
        return false;

    // Now separately go get the path, publisher, and hash rules and add them to "rules"
    if (!ParseRules(sRuleCollectionXml, L"FilePathRule", rules))
        return false;
    if (!ParseRules(sRuleCollectionXml, L"FilePublisherRule", rules))
        return false;
    if (!ParseRules(sRuleCollectionXml, L"FileHashRule", rules))
        return false;

    if (!ParseExtensions(sRuleCollectionXml, extensions))
        return false;

    return true;
}

static const std::wstring sAngle      = std::wstring(L"<");
static const std::wstring sAngleSlash = std::wstring(L"</");

/// <summary>
/// Internal method used by ParseRuleCollection to extract path, publisher, and hash rules separately. 
/// </summary>
/// <param name="sRuleCollectionXml">Input: the XML representing a rule collection</param>
/// <param name="szRuleName">The XML element name for the rule type; e.g., "FilePathRule"</param>
/// <param name="rules">A collection of structures containing the XML for each rule and the GUID ID.</param>
/// <returns>true if successful, false on any parsing error.</returns>
bool AppLockerXmlParser::ParseRules(const std::wstring& sRuleCollectionXml, const wchar_t* szRuleName, RuleInfoCollection_t& rules)
{
    // Create strings representing text to find the opening and corresponding closing elements
    // for this rule.
    const std::wstring sRuleStartElem = sAngle + szRuleName;
    const std::wstring sRuleEndElem = sAngleSlash + szRuleName;
    size_t ixRuleStart = 0;
    while (std::wstring::npos != ixRuleStart)
    {
        // Find the next instance of the rule
        ixRuleStart = sRuleCollectionXml.find(sRuleStartElem, ixRuleStart);
        if (std::wstring::npos != ixRuleStart)
        {
            // Find the next instance of the rule ending following this rule start
            size_t ixRuleEnd = sRuleCollectionXml.find(sRuleEndElem, ixRuleStart);
            if (std::wstring::npos == ixRuleEnd)
                return false;
            // Find the next closing '>' after the ending element text (could be whitespace between the end of the name and the '>')
            ixRuleEnd = sRuleCollectionXml.find(L">", ixRuleEnd);
            if (std::wstring::npos == ixRuleEnd)
                return false;
            // Create a new RuleInfo_t and get the substring representing the rule into .sXml.
            RuleInfo_t ruleInfo;
            ruleInfo.sXml = sRuleCollectionXml.substr(ixRuleStart, ixRuleEnd + 1 - ixRuleStart);
            // Find the "Id" attribute within this rule
            size_t ixId = ruleInfo.sXml.find(L"Id");
            if (std::wstring::npos == ixId)
                return false;
            // Get the text in between the next two dquotes -- that's the GUID.
            size_t ixDQ = ruleInfo.sXml.find(L'"', ixId);
            if (std::wstring::npos == ixDQ)
                return false;
            size_t ixGuidStart = ixDQ + 1;
            ixDQ = ruleInfo.sXml.find(L'"', ixGuidStart);
            if (std::wstring::npos == ixDQ)
                return false;
            ruleInfo.sGuid = ruleInfo.sXml.substr(ixGuidStart, ixDQ - ixGuidStart);
            // Add it to the collection.
            rules.push_back(ruleInfo);
            // Move up the index before searching for the next rule
            ixRuleStart += ruleInfo.sXml.length();
        }
    }

    return true;
}

/// <summary>
/// Provide support for AppLocker rule collection extensions
/// https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/applocker/rule-collection-extensions
/// </summary>
/// <param name="sRuleCollectionXml">Input: the XML representing a rule collection</param>
/// <param name="extensions">Output: a collection of AppLocker rule collection extension settings</param>
/// <returns>true if successful, false on any parsing error.</returns>
bool AppLockerXmlParser::ParseExtensions(const std::wstring& sRuleCollectionXml, RuleCollectionExtensions_t& extensions)
{
    extensions.Clear();

    // Strings representing element names, attribute names, and allowed attribute values.
    //
    const wchar_t* szRuleCollectionsExtensions = L"RuleCollectionExtensions";
   
    const wchar_t* szThresholdExtensions       = L"ThresholdExtensions";
    const wchar_t* szServices                  = L"Services";
    const wchar_t* szEnforcementMode           = L"EnforcementMode";
    const wchar_t* szEnforcementModeOptions[]  = { L"NotConfigured", L"Enabled", L"ServicesOnly" }; // maps to ServiceEnforcementMode reg value DELETE, DWORD 1, DWORD 2
    //const size_t nEnforcementModeOptions       = sizeof(szEnforcementModeOptions) / sizeof(szEnforcementModeOptions[0]);

    const wchar_t* szRedstoneExtensions        = L"RedstoneExtensions";
    const wchar_t* szSystemApps                = L"SystemApps";
    const wchar_t* szAllow                     = L"Allow";
    const wchar_t* szAllowOptions[]            = { L"NotEnabled", L"Enabled" }; // maps to AllowWindows reg value DWORD 0, DWORD 1
    //const size_t nAllowOptions                 = sizeof(szAllowOptions) / sizeof(szAllowOptions[0]);

    const std::wstring sRuleCollectionExtensionsStartElem = sAngle + szRuleCollectionsExtensions;
    const std::wstring sRuleCollectionExtensionsEndElem = sAngleSlash + szRuleCollectionsExtensions;
    size_t ixStart = sRuleCollectionXml.find(sRuleCollectionExtensionsStartElem);
    // Look for rule collection extensions starting element; exit (successfully) if not found
    if (std::wstring::npos == ixStart)
        return true;
    // Look for corresponding closing element; fail if not found
    size_t ixEnd = sRuleCollectionXml.find(sRuleCollectionExtensionsEndElem, ixStart);
    if (std::wstring::npos == ixEnd)
        return false;

    // Assuming well-formed XML, no need to find the ending >, just work with the parts we found
    std::wstring sExtensionsXml = sRuleCollectionXml.substr(ixStart, ixEnd - ixStart);

    // ThresholdExtensions and RedstoneExtensions must both be present, but their child elements are optional
    size_t ixThresholdStart = sExtensionsXml.find(sAngle + szThresholdExtensions);
    size_t ixRedstoneStart = sExtensionsXml.find(sAngle + szRedstoneExtensions);
    // If either is missing, fail
    if (std::wstring::npos == ixThresholdStart || std::wstring::npos == ixRedstoneStart)
        return false;
    // Get the starts of the ending elements.
    size_t ixThresholdEnd = sExtensionsXml.find(sAngleSlash + szThresholdExtensions, ixThresholdStart);
    size_t ixRedstoneEnd = sExtensionsXml.find(sAngleSlash + szRedstoneExtensions, ixRedstoneStart);
    if (std::wstring::npos == ixThresholdEnd || std::wstring::npos == ixRedstoneEnd)
        return false;

    // Again, assuming well-formed XML
    std::wstring sThreshold = sExtensionsXml.substr(ixThresholdStart, ixThresholdEnd - ixThresholdStart);
    std::wstring sRedstone = sExtensionsXml.substr(ixRedstoneStart, ixRedstoneEnd - ixRedstoneStart);
    size_t ixServicesStart = sThreshold.find(sAngle + szServices);
    if (std::wstring::npos != ixServicesStart)
    {
        size_t ixEnforcementMode = sThreshold.find(szEnforcementMode, ixServicesStart);
        if (std::wstring::npos != ixEnforcementMode)
        {
            // Get the text between the next two dquotes
            size_t ixDQ0 = sThreshold.find(L'"', ixEnforcementMode);
            if (std::wstring::npos == ixDQ0)
                return false;
            size_t ixEnforcementModeValue = ixDQ0 + 1;
            size_t ixDQ1 = sThreshold.find(L'"', ixEnforcementModeValue);
            if (std::wstring::npos == ixDQ1)
                return false;
            std::wstring sEnforcementMode = sThreshold.substr(ixDQ0 + 1, ixDQ1 - ixEnforcementModeValue);
            if (0 == wcscmp(sEnforcementMode.c_str(), szEnforcementModeOptions[0]))
            { }
            else if (0 == wcscmp(sEnforcementMode.c_str(), szEnforcementModeOptions[1]))
            {
                extensions.bServicesModePresent = true;
                extensions.dwServiceEnforcementMode = 1;
            }
            else if (0 == wcscmp(sEnforcementMode.c_str(), szEnforcementModeOptions[2]))
            {
                extensions.bServicesModePresent = true;
                extensions.dwServiceEnforcementMode = 2;
            }
            else
            {
                //TODO: Invalid -- fail here?
            }
        }
    }
    size_t ixSystemAppsStart = sRedstone.find(sAngle + szSystemApps);
    if (std::wstring::npos != ixSystemAppsStart)
    {
        size_t ixAllow = sRedstone.find(szAllow, ixSystemAppsStart);
        if (std::wstring::npos != ixAllow)
        {
            // Get the text between the next two dquotes
            size_t ixDQ0 = sRedstone.find(L'"', ixAllow);
            if (std::wstring::npos == ixDQ0)
                return false;
            size_t ixAllowValue = ixDQ0 + 1;
            size_t ixDQ1 = sRedstone.find(L'"', ixAllowValue);
            if (std::wstring::npos == ixDQ1)
                return false;
            std::wstring sAllow = sRedstone.substr(ixDQ0 + 1, ixDQ1 - ixAllowValue);
            if (0 == wcscmp(sAllow.c_str(), szAllowOptions[0]))
            {
                extensions.dwAllowWindows = 0;
            }
            else if (0 == wcscmp(sAllow.c_str(), szAllowOptions[1]))
            {
                extensions.dwAllowWindows = 1;
            }
            else
            {
                //TODO: Invalid -- fail here?
            }
        }
    }

    return true;
}
