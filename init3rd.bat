
@ECHO OFF
SETLOCAL
    SET command_flag=false
    IF "%1"==""                     SET command_flag=true & CALL:funInit3rd

    IF %command_flag%==false ECHO "no such command!"
ENDLOCAL

pause

:: end
GOTO:EOF

:funInit3rd
SETLOCAL
    CALL:funGitSubmoduleUpdate
    CALL:funBuild_dd
ENDLOCAL
GOTO:EOF


:funGitSubmoduleUpdate 
SETLOCAL
    echo funGitSubmoduleUpdate
    git submodule update --init --recursive
    git submodule foreach "git fetch origin main && git reset --hard origin/main"
    if %errorlevel% equ 0 (
        echo funGitSubmoduleUpdate succeeded.
    ) else (
        echo funGitSubmoduleUpdate failed.
        GOTO:EOF
    )        
ENDLOCAL
GOTO:EOF

:funBuild_dd
SETLOCAL
    echo funBuild_dd
    pushd 3rd\dd
    echo. | CALL build.bat
    popd
    if %errorlevel% equ 0 (
        echo funBuild_dd succeeded.
    ) else (
        echo funBuild_dd failed.
        GOTO:EOF
    )        
ENDLOCAL
GOTO:EOF
