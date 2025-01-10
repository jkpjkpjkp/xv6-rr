  .align 4
  .globl m_trap_vector

m_trap_vector:
  # We are in M-mode. "unimp" typically kills QEMU if there's no OpenSBI
  unimp               