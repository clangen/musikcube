set(SERVER_SOURCES 
	ConnectedUsersController.cpp
	ConnectedUsersListController.cpp
	ConnectedUsersListModel.cpp
	main.cpp
	MainWindowController.cpp
	pch.cpp
	SyncpathController.cpp
	SyncpathListController.cpp
	SyncpathListModel.cpp
	SyncpathView.cpp
	users/EditUserController.cpp
	users/EditUserView.cpp
	users/UsersController.cpp
	users/UsersListController.cpp
	users/UsersListModel.cpp
	users/UsersView.cpp
	#resources/resource.rc
)

set (SERVER_HEADERS
	ConnectedUsersController.hpp
	ConnectedUsersListController.hpp
	ConnectedUsersListModel.hpp
	MainWindowController.hpp
	pch.hpp
	SyncpathController.hpp
	SyncpathListController.hpp
	SyncpathListModel.hpp
	SyncpathView.hpp
	resources/resource.h
	users/EditUserController.hpp
	users/EditUserView.hpp
	users/UsersController.hpp
	users/UsersListModel.hpp
	users/UsersView.hpp
)

SOURCE_GROUP("Header Files\\users" FILES
	users/EditUserController.hpp
	users/EditUserView.hpp
	users/UsersController.hpp
	users/UsersListModel.hpp
	users/UsersView.hpp)
SOURCE_GROUP("Source Files\\users" FILES
	users/EditUserController.cpp
	users/EditUserView.cpp
	users/UsersController.cpp
	users/UsersListController.cpp
	users/UsersListModel.cpp
	users/UsersView.cpp)
SOURCE_GROUP("Header Files\\resources" FILES 
	resources/resource.h)


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32)
	if(NOT DEFINED MINGW)
        	
	endif(NOT DEFINED MINGW)
else(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fpermissive)
endif(CMAKE_SYSTEM_NAME MATCHES "Windows")

add_definitions( 
  -DXML_STATIC
  -D_CRT_SECURE_NO_DEPRECATE
  -D_DEBUG
)

add_executable( musikServer WIN32 ${SERVER_SOURCES} ${SERVER_HEADERS})
target_link_libraries( musikServer win32cpp musik comctl32)


