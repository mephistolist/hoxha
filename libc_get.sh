#/bin/sh

A=$(dpkg -l | awk '/libc-bin/ { print $2 }'); B=$(dpkg -l | awk '/libc-bin/ { print $3 }'); PKG=$A\_$B\_amd64.deb;

QUERY=$(printf "%s debian package site:packages.debian.org" "$PKG" | sed 's/ /+/g')

WRAPPED_URL=$(curl -sL "https://duckduckgo.com/html/?q=$QUERY" | grep -oP 'href="\K//duckduckgo.com/l/\?uddg=[^"]+' | head -n1); REAL_URL=$(printf '%s\n' "$WRAPPED_URL" | sed -E 's/.*uddg=//;s/&amp.*//' | python3 -c 'import sys, urllib.parse as u; print(u.unquote(sys.stdin.read()))'); echo "$REAL_URL"

GET_IT=$(curl -sL "$REAL_URL" | awk -v pkg="$PKG" -F'"' '$0 ~ pkg && /href/ { print $2; exit }')

curl -sLOk $GET_IT;
