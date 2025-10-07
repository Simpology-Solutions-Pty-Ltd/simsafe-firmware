#! /bin/bash

if [ "$EUID" -ne 0 ]; then
  echo "Error: This script must be run as root"
  echo "Please run with: sudo $0"
  exit 1
fi

apt update

echo "Adding PostgreSQL repo to apt"

apt install curl ca-certificates
install -d /usr/share/postgresql-common/pgdg
curl -o /usr/share/postgresql-common/pgdg/apt.postgresql.org.asc --fail https://www.postgresql.org/media/keys/ACCC4CF8.asc

. /etc/os-release
sh -c "echo 'deb [signed-by=/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc] https://apt.postgresql.org/pub/repos/apt $VERSION_CODENAME-pgdg main' > /etc/apt/sources.list.d/pgdg.list"

apt update

echo "Installing required packages"

apt install postgresql-17 postgresql-server-dev-17 make gpiod libgpiod-dev g++

echo "Cloning and installing libpqxx"

git clone https://github.com/jtv/libpqxx.git

cd libpqxx

./configure --disable-shared

make

make install

cd ..

rm -rf libpqxx

echo "Compiling sisafe firmware"

make

./service_setup.sh

echo "All done!"