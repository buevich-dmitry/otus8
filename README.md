To download and install the latest version, run:
```
wget -O otus8-Linux.deb https://github.com/buevich-dmitry/otus8/releases/latest/download/otus8-Linux.deb
sudo dpkg -i otus8-Linux.deb
```

Also you can build the project locally:
```
./build.sh
sudo dpkg -i bin/otus8-*-Linux.deb
```

Then run otus8 app:
```
otus8 <block_size>
```
