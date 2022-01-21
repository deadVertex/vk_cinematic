@echo off
if not exist build mkdir build
if not exist build\doxygen mkdir build\doxygen
if not exist build\doxybook2 mkdir build\doxybook2
if not exist build\site mkdir build\doxybook2

doxygen .doxygen
doxybook2 --input build\doxygen\xml --output build\doxybook2 --config .doxybook2
mkdocs build -f mkdocs.yml
