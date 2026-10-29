#!/bin/sh
set -e

install -Dm755 build/libheidisql-fix.so /usr/local/lib/libheidisql-fix.so
install -Dm755 build/libfirefox-fix.so /usr/local/lib/libfirefox-fix.so
install -Dm755 build/omarchy-dex-menu /usr/local/bin/omarchy-dex-menu
install -Dm755 bin/omarchy-dex-daemon /usr/local/bin/omarchy-dex-daemon
install -Dm755 bin/heidisql /usr/local/bin/heidisql
install -Dm755 bin/firefox /usr/local/bin/firefox

mkdir -p /root/.config/firefox-omarchy/chrome
cp etc/firefox/userChrome.css /root/.config/firefox-omarchy/chrome/userChrome.css

if [ -f /usr/share/applications/heidisql.desktop ]; then
    sed -i 's|^Exec=.*|Exec=/usr/local/bin/heidisql %F|' /usr/share/applications/heidisql.desktop
fi

if [ -f /usr/share/applications/firefox.desktop ]; then
    sed -i 's|^Exec=.*|Exec=/usr/local/bin/firefox %u|' /usr/share/applications/firefox.desktop
fi

cat << 'EOF' > /etc/profile.d/omarchy-dex.sh
if [ "$(id -u)" = "0" ]; then
    if ! pgrep -f "/usr/local/bin/omarchy-dex-daemon" >/dev/null 2>&1; then
        nohup /usr/local/bin/omarchy-dex-daemon >/dev/null 2>&1 &
    fi
fi
EOF
chmod 644 /etc/profile.d/omarchy-dex.sh
