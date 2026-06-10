FROM --platform=linux/amd64 archlinux:latest

RUN echo "DisableSandbox" >> /etc/pacman.conf && \
    pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        gcc \
        make \
        nasm \
        gnu-efi-libs \
        binutils \
        mtools \
        xxd \
        qemu-system-x86 && \
    pacman -Scc --noconfirm

WORKDIR /project