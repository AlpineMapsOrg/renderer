# Function to generate the batch script
function(generate_webigeo_eval_batch_script)
	# Output batch file content
    file(WRITE "${CMAKE_BINARY_DIR}/webigeo_eval.bat"
"@echo off
:: Prepend DLL paths to PATH environment variable
set SDL_DLL_DIR=${ALP_SDL_INSTALL_DIR}/bin
set QT_DLL_DIR=${Qt6_DIR}/../../../bin
set OLD_PATH=%PATH%
set PATH=%SDL_DLL_DIR%;%QT_DLL_DIR%;%PATH%

:: Run the executable and capture stdout and stderr
webigeo_eval.exe %* > eval_output.log 2>&1
set PATH=%OLD_PATH%

:: Show the output to the screen
type eval_output.log"
    )

    message(STATUS "Batch script generated at ${CMAKE_BINARY_DIR}/webigeo_eval.bat")
endfunction()
