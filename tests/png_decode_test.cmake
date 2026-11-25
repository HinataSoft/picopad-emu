# Spusti png_decode.py nad soubory zapsanymi testem png (cwd = build adresar).
# Parametry: -DPYTHON=... -DDECODE=...
function(run_decode)
    execute_process(COMMAND ${PYTHON} -I ${DECODE} ${ARGN}
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "png_decode.py ${ARGN} failed (${rc}): ${out}${err}")
    endif()
endfunction()
run_decode(png_two.png 0 0 255 0 0 1 0 0 255 0)
run_decode(png_stride.png 0 0 255 0 0 1 0 0 255 0)
run_decode(--sample png_big.png)
