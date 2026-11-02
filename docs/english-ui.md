# English UI support (Issue #2)

The plugin reuses its existing EN/ZH resources and MFC/GDI+ controls. OnInitialize
reads TrafficMonitor's GetLanguageId: Chinese language variants select the
existing Simplified Chinese resources; other languages select English. The
host thread's UI language is not changed. Hosts without OnInitialize use English.

UiLanguage.cpp explicitly loads this DLL's string tables and dialog templates.
DataManager caches resource strings by language and ID so returned pointers
remain stable. Runtime labels, option messages, status text and tooltips use
those resources. UI language is bound before the configured startup worker runs.
Changing the host language takes effect after the host's normal restart.

API language remains independent. Geography, provider data and user-defined
target names are displayed as supplied. Persisted default target names and ISP
normalization strings remain unchanged. The INI schema, item IDs, request
protocols, latency calculation and worker ownership are unchanged.

## Taskbar layout

New profiles use the **Country code** format (`display_format=5`), for example
`GB`, `CN` or `US`. The five existing format values keep their previous meaning;
an explicitly saved choice is not replaced. Select Country code in the options
to switch an existing profile. Missing or invalid format values use the new
default; legacy profiles with only show_ip retain their old interpretation.
A country that cannot be resolved displays `--` rather than a guessed
abbreviation or a long error message. Full location, IP, ISP and diagnostics
remain available in the tooltip and status page.

Default item labels follow the host UI language: English uses `Direct:` and
`Proxy:`, while Chinese keeps `直:` and `代:`. Country codes are the same in both
languages. TrafficMonitor's saved label overrides are preserved, so an existing
custom label may need to be reset in the host when switching languages.

The parser retains country_code/countryCode separately from the full location.
For old provider URLs that omit that field, it matches an exact English, native
or Windows-localized country name against Windows locale data. If a provider
returns only a two-letter country, the tooltip uses its English country name
when Windows can resolve it. No extra network lookup is made. Country codes
survive the existing cached-result retention policy with their associated IP.

GetItemValueSampleText returns the actual bounded display value instead of a
fixed example. TrafficMonitor measures that string using its own font and DPI.
The plugin retains the measured value per rendering thread until the next
measurement when drawing in the taskbar. This prevents a newly published longer
result from overflowing an earlier measurement. The main window reads current
data. TrafficMonitor 1.86's taskbar recalculates layout every second and measures
and paints on its UI thread; other host scheduling models need separate testing.

The region/latency format limits the region to 20 display units before appending
latency. Other item text has a 48-unit limit. Wide characters count as two units;
these limits control compactness, not pixel width. Long values use an ellipsis,
preserving UTF-16 pairs and avoiding a cut inside a combining accent. Raw results,
tooltips and copy actions keep full data. User-customized TrafficMonitor labels
are not overwritten and can still contain text saved in an earlier language.

## Validation

Run tests/localization/Run-Tests.ps1 for resource parity, native hidden EN/ZH
dialogs, language/data separation, startup order and taskbar GDI checks. See its
README for coverage and manual GUI checks. Run tests/concurrency/Run-Tests.ps1
and tests/abi/Run-AbiTest.ps1 separately. Test-only exports are excluded from
ordinary builds.

Build Release|x64 with the project's v145 toolset, MFC/ATL and Windows SDK.
Use /p:PostBuildEventUseInBuild=false when building a test artifact: the original
post-build event deploys to a TrafficMonitor plugins directory. Set separate
OutDir and IntDir for each test run. No working installation is required.

### Visible dialog DPI checks (2026-11-02)

The production x64 DLL was loaded into a standalone Win32 host using the same
system-DPI awareness declared by TrafficMonitor 1.86. Its real options dialog
was inspected on physical monitors with Windows scaling enabled. The host used
loopback responses with test IP addresses and deliberately long geography.

| Scale and screen | EN | ZH | Coverage |
| --- | --- | --- | --- |
| 150%, 3840x2160 | Checked | Checked | All three pages; empty-target validation message |
| 100%, 1680x1050 | Checked | Checked | All three pages after moving from the 150% monitor |
| 200%, 3840x2160 | Checked | Checked | General / Network page; complete outer dialog fits |
| 200%, 1680x1050 | Known limitation | Known limitation | All three pages switch; bottom dialog buttons are clipped |

The 150% to 100% transfer was checked in both languages; the reverse transfer
was checked in English. With the 150% primary monitor, GetDpiForWindow remained
144 during transfer and Windows scaled the system-aware window. At the 200%
stage both monitors were set to 200% and a fresh host process used DPI 192.
A mixed 150% / 200% configuration and the reverse Chinese transfer were not
checked.

On the 1680x1050 screen at 200%, the work area was 954 pixels high. The English
dialog measured 1456x1151 and the Chinese dialog 1566x1117, so their fixed layout
did not fit vertically. This small-screen limitation is accepted for the current
scope; no dialog resizing, scrolling or font changes are included. It must not
be reported as a successful fit or a corrected DPI issue.

Long Region values are clipped in the single-line status cards at all three
tested scales. The complete location and IP remain in the scrollable diagnostic
report. This does not change the compact taskbar format or its full tooltip.

These observations cover the plugin dialog in the standalone host. They do not
complete end-to-end taskbar and tooltip DPI checks in TrafficMonitor itself.
The installed elevated host was not accessible to the window automation tool.
Earlier user checks confirmed the compact panel and English labels after
resetting saved TrafficMonitor label overrides. The automated GDI/tooltip checks
and those confirmations remain separate from the visible-dialog DPI session.

## Pull request scope

The localization branch starts at fix/development-baseline, commit b06453b.
Review its changes against that baseline. A PR directly against the current
upstream main would also include six earlier baseline fixes.

Localization uses the official interface from db54256, synchronized settings
and stable getter strings from 19812bd, and the options task lifetime from
b6e0fd0 (which uses module retention from 9c0507e). Those should be accepted in
the baseline first. Request deadlines (682b4df) and HTTPS hardening (b06453b)
are independent security fixes; they do not belong in the localization diff.
Prefer a separate baseline PR followed by this localization PR, or a stacked
PR targeting the reviewed baseline. No history rewrite or publication is
required to prepare these local changes.
