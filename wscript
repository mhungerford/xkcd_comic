
#
# This file is the default set of rules to compile a Pebble project.
#
# Feel free to customize this to your needs.
#

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')
    ctx.exec_command('./gen_animation_style_config.sh')

def build(ctx):
    ctx.load('pebble_sdk')
    ctx.env.CFLAGS=['-std=c99',
                        '-mcpu=cortex-m3',
                        '-mthumb',
                        '-Os',
                        '-g',
                        '-ffunction-sections',
                        '-fdata-sections',
                        #'-funroll-loops',
                        '-Wall',
                        '-Wextra',
                        #'-Werror',
                        '-Wno-unused-parameter',
                        '-Wno-error=unused-function',
                        '-Wno-error=unused-variable' ]
    
    ctx.env.CFLAGS.append('-Wa,-mimplicit-it=always')
    #ctx.env.CFLAGS.append('-ffixed-sp')
    #ctx.env.CFLAGS.append('-ffixed-fp')
    #ctx.env.CFLAGS.append('-fomit-frame-pointer')
    #ctx.env.CFLAGS.append('-ffast-math')
    #ctx.env.CFLAGS.append('-funroll-loops')

    ctx.pbl_program(source=ctx.path.ant_glob('src/**/*.c'),
                    target='pebble-app.elf')

    ctx.pbl_bundle(elf='pebble-app.elf',
                   js=ctx.path.ant_glob('src/js/**/*.js'))
