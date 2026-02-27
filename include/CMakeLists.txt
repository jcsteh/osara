add_subdirectory(fmt)
add_subdirectory(simpleson)

add_library(WDL)
target_sources(WDL
  PRIVATE
    WDL/WDL/jnetlib/httpGet.cpp
    WDL/WDL/jnetlib/connection.cpp
    WDL/WDL/jnetlib/asyncdns.cpp
    WDL/WDL/jnetlib/util.cpp
    WDL/WDL/win32_utf8.c
  PUBLIC 
    FILE_SET libHeaders
    TYPE HEADERS
    BASE_DIRS WDL
)

add_library(tinygettext)
target_sources(
  tinygettext
  PRIVATE
    tinygettext/src/dictionary.cpp
	tinygettext/src/language.cpp
	tinygettext/src/log.cpp
	tinygettext/src/plural_forms.cpp
	tinygettext/src/po_parser.cpp
	tinygettext/src/tinygettext.cpp
  PUBLIC
    FILE_SET libHeaders
    TYPE HEADERS
    BASE_DIRS tinygettext/include
)