#!/bin/bash

cd "$(dirname "$0")"

sudo apt-get -y install python3 python3-pip
sudo pip3 install RPi.GPIO Adafruit-CharLCD pyserial esptool

touch planter-starter.sh
chmod +x planter-starter.sh

echo "#!/bin/bash" > planter-starter.sh
echo "" >> planter-starter.sh
echo "export PATH=\"$PATH\"" >> planter-starter.sh
echo "cd `pwd`" >> planter-starter.sh
echo "bash update.sh" >> planter-starter.sh
echo "python3 planter.py &" >> planter-starter.sh

(crontab -l 2>/dev/null; echo "@reboot bash `pwd`/planter-starter.sh") | crontab -