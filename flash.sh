openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program build/apps/$1.elf verify reset exit"
