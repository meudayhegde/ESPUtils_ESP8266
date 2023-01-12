#!/usr/bin/python3

import sys
import os
import hashlib
import tarfile
from os.path import basename
from os.path import join
import zipfile


def terminal_length():
    return os.get_terminal_size().columns


def error(message: str, exitcode: int = 1):
    print('\n', '=' * terminal_length(), "#Error: " + message, '=' * terminal_length(), sep='\n')
    exit(exitcode)


def get_input_file():
    print('=' * terminal_length())
    _bin_file_path = None if not len(sys.argv) > 1 else sys.argv[1]

    while (_bin_file_path is None) or (not (os.path.exists(_bin_file_path) and os.path.isfile(_bin_file_path))):
        print('File {} doesn\'t exist or cannot access, please provide a correct file path.'.format(_bin_file_path))
        print('-' * terminal_length())
        _bin_file_path = eval(input('Drag and drop the Arduino compiled binary file here:\n'))

    print('-' * terminal_length())
    print('File name:', basename(_bin_file_path))

    if os.path.getsize(_bin_file_path) > 16777216:
        error('Binary file size is greater than 16MB, it can\'t be installed on ESP device.', 2)
    return _bin_file_path


def get_destination_dir(bin_path: str):
    _parent_dir = os.path.abspath(os.path.dirname(bin_path))
    dir_confirmed = False
    while True:
        print('-' * terminal_length())
        resp = input('Output Directory:\n{}\nContinue?[y/n]: '.format(_parent_dir))
        if resp.lower() not in ['y', 'yes', 'n', 'no', '']:
            print("Invalid response")
            continue
        dir_confirmed = resp.lower() in ['y', 'yes', '']
        break

    while not dir_confirmed:
        print('-' * terminal_length())
        _parent_dir = eval(input("Drag and drop the \"Destination directory\" here:\n"))
        if os.path.exists(_parent_dir) and os.path.isdir(_parent_dir):
            dir_confirmed = True
            continue
        else:
            print("Directory {} doesn't exist, please Enter a correct destination path.")
    print('-' * terminal_length())
    print('Destination Directory:', _parent_dir)
    return _parent_dir


def xz_tar_compress(dest_dir, xz_tar_name, *entry_file_names):
    os.chdir(dest_dir)
    xz_tar_file_path = join(dest_dir, xz_tar_name)
    with tarfile.open(xz_tar_file_path, 'w:xz') as xz_tar_file:
        for entry in entry_file_names:
            xz_tar_file.add(entry)
    return xz_tar_file_path


def zip_compress(dest_dir, zip_file_name, *entry_file_names):
    os.chdir(dest_dir)
    zip_file_path = join(dest_dir, zip_file_name)
    with zipfile.ZipFile(zip_file_path, 'w') as zip_file:
        for entry in entry_file_names:
            zip_file.write(entry)
    return zip_file_path


def copy_bin_file(orig_file_path: str, dest_file_path: str):
    try:
        orig_file = open(orig_file_path, 'rb')
        dest_file = open(dest_file_path, 'wb')
        dest_file.write(orig_file.read())
        dest_file.flush()
        dest_file.close()
        orig_file.close()
    except OSError as ex:
        error(str(ex.__str__), 3)


if __name__ == "__main__":
    bin_file_path = get_input_file()
    parent_dir = get_destination_dir(bin_file_path)
    bin_file_name_no_extension, extension = os.path.splitext(basename(bin_file_path))
    ota_system_file_name = ('' if bin_file_name_no_extension.startswith('system') else 'system_') \
                           + basename(bin_file_path)
    ota_system_file_path = join(parent_dir, "system_" + bin_file_name_no_extension + extension)

    copy_bin_file(bin_file_path, ota_system_file_path)

    hash_content = hashlib.md5((hashlib.md5(open(ota_system_file_path, 'rb').read()).hexdigest()
                                + hashlib.md5(basename(ota_system_file_path).encode())
                                .hexdigest()).encode()).hexdigest()

    ota_hash_file_path = join(parent_dir, ".hash")
    with open(ota_hash_file_path, 'w') as hash_file:
        hash_file.write(hash_content)

    ota_file_name = "OTA_" + bin_file_name_no_extension + ".zip"
    ota_file_path = zip_compress(parent_dir, ota_file_name,
                                 basename(ota_system_file_path), basename(ota_hash_file_path))

    print("OTA Update file is located at:\n{}".format(ota_file_path))

    os.remove(ota_hash_file_path)
    os.remove(ota_system_file_path)

    end_txt = 'Operation Complete'
    half_len = (terminal_length() - len(end_txt)) // 2
    print('\n', '=' * half_len, end_txt, '=' * half_len, sep='')
