@echo off
echo.
echo ===========================================================
echo      BUILDING WASM (PRO: GPU INSTANCING + WEBGL2)
echo ===========================================================
echo.
setlocal enabledelayedexpansion
set EMCC=emcc
set LOGFILE=build_log.txt
set CFLAGS=-std=c11 -I./ta-lib/include -I./ta-lib/src/ta_common -I./ta-lib/src/ta_func -O3 -c

echo [LOG] Build started > %LOGFILE%
echo [STEP 0] Compiling ALL TA-Lib C files...

set OBJ_FILES=

echo   ta_common ...
for %%f in (ta-lib\src\ta_common\*.c) do (
    call %EMCC% "%%f" %CFLAGS% -o "%%~nf.o" >> %LOGFILE% 2>&1
    if errorlevel 1 goto :fail
    set "OBJ_FILES=!OBJ_FILES! %%~nf.o"
)

echo   ta_func ... (tunggu ~2 menit)
for %%f in (ta-lib\src\ta_func\*.c) do (
    call %EMCC% "%%f" %CFLAGS% -o "%%~nf.o" >> %LOGFILE% 2>&1
    if errorlevel 1 goto :fail
    set "OBJ_FILES=!OBJ_FILES! %%~nf.o"
)

echo   TA-Lib OK!
goto :step1

:fail
echo.
echo   [ERROR] TA-Lib FAILED on %%f
echo   Cek %LOGFILE%
pause
exit /b 1

:step1
echo [STEP 1] Collecting CPP files...
set CPP_FILES=main.cpp GPUCandleRenderer.cpp stb_image_impl.cpp

for %%f in (*.cpp) do (
    if not "%%f"=="main.cpp" if not "%%f"=="GPUCandleRenderer.cpp" if not "%%f"=="stb_image_impl.cpp" (
        set CPP_FILES=!CPP_FILES! %%f
    )
)

set CPP_FILES=!CPP_FILES! imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp
set CPP_FILES=!CPP_FILES! imgui\backends\imgui_impl_glfw.cpp imgui\backends\imgui_impl_opengl3.cpp
set CPP_FILES=!CPP_FILES! implot\implot.cpp implot\implot_items.cpp

echo [STEP 2] Scanning assets...
set PRELOAD=
if exist Roboto.ttf set PRELOAD=!PRELOAD! --preload-file Roboto.ttf
if exist times.ttf set PRELOAD=!PRELOAD! --preload-file times.ttf
if exist code.ttf set PRELOAD=!PRELOAD! --preload-file code.ttf
if exist seguisym.ttf set PRELOAD=!PRELOAD! --preload-file seguisym.ttf
for %%F in (candles_*.cache) do set PRELOAD=!PRELOAD! --preload-file %%F
if exist imgui.ini set PRELOAD=!PRELOAD! --preload-file imgui.ini
if exist assets set PRELOAD=!PRELOAD! --preload-file assets
if exist Candle.h set PRELOAD=!PRELOAD! --preload-file Candle.h@/Candle.h
if exist Indicators.h set PRELOAD=!PRELOAD! --preload-file Indicators.h@/Indicators.h
if exist TradeModule.h set PRELOAD=!PRELOAD! --preload-file TradeModule.h@/TradeModule.h
if exist GlobalShapeManager.h set PRELOAD=!PRELOAD! --preload-file GlobalShapeManager.h@/GlobalShapeManager.h
if exist CreatorEngine.h set PRELOAD=!PRELOAD! --preload-file CreatorEngine.h@/CreatorEngine.h
if exist MultiChart.h set PRELOAD=!PRELOAD! --preload-file MultiChart.h@/MultiChart.h
if exist GPUCandleRenderer.h set PRELOAD=!PRELOAD! --preload-file GPUCandleRenderer.h@/GPUCandleRenderer.h
if exist GPUCandleShaders.h set PRELOAD=!PRELOAD! --preload-file GPUCandleShaders.h@/GPUCandleShaders.h

echo [STEP 3] Compiling C++ + linking... (tunggu 5-15 menit)
echo STEP 3 start >> %LOGFILE%

call %EMCC% %CPP_FILES% %OBJ_FILES% -std=c++17 -DIMGUI_USE_WCHAR32 -I. -I./glad/include -I./imgui -I./imgui/backends -I./implot -I./ta-lib/include %PRELOAD% -s FETCH=1 -s USE_GLFW=3 -s USE_WEBGL2=1 -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH=1 -s FORCE_FILESYSTEM=1 -lidbfs.js -s DISABLE_EXCEPTION_CATCHING=0 -s ASYNCIFY=1 -s WASM=1 -s ASSERTIONS=1 -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,FS -s EXPORTED_FUNCTIONS=_main,_LoadSettings,_wasm_login_success,_wasm_rebuild_all_htfs,_wasm_push_candle,_wasm_push_tick,_wasm_pan_chart_pixels,_wasm_zoom_chart,_wasm_notify_touch_start,_wasm_mouse_move,_wasm_mouse_click,_wasm_mouse_wheel,_wasm_send_char,_wasm_send_key,_malloc,_free,_InjectFeature,_ToggleCreatorMode,_wasm_push_candle_for_tab,_wasm_rebuild_htfs_for_tab,_wasm_clear_tab,_wasm_push_candle_for_symbol,_wasm_set_primary_loading,_wasm_begin_prepend,_wasm_prepend_candle,_wasm_end_prepend,_wasm_set_lazy_load_done,_wasm_get_oldest_loaded_time,_wasm_clear_chart,_wasm_save_view_anchor,_wasm_restore_view_anchor,_wasm_set_tab_lazy_done,_wasm_set_tab_no_more_history,_wasm_save_view_anchor_tab,_wasm_restore_view_anchor_tab,_wasm_clear_orderbook,_wasm_push_orderbook_level,_wasm_push_ob_snapshot,_wasm_clear_ob_snapshot,_wasm_push_footprint,_wasm_get_replay_gate,_wasm_cancel_replay,_jarvis_on_response,_jarvis_on_error,_jarvis_send_to_api -O3 -o index.js >> %LOGFILE% 2>&1

if errorlevel 1 (
    echo.
    echo   [ERROR] Build FAILED! Cek %LOGFILE%
    pause
    exit /b 1
)

del ta_*.o 2>nul

echo.
echo   BUILD SUCCESS! Output: index.js + index.wasm
pause