cmake_minimum_required(VERSION 2.8.3)
project(robot_face)

add_definitions(-std=c++11)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(catkin REQUIRED COMPONENTS
	roscpp
	rospy
)

catkin_package(
	INCLUDE_DIRS
	include/
	${catkin_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR}
	${GLUT_INCLUDE_DIRS}
)

include_directories(
	include/
	${catkin_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR}
	${GLUT_INCLUDE_DIRS}
)

add_executable(robot_face
	src/main.cpp
)

add_dependencies(robot_face ${catkin_EXPORTED_TARGETS})
target_link_libraries(robot_face
	${catkin_LIBRARIES}
	${OPENGL_LIBRARIES}
	${GLUT_LIBRARIES}
	assimp
)
