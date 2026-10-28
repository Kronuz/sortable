# Regenerates test/vectors.txt and test/reject.txt with the reference model and compares them with the
# committed files. Usage: cmake -DPYTHON=... -DROOT=... -DWORK=... -P check_fixtures.cmake
foreach(spec "vectors;model.py;vectors" "reject;gen_reject.py;")
  list(GET spec 0 name)
  list(GET spec 1 script)
  list(GET spec 2 arg)
  execute_process(COMMAND ${PYTHON} -I ${ROOT}/tools/${script} ${arg}
    OUTPUT_FILE ${WORK}/${name}.regen RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${script} failed")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${WORK}/${name}.regen ${ROOT}/test/${name}.txt RESULT_VARIABLE diff)
  if(NOT diff EQUAL 0)
    message(FATAL_ERROR "test/${name}.txt differs from what ${script} writes")
  endif()
endforeach()
