gen = [
  decodetree.process('insn.decode', extra_args: [ '--decode', 'decode_insn',
                                                  '--varinsnwidth', '32'  ])
]

avr32_ss = ss.source_set()
avr32_softmmu_ss = ss.source_set()

avr32_ss.add(gen)
avr32_ss.add(files(
  'cpu.c',
  'disas.c',
  'helper.c',
  'helper_conditions_sflags.c',
  'translate.c'
  ))

avr32_softmmu_ss.add(files('machine.c'))

target_arch += {'avr32': avr32_ss}
target_softmmu_arch += {'avr32': avr32_softmmu_ss}
