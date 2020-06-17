@echo off
if not exist "VisualStudio" mkdir VisualStudio
cd VisualStudio

cmake -A x64 ../
pause