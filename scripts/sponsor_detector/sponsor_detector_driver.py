#!/usr/bin/python
# coding: UTF-8

import os
import subprocess


_RESULT_FILENAME = 'sponsor.txt'
_CONVERTED_FILENAME_PREFIX = 'converted_'
_TESSERACT_WHITELIST = u'提供' + u''.join(
    [unichr(c) for c in range(ord(u'あ'), ord(u'ん') + 1)] +
    [unichr(c) for c in range(ord(u'ァ'), ord(u'ン') + 1)])


def GetSponsorMarkDumpDirname():
    dirname = 'sponsor_dump'
    if not os.path.isdir(dirname):
        raise IOError('%s should be a directory' % dirname)
    return dirname


def GetTargetFiles():
    dirname = GetSponsorMarkDumpDirname()
    filenames = ['%s/%s' % (dirname, f)
                 for f in sorted(os.listdir(dirname))
                 if not f.startswith(_CONVERTED_FILENAME_PREFIX)]
    return [f for f in filenames if os.path.isfile(f)]


def ConvertFile(filename):
    converted_filename = os.path.join(
        os.path.dirname(filename), '%s%s' % (
            _CONVERTED_FILENAME_PREFIX, os.path.basename(filename)))
    script_dirname = os.path.dirname(os.path.abspath(__file__))
    ret = subprocess.call([
        '%s/%s' % (script_dirname, 'sponsor_detector'),
        filename,
        converted_filename,
    ])
    return converted_filename if not ret else None


def IsSponsorImage(filename):
    utf8_env = {
        'LANG': 'ja_JP.UTF-8',
        'LC_ALL': 'ja_JP.UTF-8',
    }
    command = [
        'tesseract',
        filename,
        'stdout',
        '-l', 'jpn',
        '-psm', '6',
        '-c', u'tessedit_char_whitelist=%s' % _TESSERACT_WHITELIST]
    output = subprocess.check_output(command, env = utf8_env).decode('utf-8')
    return 0 <= output.find(u'提') < output.find(u'供')


def OutputToFile(filename, lines):
    with open(_RESULT_FILENAME, 'w') as output_file:
        for line in lines:
            output_file.write('%s\n' % line)


def Main():
    original_filenames = GetTargetFiles()
    result = []
    for filename in original_filenames:
        converted_filename = ConvertFile(filename)
        index = os.path.splitext(os.path.basename(filename))[0]
        if not converted_filename or not IsSponsorImage(converted_filename):
            result.append('%s False' % index)
        else:
            result.append('%s True' % index)
    OutputToFile(_RESULT_FILENAME, result)


if __name__ == '__main__':
    Main()
