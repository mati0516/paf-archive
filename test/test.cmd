@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ============================================
echo         PAF Total Function Test
echo ============================================

:: Cleanup
echo [*] Cleaning up...
del /q test_all.paf test_result.log >nul 2>&1
rmdir /s /q out out_single out_dir test_all_data >nul 2>&1

:: Setup
echo [*] Generating test data...
mkdir test_all_data
echo hello > test_all_data\hello.txt
echo world > test_all_data\readme.md
echo data123 > test_all_data\data.csv
type nul > test_all_data\empty.txt
type nul > test_all_data\empty.log
fsutil file createnew test_all_data\dummy.bin 10240 >nul 2>&1
echo 日本語 > test_all_data\日本語.txt
echo spaced file > "test_all_data\space file.md"
mkdir test_all_data\subdir1
echo inner > test_all_data\subdir1\inner.txt
echo deep > test_all_data\subdir1\deep.md
mkdir test_all_data\subdir2\nested
echo verydeep > test_all_data\subdir2\nested\file.log

(
    echo *.txt
    echo 日本語.txt
    echo subdir1\*.txt
) > test_all_data\.pafignore

:: Run test
echo [1/6] Creating archive...
.\test_all.exe > test_result.log 2>&1

:: Show result
type test_result.log

pause
