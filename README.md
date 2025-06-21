# AppLockerPolicyTool

AppLockerPolicyTool.exe is a command-line tool to manage AppLocker policy on the local Windows endpoint:
listing, replacing, or deleting AppLocker policy either through local GPO, or CSP/MDM interfaces (without an MDM server).
It can also retrieve _effective_ GPO policy, which can incorporate AppLocker policies from Active Directory GPO.
Finally, it provides an emergency interface directly into the AppLocker policy cache.

AppLockerPolicyTool.exe is released only as a 32-bit executable. It works correctly on 32- and 64-bit Windows versions.

## Command-line syntax:

```
  Configuration Service Provider (CSP) operations:

    AppLockerPolicyTool.exe -csp -get [-out filename]
    AppLockerPolicyTool.exe -csp -set filename [-gn groupname]
    AppLockerPolicyTool.exe -csp -deleteall

  Local Group Policy Object (LGPO) operations:

    AppLockerPolicyTool.exe -lgpo -get [-out filename]
    AppLockerPolicyTool.exe -lgpo -set filename
    AppLockerPolicyTool.exe -lgpo -clear

  Effective Group Policy Object (GPO) operations:

    AppLockerPolicyTool.exe -gpo -get [-out filename]

  Last resort emergency operations:

    AppLockerPolicyTool.exe -911 [-list | -deleteall]
```

## Configuration Service Provider (CSP) operations

_Note: all CSP operations must be executed under the System account. Administrative rights are insufficient. (See Sysinternals PsExec and its `-s` switch.)_

CSPs are the interfaces used by MDM services such as Microsoft's Intune. AppLocker policy configured through CSP are not visible
through any of the means one would use to identify [L]GPO-managed AppLocker policies -- they aren't reflected through the
Group Policy (gpedit.msc) or Local Security Policy (secpol.msc) editors, GpResult reports, the registry, nor the
PowerShell `Get-AppLockerPolicy` cmdlet.

Some technical notes on CSP/MDM/AppLocker and interactions with [L]GPO, based on my observations:
* You can define multiple rule sets under different group names. It appears to me that execution of a
  file is allowed only if it is allowed by every rule set.
  This is different from what seems to happen with AppLocker rule sets in GPO, where rule sets in AD
  GPO are merged together and with any in local GPO. Overlapping sets of allow rules in GPO are therefore
  more likely to lead to more permissive results, while overlapping sets of allow rules in CSP/MDM are
  more likely to lead to more restrictive results.

* If different policies are applied through CSP/MDM and through [L]GPO, it appears that execution of a
  file is allowed only if it is allowed by both CSP/MDM and [L]GPO evaluations.

* If identical policies are defined through both CSP/MDM and [L]GPO, each event seems to be recorded twice -- once for each interface.

* It doesn't seem possible in AppLocker/CSP/MDM to set a rule collection to "NotConfigured" and no rules - 
  seems to be the equivalent of "allow nothing."

* The AppLocker CSP interface appears to work on all recent Windows 10/11 SKUs I tried, including Home and Pro.
  However, it doesn't work on Windows 10 v10240 - the original 2015 release. I don't know in which Win10
  version it was introduced.

The `-get` switch outputs any/all CSP-configured AppLocker policies as XML; with `-out` it is written to a UTF8-encoded file.
If there are multiple CSP-configured policies, each policy XML is preceded by its group name.

The `-set` switch applies AppLocker policy from the AppLocker XML UTF8-encoded file specified by `filename`, optionally with a group name following `-gn`.
If no group name is specified, the default group name is `SysNocturnals_Managed`. If a set had already existed with that group name, it is overwritten by the new policy.

The `-deleteall` switch deletes all CSP-configured AppLocker policies.

## Local Group Policy Object (LGPO) operations

LGPO-managed AppLocker policy is visible in the Local Group/Security Policy editors, `Get-AppLockerPolicy -Local` in PowerShell, and is saved in the computer `registry.pol` file
(observable using Microsoft's LGPO.exe tool -- which I also wrote:), and its artifacts can often be observed in the registry and GpResult reports, as long as a domain GPO hasn't overridden it. 

Unlike with CSP and domain GPO, there is at most only one LGPO AppLocker policy.

Note that the `-lgpo` options do not configure the AppIdSvc Windows service, which must be running to enfoce [L]GPO policies.

`-get` outputs an XML document representing LGPO-configured AppLocker policy, optionally to a UTF-encoded file specified via `-out`.
Does not require administrative rights (but a non-admin will get an "access denied" failure if the LGPO directories are not present).

`-set` applies AppLocker policy from the AppLocker XML UTF8-encoded file specified by `filename`. The new policy overwrites any existing LGPO-configured AppLocker policy. 
The `-set` option requires administrative rights.

`-clear` deletes LGPO-configured AppLocker policy, and requires administrative rights.

## Effective Group Policy Object (GPO) operations

`-gpo -get` outputs an XML document representing effective GPO-configured AppLocker policy,
based on evidence in the registry. Effective AppLocker policy is expected to be the merged 
results from Active Directory policies and local GPO. This operation does not require administrative rights.

## Last resort emergency operations

As a last resort, it might be necessary to clear the contents of the directory
containing the files containing effective AppLocker policy, under
C:\Windows\System32\AppLocker. Sometimes files remain when there's no
Group Policy or CSP/MDM policies behind them.

AppLocker's actual policy cache is in binary files in System32\AppLocker. 
Sometimes (it seems) policy artifacts end up in these files and are out of sync
with LGPO and CSP/MDM, and configuration via LGPO or CSP/MDM fails to override or
replace this content. As a VERY LAST RESORT, policy can be removed by
deleting the files in this directory.

The `-911` options provide visibility into the content of that directory, and
the ability to remove it all. The `-list` option does not require administrative rights; the
`-deleteall` option does.
