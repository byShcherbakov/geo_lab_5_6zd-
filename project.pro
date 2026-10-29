QT += core gui widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += opengl openglwidgets
CONFIG += c++17
TEMPLATE = app
TARGET = OpenGL_Lab5_Variant2
SOURCES += main.cpp
msvc: QMAKE_CXXFLAGS += /utf-8
