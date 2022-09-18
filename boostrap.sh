if [ -z $PICO_SDK_PATH ] ; then
  echo -e "\e[31mPICO_SDK_PATH not found, did you set up the Pico SDK properly?\e[0m"
  exit 1
fi

mkdir -p lib

if ! test -e lib/compat ; then
    echo -e "\e[34mFetching the pico-arduino-compat library...\e[0m"
    ( cd lib ; git clone https://github.com/fhdm-dev/pico-arduino-compat.git compat )
    git -C lib/compat checkout 047f819e
    git -C lib/compat submodule update --init arduino-compat/arduino-pico
fi

if ! test -e lib/RadioHead ; then
    echo -e "\e[34mFetching the RadioHead library...\e[0m"
    ( cd lib ; wget http://www.airspayce.com/mikem/arduino/RadioHead/RadioHead-1.121.zip )
    echo -e "\e[34mUnzipping...\e[0m"
    ( cd lib ; unzip -q RadioHead-1.121.zip )
fi

if ! test -e lib/ookDecoder ; then
    echo -e "\e[34mFetching my fork of the ookDecoder library...\e[0m"
    ( cd lib ; git clone https://github.com/Cactusbone/ookDecoder ; git checkout pico-rfm69-ook-experiments)
fi





mkdir -p build
cd build
cmake ..
