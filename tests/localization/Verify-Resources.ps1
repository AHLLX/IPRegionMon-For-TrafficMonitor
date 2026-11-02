$ErrorActionPreference='Stop'
$repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$rc=[IO.File]::ReadAllText((Join-Path $repoRoot 'IPRegionMon.rc'))
$tables=[regex]::Matches($rc,'(?s)STRINGTABLE\s*BEGIN(.*?)END')
if ($tables.Count -ne 2) { throw 'Expected the existing ZH and EN string tables.' }
$values=@()
foreach ($table in $tables) {
    $strings=@{}
    foreach ($entry in [regex]::Matches($table.Groups[1].Value,'(?m)^\s*(IDS_\w+)\s+("(?:[^"\\]|\\.)*")')) {
        if ($strings.ContainsKey($entry.Groups[1].Value)) { throw 'Duplicate string-table ID.' }
        $strings[$entry.Groups[1].Value]=$entry.Groups[2].Value | ConvertFrom-Json
    }
    $values+=,$strings
}
$zhIds=($values[0].Keys | Sort-Object) -join ','
$enIds=($values[1].Keys | Sort-Object) -join ','
if ($zhIds -cne $enIds) { throw 'EN/ZH resource IDs differ.' }
$header=[IO.File]::ReadAllText((Join-Path $repoRoot 'resource.h'))
$numericIds=@{}
foreach ($definition in [regex]::Matches($header,'(?m)^#define\s+(IDS_\w+)\s+(\d+)')) {
    if ($values[0].ContainsKey($definition.Groups[1].Value)) {
        $number=$definition.Groups[2].Value
        if ($numericIds.ContainsKey($number)) { throw "Duplicate numeric string ID: $number" }
        $numericIds[$number]=$definition.Groups[1].Value
    }
}
if ($numericIds.Count -ne $values[0].Count) { throw 'String-table IDs and resource.h do not match.' }
$dialogs=@{}
foreach ($dialog in [regex]::Matches($rc,'(?ms)^(IDD_\w+)\s+DIALOGEX\b.*?^BEGIN\s*$(.*?)^END\s*$')) {
    $name=$dialog.Groups[1].Value
    $ids=([regex]::Matches($dialog.Groups[2].Value,'\b(?:IDC_\w+|IDOK|IDCANCEL)\b') |
        ForEach-Object Value | Sort-Object) -join ','
    if (!$dialogs.ContainsKey($name)) { $dialogs[$name]=@() }
    $dialogs[$name]+=,$ids
}
if ($dialogs.Count -ne 4) { throw 'Expected the four existing dialog templates.' }
foreach ($name in $dialogs.Keys) {
    if ($dialogs[$name].Count -ne 2 -or $dialogs[$name][0] -cne $dialogs[$name][1]) {
        throw "EN/ZH dialog control IDs differ: $name"
    }
}
foreach ($id in $values[0].Keys) {
    if (!$values[0][$id] -or !$values[1][$id]) { throw "Empty localized resource: $id" }
    if ($values[1][$id] -match '[\u4e00-\u9fff]') { throw "Chinese UI text in English resource: $id" }
    $pattern='%(?:[-+ #0]*\d*(?:\.\d+)?)(?:ll|l)?[sdfu]'
    $zh=([regex]::Matches($values[0][$id],$pattern) | ForEach-Object Value) -join ','
    $en=([regex]::Matches($values[1][$id],$pattern) | ForEach-Object Value) -join ','
    if ($zh -cne $en) { throw "Format arguments differ: $id" }
}
# Persisted target names and provider-data normalization are data, not UI labels.
$dataLiterals=@('百度','默认','中国电信','中国联通','中国移动','教育网','科技网')
$sources=@(Get-ChildItem -LiteralPath $repoRoot -Filter '*.cpp' -File) +
    @(Get-ChildItem -LiteralPath $repoRoot -Filter '*.h' -File)
foreach ($file in $sources) {
    $name=$file.Name
    $source=[IO.File]::ReadAllText($file.FullName)
    foreach ($literal in [regex]::Matches($source,'L"(?:[^"\\]|\\.)*"')) {
        $text=$literal.Value.Substring(1) | ConvertFrom-Json
        if ($text -match '[\u4e00-\u9fff]') {
            if ($name -eq 'DataManager.cpp' -and $text -cin $dataLiterals) { continue }
            throw "Unlocalized C++ UI literal in $name : $text"
        }
    }
}
Write-Host "PASS: $($values[0].Count) EN/ZH resource pairs, 4 dialog ID pairs, format arguments, English text and C++ literal inventory"
