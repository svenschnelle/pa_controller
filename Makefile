CLI=/home/svens/arduino-cli/bin/arduino-cli
OBJDUMP=/home/svens/x-tools/arm-none-eabi/bin/arm-none-eabi-objdump
SIZE=/home/svens/x-tools/arm-none-eabi/bin/arm-none-eabi-size
all: compile

compile:
	$(CLI) compile --fqbn arduino:sam:arduino_due_x pa_controller.ino -e --warnings all --optimize-for-debug
	$(SIZE) build/arduino.sam.arduino_due_x/pa_controller.ino.elf
#	$(CLI) compile --fqbn arduino:sam:arduino_due_x pa_controller.ino --only-compilation-database -e

upload: compile
	$(CLI) upload --fqbn arduino:sam:arduino_due_x pa_controller.ino -p /dev/ttyACM0

listing: compile
	$(OBJDUMP) -d build/arduino.sam.arduino_due_x/pa_controller.ino.elf > pa_controller.lst
