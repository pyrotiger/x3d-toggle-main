#!/bin/sh
# Optional GTK4 GUI Compilation Script for X3D Toggle
# Called conditionally by setup.sh

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

if [ "$X3D_EXEC" != "1" ]; then
    journal_write -37
fi

if [ -z "$X3D_XUI_LOADED" ]; then
    printf "    \033[31mError:\033[0m XUI framework (xui.sh) could not be initialized."
    printf ""
    printf "    Please ensure scripts/framework/xui.sh exists and is valid."
    exit 1
fi

if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists gtk4 libadwaita-1; then
    printf_step "2,${XOUT} Error: GTK4 Development Libraries Missing" \
                "2,Please install 'gtk4' and 'libadwaita-1' development headers."
    exit 1
fi

printf_center "X3D GUI COMPONENT CONSTRUCTOR"
printf_divider

printf_step "2,${GEAR} Compiling GTK4 GResource manifest..."
if ! glib-compile-resources --sourcedir=src/gtk4 \
    --generate-source --target=src/gtk4/x3d-gui-resources.c \
    src/gtk4/x3d-toggle-gui.gresource.xml; then
    printf_step "2,${XOUT} Error: GResource Compilation Failed"
    exit 1
fi

printf_step "2,${HAMMER} Compiling x3d-gui autonomous binary..."
if ! clang $(pkg-config --cflags gtk4 libadwaita-1) -Wall -O2 \
    -Iinclude -Ibuild -DLIBC_NO_BOOTSTRAP -Wl,--allow-multiple-definition \
    src/gtk4/gui.c src/gtk4/x3d-gui-resources.c src/gtk4/stubs.c \
    src/libc.c src/worker.c build/xui.c src/error.c \
    -o x3d-gui \
    $(pkg-config --libs gtk4 libadwaita-1); then
    printf_step "2,${XOUT} Error: Compilation Failed"
    rm -f src/gtk4/x3d-gui-resources.c
    exit 1
fi

printf_step "2,${PACKAGE} Installing x3d-gui to system path..."
if [ "$(id -u)" -eq 0 ]; then
    install -m755 x3d-gui /usr/bin/x3d-gui
else
    printf_step "2,${WARN} Warning: Root privileges required for installation."
    printf_step "2,Run with sudo to finalize installation."
fi

# Clean up build artifacts
rm -f x3d-gui src/gtk4/x3d-gui-resources.c

patch_desktop() {
    _file="$1"
    if [ -f "$_file" ]; then
        _tmp="${_file}.tmp"
        while IFS= read -r line; do
            case "$line" in
                Exec=*) printf "Exec=x3d-gui\n" ;;
                *)      printf "%s\n" "$line" ;;
            esac
        done < "$_file" > "$_tmp"
        chmod --reference="$_file" "$_tmp" 2>/dev/null || chmod 644 "$_tmp"
        mv "$_tmp" "$_file"
    fi
}

# Patch desktop entry
DESKTOP_SYS="/usr/share/applications/x3d-toggle.desktop"
if [ -f "$DESKTOP_SYS" ]; then
    if [ "$(id -u)" -eq 0 ]; then
        patch_desktop "$DESKTOP_SYS"
        printf_step "2,${ALRIGHT} System desktop entry patched to launch x3d-gui."
    fi
fi

if [ -n "$ACTUAL_USER" ]; then
    DESKTOP_USR="/home/$ACTUAL_USER/Desktop/x3d-toggle.desktop"
    if [ -f "$DESKTOP_USR" ]; then
        patch_desktop "$DESKTOP_USR"
        printf_step "2,${ALRIGHT} User desktop entry patched to launch x3d-gui."
    fi
fi

printf_br
printf_center "${ALRIGHT} GTK4 GUI SUCCESSFULLY BUILT AND INSTALLED ${ALRIGHT}"
printf_step "4,${GEAR} You can now launch the GUI by running 'x3d-gui' or from your application menu."
printf_signature
exit 0
