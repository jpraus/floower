#!/bin/bash

cd "$(dirname "$0")"

echo "Installing dependencies"

sudo apt-get -y install python3 python3-pip
sudo pip3 install RPi.GPIO Adafruit-CharLCD pyserial esptool

echo "Creating launcher file"

touch planter-starter.sh
chmod +x planter-starter.sh

echo "#!/bin/bash" > planter-starter.sh
echo "" >> planter-starter.sh
echo "export PATH=\"$PATH\"" >> planter-starter.sh
echo "cd `pwd`" >> planter-starter.sh
echo "bash update.sh" >> planter-starter.sh
echo "python3 planter.py" >> planter-starter.sh

if ! grep -q planter-starter /etc/rc.local
then
	echo "Adding to rc.local"
	sudo sed -i -e '$i \su pi -c "bash /home/pi/floower/src/tools/planter-rpi/planter-starter.sh &"\n' /etc/rc.local
else
	echo "Replacing to rc.local"
	sudo sed -i '/planter-starter/c\su pi -c "bash /home/pi/floower/src/tools/planter-rpi/planter-starter.sh &"' /etc/rc.local
fi

echo "Done"