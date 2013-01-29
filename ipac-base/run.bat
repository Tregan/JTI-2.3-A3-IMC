@echo on
cd "C:\Users\Robin\Desktop\IMC Project\JTI-2.3-A3-IMC\ipac-base"
make
cd "C:\Program Files (x86)\Streamit\STP-light"
tasklist /FI "IMAGENAME eq STP-light.exe" | grep STP-light.exe
if ERRORLEVEL 1 start STP-light.exe C:\Users\Robin\Desktop\IMC Project\JTI-2.3-A3-IMC\ipac-base\ipac.hex
