git clean . -xdf
cmake -DCMAKE_BUILD_TYPE=Release . &&
make &&
codesign --deep --force --verbose --sign 'Developer ID App' update_tools.app/ &&
appdmg dmg_spec.json update_tools.dmg 

