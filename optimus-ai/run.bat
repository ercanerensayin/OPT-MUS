@echo off
REM OPTIMUS AI - Windows kurulum ve calistirma
REM Bu dosyayi cift tiklayabilir veya cmd icinde "run.bat" yazabilirsin.
setlocal

cd /d "%~dp0"

REM Python launcher (py) varsa onu tercih et, yoksa python.
set "PY=python"
where py >nul 2>nul && set "PY=py -3"

%PY% --version >nul 2>nul
if errorlevel 1 (
  echo [HATA] Python bulunamadi.
  echo        https://www.python.org/downloads/ adresinden kur ve
  echo        kurulum sirasinda "Add Python to PATH" secenegini isaretle.
  goto :fail
)

if not exist ".venv" (
  echo . sanal ortam kuruluyor
  %PY% -m venv .venv
  if errorlevel 1 goto :fail
)

set "VPY=.venv\Scripts\python.exe"
if not exist "%VPY%" (
  echo [HATA] Sanal ortam bozuk. .venv klasorunu sil ve tekrar dene.
  goto :fail
)

echo . bagimliliklar yukleniyor
"%VPY%" -m pip install --quiet --upgrade pip
"%VPY%" -m pip install --quiet -r requirements.txt
if errorlevel 1 goto :fail

if not exist ".env" (
  copy ".env.example" ".env" >nul
  echo.
  echo . .env dosyasi olusturuldu.
  echo   Not Defteri ile ac, ANTHROPIC_API_KEY satirini doldur,
  echo   kaydet ve bu dosyayi tekrar calistir.
  echo.
  goto :done
)

where ffmpeg >nul 2>nul
if errorlevel 1 (
  echo [uyari] ffmpeg bulunamadi - video ve kurgu calismaz.
  echo         Kurmak icin: winget install Gyan.FFmpeg
  echo         Kurduktan sonra bu pencereyi kapatip yeniden ac.
  echo.
)

echo . sunucu baslatiliyor - tarayicida http://127.0.0.1:8000
echo   durdurmak icin Ctrl+C
echo.
"%VPY%" -m optimus_ai.server

:done
echo.
pause
exit /b 0

:fail
echo.
echo Kurulum basarisiz oldu.
pause
exit /b 1
