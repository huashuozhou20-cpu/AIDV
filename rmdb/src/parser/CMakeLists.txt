# Flex/Bison are NOT required — generated sources (lex.yy.cpp, yacc.tab.cpp,
# yacc.tab.h) are checked into the repository.  This avoids a hard dependency on
# flex and bison at build time.  If you modify lex.l or yacc.y, regenerate the
# sources manually before committing.
set(PARSER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/yacc.tab.cpp
    ast.cpp
)
add_library(parser STATIC ${PARSER_SOURCES})

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser parser)
add_test(NAME test_parser COMMAND test_parser
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
