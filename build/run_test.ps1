$env:Path = 'D:\inASt\qt\5.15.2\msvc2019_64\bin;' + $env:Path
$out = & 'd:\code\.std\opengl\build\my_demo\Debug\test_gameplay.exe' 2>&1
Write-Output ("EXIT CODE: " + $LASTEXITCODE)
Write-Output "--- last 8 lines ---"
$out | Select-Object -Last 8
