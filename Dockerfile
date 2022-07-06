FROM alpine:3.15

RUN apk update && apk add --no-cache build-base cmake boost-dev \
	z3-dev llvm11-dev git gcc-riscv-none-elf newlib-riscv-none-elf \
	chicken emacs nano vim gdb-multiarch
RUN adduser -G users -g 'RISC-V VP User' -D riscv-vp
ADD --chown=riscv-vp:users . /home/riscv-vp/riscv-vp
RUN cd /home/riscv-vp/riscv-vp/sisl && chicken-install
RUN su - riscv-vp -c 'make -C /home/riscv-vp/riscv-vp'
CMD su - riscv-vp
