# - Try to find the sqlite library
# Once done this will define
#
#  SQLITE_FOUND - system has sqlite
#  SQLITE_INCLUDE_DIRS - the sqlite include directory
#  SQLITE_LIBRARIES - Link these to use sqlite
#
#  Copyright (c) 2008 Bjoern Ricks <bjoern.ricks@googlemail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#       

INCLUDE( FindPkgConfig )

IF ( Sqlite_FIND_REQUIRED )
        SET( _pkgconfig_REQUIRED "REQUIRED" )
ELSE( Sqlite_FIND_REQUIRED )
        SET( _pkgconfig_REQUIRED "" )   
ENDIF ( Sqlite_FIND_REQUIRED )

IF ( SQLITE_MIN_VERSION )
        PKG_SEARCH_MODULE( SQLITE ${_pkgconfig_REQUIRED} sqlite>=${SQLITE_MIN_VERSION} )
ELSE ( SQLITE_MIN_VERSION )
        PKG_SEARCH_MODULE( SQLITE ${_pkgconfig_REQUIRED} sqlite )
ENDIF ( SQLITE_MIN_VERSION )


IF( NOT SQLITE_FOUND AND NOT PKG_CONFIG_FOUND )
        FIND_PATH( SQLITE_INCLUDE_DIRS sqlite.h )
        FIND_LIBRARY( SQLITE_LIBRARIES sqlite )

        # Report results
        IF ( SQLITE_LIBRARIES AND SQLITE_INCLUDE_DIRS )
                SET( SQLITE_FOUND 1 )
                IF ( NOT Sqlite_FIND_QUIETLY )
                        MESSAGE( STATUS "Found Sqlite: ${SQLITE_LIBRARIES}" )
                ENDIF ( NOT Sqlite_FIND_QUIETLY )
        ELSE ( SQLITE_LIBRARIES AND SQLITE_INCLUDE_DIRS )       
                IF ( Sqlite_FIND_REQUIRED )
                        MESSAGE( SEND_ERROR "Could NOT find Sqlite" )
                ELSE ( Sqlite_FIND_REQUIRED )
                        IF ( NOT Sqlite_FIND_QUIETLY )
                                MESSAGE( STATUS "Could NOT find Sqlite" )       
                        ENDIF ( NOT Sqlite_FIND_QUIETLY )
                ENDIF ( Sqlite_FIND_REQUIRED )
        ENDIF ( SQLITE_LIBRARIES AND SQLITE_INCLUDE_DIRS )     
ENDIF( NOT SQLITE_FOUND AND NOT PKG_CONFIG_FOUND )

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED( SQLITE_LIBRARIES SQLITE_INCLUDE_DIRS )
