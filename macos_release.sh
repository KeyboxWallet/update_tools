git clean . -xdf
cmake -DCMAKE_BUILD_TYPE=Release .
make
macdeployqt update_tools.app/ -dmg
