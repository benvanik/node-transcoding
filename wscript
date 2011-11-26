#!/usr/bin/env python

import sys
import os

import json
package = json.load(open('package.json'))
NAME = package['name']
APPNAME = 'node-' + NAME
VERSION = package['version']

srcdir = '.'
blddir = 'build'

def set_options(opt):
  opt.tool_options('compiler_cxx')
  opt.tool_options('node_addon')

def configure(conf):
  conf.check_tool('compiler_cxx')
  conf.check_tool('node_addon')

  conf.env.append_unique('CXXFLAGS', ['-D__STDC_CONSTANT_MACROS'])

  conf.check(header_name='libavformat/avformat.h', mandatory=True)
  conf.check(header_name='libavcodec/avcodec.h', mandatory=True)
  conf.check(lib='avutil', uselib_store='LIBAVUTIL')
  conf.check(lib='avformat', uselib_store='LIBAVFORMAT')
  conf.check(lib='avcodec', uselib_store='LIBAVCODEC')

def build(bld):
  t = bld.new_task_gen('cxx', 'shlib', 'node_addon')
  t.target = 'node_transcoding'
  t.cxxflags = ['-D__STDC_CONSTANT_MACROS']
  t.uselib = ['LIBAVUTIL', 'LIBAVFORMAT', 'LIBAVCODEC']
  t.source = [
    'src/binding.cpp',
    'src/mediainfo.cpp',
    'src/profile.cpp',
    'src/query.cpp',
    'src/querycontext.cpp',
    'src/task.cpp',
    'src/taskcontext.cpp',
    'src/io/filereader.cpp',
    'src/io/filewriter.cpp',
    'src/io/io.cpp',
    'src/io/iohandle.cpp',
    'src/io/streamreader.cpp',
    'src/io/streamwriter.cpp',
  ]
