shellcheck_exe = find_program('shellcheck', required: true)

test('shellcheck-rdi-installer',
  shellcheck_exe,
  args: ['-a', '-x', rdi_installer]
)

test('shellcheck-rdii-ssh-setup',
  shellcheck_exe,
  args: [files('../scripts/rdii-ssh-setup.sh')]
)
