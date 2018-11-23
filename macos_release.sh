git clean . -xdf
cmake -DCMAKE_BUILD_TYPE=Release . &&
make &&
macdeployqt update_tools.app &&
lrelease i18n/*.ts &&
cp i18n/*.qm update_tools.app/Contents/Resources &&
codesign --deep --force --verbose --sign 'Developer ID App' update_tools.app/ &&
appdmg dmg_spec.json update_tools.dmg 

