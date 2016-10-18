#!/bin/bash
# -*- ENCODING: UTF-8 -*-
git clone https://github.com/sisoputnfrba/so-commons-library.git $HOME/TP/so-commons-library
git clone https://github.com/sisoputnfrba/so-nivel-gui-library.git $HOME/TP/so-nivel-gui-library
git clone https://github.com/sisoputnfrba/so-pkmn-utils.git $HOME/TP/so-pkmn-utils

cd $HOME/TP/so-commons-library
sudo make install

sudo apt-get install libncurses5-dev
cd $HOME/TP/so-nivel-gui-library
make && make install

cd $HOME/TP/so-pkmn-utils/src
make all
sudo make install

exit
