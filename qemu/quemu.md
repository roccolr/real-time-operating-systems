# Quick emulator

## install
```bash
# Installa QEMU (system mode) e utility per la gestione dischi
sudo apt update
sudo apt install qemu-system-x86 qemu-utils

# Verifica che il tuo host supporti KVM (serve virtualizzazione hardware attiva nel BIOS)
kvm-ok    # oppure: ls /dev/kvm
```


## Emulazione pura
in questo caso QEMU usa il TCG per tradurre ogni istruzione del guest (molto lento).

```bash
qemu-system-x86_64 \
  -m 512 \
  -cdrom qualche_iso_leggero.iso \
  -boot d
```

## Emulazione accelerata da KVM 

```bash
qemu-system-x86_64 \
  -m 2048 \
  -enable-kvm \
  -smp 2 \
  -cdrom qualche_iso_leggero.iso \
  -boot d
```

## Monitoring 

```bash
ps -eLf | grep qemu-system   # vedrai i thread: main thread + un thread per ogni vCPU
top -H -p $(pgrep qemu-system-x86_64)   # -H mostra i thread singoli
```

## User mode emulation

```bash
sudo apt install qemu-user
file /bin/ls   # binario nativo x86_64

# scarica/compila un binario ARM di prova, poi:
qemu-arm ./programma_arm
```


