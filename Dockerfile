# We start from an ubuntu-14 image, prepared to access through vnc
FROM dorowu/ubuntu-desktop-lxde-vnc:trusty

# Update and install text editors
RUN ["apt-get", "update"]
RUN ["apt-get", "install", "-y", "vim"]
RUN ["apt-get", "install", "-y", "nano"]

# Install gcc
RUN apt-get install build-essential software-properties-common -y && \
add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
apt-get update && \
apt-get install gcc-snapshot -y && \
apt-get update && \
apt-get install gcc-6 g++-6 -y && \
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-6;

# Install SDL (simple directmedia layer) from deb packages
RUN ["apt-get", "install", "-y", "libsdl1.2debian"]
RUN ["apt-get", "install", "-y", "libsdl1.2-dev"]
RUN ["apt-get", "install", "-y", "libsdl1.2-dbg"]
RUN ["apt-get", "install", "-y", "libsdl-ttf2.0-dev"]

WORKDIR /usr/src/spaceinvaders
