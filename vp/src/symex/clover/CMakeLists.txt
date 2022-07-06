subdirs(klee)

add_library(clover solver.cpp bitvector.cpp concolic.cpp trace.cpp
	intval.cpp node.cpp memory.cpp context.cpp testcase.cpp)
set_property(TARGET clover PROPERTY CXX_STANDARD 17)
target_include_directories(clover PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(clover PUBLIC kleaverSolver)
