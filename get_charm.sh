sudo rm -rf __get_charming
sudo rm __get_charming
mkdir __get_charming
cd __get_charming
git clone https://github.com/zweistein-frm2/CHARMing.git
cd CHARMing
git submodule update --init --recursive
chmod +x buildpackages.sh
./buildpackages.sh
