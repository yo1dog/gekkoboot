project(
    'gekkoboot',
    ['c', 'cpp'],
    meson_version: '>=1.2',
    default_options: {
        'optimization': 's',
        'c_std': 'gnu2x',
    },
)

emu_build = get_option('emu_build')

platform = 'cube'
libogc_libs = ['ogc']

if emu_build
    platform = 'wii'
    libogc_libs += ['bte', 'wiiuse']
endif

libogc_proj = subproject(
    'libogc2',
    default_options: {
        'platform': platform,
        'libraries': libogc_libs,
    },
)
libogc_deps_dic = libogc_proj.get_variable('deps')
libogc_deps = []
foreach lib : libogc_libs
    libogc_deps += [libogc_deps_dic[lib]]
endforeach

if emu_build
    libfat_proj = subproject('libfat')
    libfat_dep = libfat_proj.get_variable('dep')
endif

subdir('buildtools')
subdir('res')
objcopy = find_program('objcopy')
elf2dol = find_program('elf2dol')

# Add executables to this mapping to generate corresponding DOLs.
# The variable '@key@_dol' will be set for later use.
dols = {}

# Add executables to this mapping as {'exe':, 'link_args':, 'dol':} to compress them.
# The variable '@key@_compressed' will be set for later use.
# The optional key 'dol' is a boolean that controls whether to add the compressed
# executable to `dols`.
compressed_exes = {}

subdir('stub')

git = find_program('git')
version_file = vcs_tag(
    command: [git, 'describe', '--always', '--tags', '--dirty'],
    input: 'source/version.c.in',
    output: 'version.c',
)

font = custom_target(
    'font',
    input: terminus,
    output: 'font.c',
    command: [psf2c, '@INPUT@', '@OUTPUT@', 'console_font_8x16'],
)

gekkoboot_link_args = []
gekkoboot_link_depends = []
if emu_build
    # Always link the math library because it is used by wiiuse.
    # It is never explicitly included so the --as-needed flag would omit it otherwise.
    gekkoboot_link_args += ['-lm']
else
    linker_script = meson.current_source_dir() / 'ogc.ld'
    gekkoboot_link_args += ['-T' + linker_script]
    gekkoboot_link_depends += [linker_script]
endif

gekkoboot = executable(
    'gekkoboot',
    'source/main.c',
    'source/filesystem.c',
    'source/shortcut.c',
    'source/cli_args.c',
    'source/config.c',
    'source/types.c',
    'source/fatfs/ff.c',
    'source/fatfs/ffsystem.c',
    'source/fatfs/ffunicode.c',
    'source/ffshim.c',
    'source/inih/ini.c',
    version_file,
    font,
    dependencies: (
        libogc_deps +
        [stub_dep] +
        (emu_build? [libfat_dep] : [])
    ),
    link_args: gekkoboot_link_args,
    link_depends: gekkoboot_link_depends,
    name_suffix: 'elf',
)

if emu_build
    dols += {'gekkoboot_emu': gekkoboot}
else
    compressed_exes += {
        'gekkoboot': {
            'exe': gekkoboot,
            'link_args': [
                '-Wl,--section-start,.init=0x01300000'
            ],
            'dol': true,
        },
        'gekkoboot_sx': {
            'exe': gekkoboot,
            'link_args': [
                # This is the same entry point as the original BIOS,
                # but the recovery slot hangs on "starting flashed app..."
                # when loading it. A lowmem image works just fine.
                #'-Wl,--section-start,.init=0x01500000',

                # Makes the ELF smaller so it actually fits
                '-Wl,--nmagic',
            ],
        },
        'gekkoboot_lowmem': {
            'exe': gekkoboot,
            'dol': true,
        },
    }

    subdir('packer')
endif

foreach name, exe: dols
    dol = custom_target(
        name + '_dol',
        input: exe,
        output: name + '.dol',
        command: [elf2dol, '@INPUT@', '@OUTPUT@'],
        build_by_default: true,
    )
    set_variable(name + '_dol', dol)
endforeach

if not emu_build
    if full_rom_opt.allowed()
        qoob_pro = custom_target(
            'qoob_pro',
            input: [gekkoboot_dol, ipl_rom],
            output: 'gekkoboot_qoob_pro.gcb',
            command: [dol2ipl, '@OUTPUT@', '@INPUT@'],
            build_by_default: true,
            install: true,
            install_dir: '/qoob_pro',
        )
        qoob_pro_updater_tgt = custom_target(
            'qoob_pro_updater',
            input: [qoob_pro, qoob_pro_updater],
            output: 'qoob_pro_gekkoboot_upgrade.elf',
            command: [qoob_injector, '@INPUT@', '@OUTPUT@'],
            build_by_default: true,
            install: true,
            install_dir: '/qoob_pro',
        )
    endif

    gekkoboot_sx_stripped = custom_target(
        gekkoboot_sx_compressed.name() + '_stripped',
        input: gekkoboot_sx_compressed,
        output: gekkoboot_sx_compressed.name() + '_stripped.elf',
        command: [objcopy, '-S', '@INPUT@', '@OUTPUT@'],
    )
    qoob_sx = custom_target(
        'qoob_sx',
        input: gekkoboot_sx_stripped,
        output: 'gekkoboot_qoob_sx.qbsx',
        command: [dol2ipl, '@OUTPUT@', '@INPUT@'],
    )
    qoob_sx_updater_tgt = custom_target(
        'qoob_sx_updater',
        input: [qoob_sx, qoob_sx_updater],
        output: 'qoob_sx_gekkoboot_upgrade.elf',
        command: [qoob_injector, '@INPUT@', '@OUTPUT@'],
        build_by_default: true,
        install: true,
        install_dir: '/',
    )

    viper = custom_target(
        'viper',
        input: gekkoboot_dol,
        output: 'gekkoboot_viper.vgc',
        command: [dol2ipl, '@OUTPUT@', '@INPUT@'],
        build_by_default: true,
        install: true,
        install_dir: '/',
    )

    pico = custom_target(
        'pico',
        input: gekkoboot_dol,
        output: 'gekkoboot_pico.uf2',
        command: [dol2ipl, '@OUTPUT@', '@INPUT@'],
        build_by_default: true,
        install: true,
        install_dir: '/',
    )

    gci = custom_target(
        'gci',
        input: gekkoboot_lowmem_dol,
        output: 'gekkoboot_memcard.gci',
        command: [dol2gci, '@INPUT@', '@OUTPUT@', 'boot.dol'],
        build_by_default: true,
        install: true,
        install_dir: '/',
    )

    apploader = custom_target(
        'apploader',
        input: gekkoboot_dol,
        output: 'apploader.img',
        command: [dol2ipl, '@OUTPUT@', '@INPUT@'],
        build_by_default: true,
        install: true,
        install_dir: '/swiss_igr/swiss/patches',
    )

    install_data('README.md', install_dir: '/')
endif
