
if (Test-Path -Path ".\out.txt") 
{
    Remove-Item -Path ".\out.txt"
}

(git status | Out-String) -split "`r`n" | Select-String deleted > out.txt

Get-Content -Path ".\out.txt" | ForEach-Object {
    $string = $_
    $newString = $string -replace "deleted:    ", ""
    $newString = $newString -replace "^\s+", ""
    #Write-Host $newString

    git rm $newString
}

Remove-Item -Path ".\out.txt"