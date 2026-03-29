# RP2040 Firmware Build Instructions

This project uses the Raspberry Pi Pico SDK with CMake and the ARM GCC toolchain.

Follow these steps to set up the build environment.

---

## 1. Install dependencies (Linux)

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git
```

On Fedora:

```bash
sudo dnf install cmake arm-none-eabi-gcc-cs arm-none-eabi-newlib make git
```

Check installation:

```bash
arm-none-eabi-gcc --version
cmake --version
```

---

## 2. Download the Pico SDK

Clone the SDK somewhere on your system (outside this repo):

```bash
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
```

---

## 3. Set the SDK path environment variable

Set this variable so CMake can find the SDK:

```bash
export PICO_SDK_PATH=~/pico-sdk
```

To make it permanent (bash):

```bash
echo 'export PICO_SDK_PATH=~/pico-sdk' >> ~/.bashrc
source ~/.bashrc
```

---

## 4. Build the project

From the `code/` directory:

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

After building, the firmware file will be generated:

```bash
my_project.uf2
```

---

## 5. Flash to the RP2040

1. Hold the **BOOTSEL** button on the Pico
2. Plug the Pico into USB
3. Release BOOTSEL
4. Copy the UF2 file:

```bash
cp my_project.uf2 /media/$USER/RPI-RP2/
```

(The mount path may vary depending on your system.)

---

## 6. Rebuilding later

After the first build, you usually only need:

```bash
cd code/build
make -j$(nproc)
```

If you change `CMakeLists.txt`, run:

```bash
cmake ..
```

If something breaks, clean build:

```bash
cd code
rm -rf build
mkdir build
cd build
cmake ..
make -j$(nproc)
```
