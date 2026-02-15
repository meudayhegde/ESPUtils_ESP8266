#!/usr/bin/python3

import sys
import os
import hashlib
import tarfile
from os.path import basename
from os.path import join
import zipfile
import subprocess
import glob


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


def check_arduino_cli():
    """Check if arduino-cli is installed"""
    try:
        result = subprocess.run(['arduino-cli', 'version'], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            return True
    except (subprocess.TimeoutExpired, FileNotFoundError):
        pass
    return False


def find_ino_file():
    """Find the main .ino file in the current directory"""
    ino_files = glob.glob('*.ino')
    if not ino_files:
        error('No .ino file found in current directory', 4)
    if len(ino_files) == 1:
        return ino_files[0]
    
    print('=' * terminal_length())
    print('Multiple .ino files found:')
    for i, file in enumerate(ino_files, 1):
        print(f'{i}. {file}')
    
    while True:
        try:
            choice = int(input('Select the main sketch file (number): '))
            if 1 <= choice <= len(ino_files):
                return ino_files[choice - 1]
            print('Invalid selection')
        except ValueError:
            print('Please enter a number')


def get_board_selection():
    """Get user's board selection"""
    print('=' * terminal_length())
    print('Select target platform:')
    print('1. ESP8266 boards')
    print('2. ESP32 boards')
    print('3. Custom FQBN (enter manually)')
    print('-' * terminal_length())
    
    while True:
        platform_choice = input('Enter choice [1-3]: ').strip()
        if platform_choice == '1':
            return get_esp8266_board()
        elif platform_choice == '2':
            return get_esp32_board()
        elif platform_choice == '3':
            fqbn = input('Enter custom FQBN: ').strip()
            if fqbn:
                return fqbn
            print('FQBN cannot be empty')
        else:
            print('Invalid choice')


def get_esp8266_board():
    """Get ESP8266 board variant"""
    print('=' * terminal_length())
    print('Select ESP8266 board:')
    
    boards = {
        '1': ('Generic ESP8266 Module', 'esp8266:esp8266:generic'),
        '2': ('NodeMCU 1.0 (ESP-12E Module)', 'esp8266:esp8266:nodemcuv2'),
        '3': ('NodeMCU 0.9 (ESP-12 Module)', 'esp8266:esp8266:nodemcu'),
        '4': ('WeMos D1 R2 & mini', 'esp8266:esp8266:d1_mini'),
        '5': ('WeMos D1 R1', 'esp8266:esp8266:d1'),
        '6': ('LOLIN(WeMos) D1 mini Pro', 'esp8266:esp8266:d1_mini_pro'),
        '7': ('LOLIN(WeMos) D1 mini Lite', 'esp8266:esp8266:d1_mini_lite'),
        '8': ('ESP-01', 'esp8266:esp8266:esp01'),
        '9': ('ESP-01S', 'esp8266:esp8266:esp01_1m'),
        '10': ('Adafruit Feather HUZZAH ESP8266', 'esp8266:esp8266:huzzah'),
        '11': ('SparkFun ESP8266 Thing', 'esp8266:esp8266:thing'),
        '12': ('SparkFun ESP8266 Thing Dev', 'esp8266:esp8266:thingdev'),
        '13': ('ESPDuino (ESP-13 Module)', 'esp8266:esp8266:espduino'),
        '14': ('ESPresso Lite 1.0', 'esp8266:esp8266:espresso_lite_v1'),
        '15': ('ESPresso Lite 2.0', 'esp8266:esp8266:espresso_lite_v2'),
        '16': ('Phoenix 1.0', 'esp8266:esp8266:phoenix_v1'),
        '17': ('Phoenix 2.0', 'esp8266:esp8266:phoenix_v2'),
    }
    
    for key, (name, fqbn) in boards.items():
        print(f'{key}. {name}')
    print('-' * terminal_length())
    
    while True:
        choice = input(f'Enter choice [1-{len(boards)}]: ').strip()
        if choice in boards:
            selected_name, selected_fqbn = boards[choice]
            print(f'Selected: {selected_name}')
            return selected_fqbn
        print('Invalid choice')


def get_esp32_board():
    """Get ESP32 board variant"""
    print('=' * terminal_length())
    print('Select ESP32 board:')
    
    boards = {
        '1': ('ESP32 Dev Module', 'esp32:esp32:esp32'),
        '2': ('ESP32 Wrover Module', 'esp32:esp32:esp32wrover'),
        '3': ('ESP32-S2 Dev Module', 'esp32:esp32:esp32s2'),
        '4': ('ESP32-S2-Saola-1', 'esp32:esp32:esp32s2_saola'),
        '5': ('ESP32-S3 Dev Module', 'esp32:esp32:esp32s3'),
        '6': ('ESP32-S3-DevKitC-1', 'esp32:esp32:esp32s3camlcd'),
        '7': ('ESP32-C3 Dev Module', 'esp32:esp32:esp32c3'),
        '8': ('ESP32-C3 Super Mini', 'esp32:esp32:esp32c3'),
        '9': ('ESP32-C6 Dev Module', 'esp32:esp32:esp32c6'),
        '10': ('NodeMCU-32S', 'esp32:esp32:nodemcu-32s'),
        '11': ('DOIT ESP32 DEVKIT V1', 'esp32:esp32:esp32doit-devkit-v1'),
        '12': ('FireBeetle-ESP32', 'esp32:esp32:firebeetle32'),
        '13': ('M5Stack-Core-ESP32', 'esp32:esp32:m5stack-core-esp32'),
        '14': ('M5Stack-FIRE', 'esp32:esp32:m5stack-fire'),
        '15': ('M5Stick-C', 'esp32:esp32:m5stick-c'),
        '16': ('TTGO LoRa32-OLED V1', 'esp32:esp32:ttgo-lora32-v1'),
        '17': ('TTGO T-Beam', 'esp32:esp32:t-beam'),
        '18': ('WEMOS LOLIN D32', 'esp32:esp32:lolin_d32'),
        '19': ('WEMOS LOLIN D32 PRO', 'esp32:esp32:lolin_d32_pro'),
        '20': ('WEMOS LOLIN32', 'esp32:esp32:lolin32'),
        '21': ('WEMOS LOLIN32-Lite', 'esp32:esp32:lolin32-lite'),
        '22': ('Adafruit ESP32 Feather', 'esp32:esp32:featheresp32'),
        '23': ('SparkFun ESP32 Thing', 'esp32:esp32:esp32thing'),
        '24': ('Heltec WiFi Kit 32', 'esp32:esp32:heltec_wifi_kit_32'),
        '25': ('Heltec WiFi LoRa 32', 'esp32:esp32:heltec_wifi_lora_32'),
        '26': ('Heltec Wireless Stick', 'esp32:esp32:heltec_wireless_stick'),
    }
    
    for key, (name, fqbn) in boards.items():
        print(f'{key}. {name}')
    print('-' * terminal_length())
    
    while True:
        choice = input(f'Enter choice [1-{len(boards)}]: ').strip()
        if choice in boards:
            selected_name, selected_fqbn = boards[choice]
            print(f'Selected: {selected_name}')
            return selected_fqbn
        print('Invalid choice')


def build_sketch(sketch_path, fqbn):
    """Build the Arduino sketch using arduino-cli"""
    print('=' * terminal_length())
    print(f'Building sketch: {sketch_path}')
    print(f'Board: {fqbn}')
    print('-' * terminal_length())
    
    # Get the sketch directory and name
    sketch_abs_path = os.path.abspath(sketch_path)
    sketch_dir = os.path.dirname(sketch_abs_path) if os.path.dirname(sketch_abs_path) else os.getcwd()
    sketch_name = os.path.splitext(os.path.basename(sketch_abs_path))[0]
    parent_dir_name = os.path.basename(sketch_dir)
    
    # Arduino requires sketch name to match parent directory name
    expected_sketch_name = parent_dir_name + '.ino'
    expected_sketch_path = os.path.join(sketch_dir, expected_sketch_name)
    symlink_created = False
    
    if sketch_name != parent_dir_name:
        print(f'Note: Sketch name "{sketch_name}.ino" differs from directory "{parent_dir_name}"')
        print(f'Creating symlink: {expected_sketch_name} -> {os.path.basename(sketch_abs_path)}')
        
        if not os.path.exists(expected_sketch_path):
            try:
                os.symlink(os.path.basename(sketch_abs_path), expected_sketch_path)
                symlink_created = True
            except OSError as e:
                error(f'Failed to create symlink: {str(e)}. Please rename {os.path.basename(sketch_abs_path)} to {expected_sketch_name}', 10)
        
        compile_target = sketch_dir
    else:
        compile_target = sketch_abs_path
    
    # Set up build output directory
    build_dir = os.path.join(sketch_dir, 'build')
    os.makedirs(build_dir, exist_ok=True)
    
    # Build command with explicit output directory
    cmd = ['arduino-cli', 'compile', '--fqbn', fqbn, '--output-dir', build_dir, compile_target]
    
    try:
        print('Running: ' + ' '.join(cmd))
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        
        # Clean up symlink if we created it
        if symlink_created and os.path.islink(expected_sketch_path):
            os.unlink(expected_sketch_path)
        
        if result.returncode != 0:
            print(result.stdout)
            print(result.stderr)
            error(f'Build failed with exit code {result.returncode}', 5)
        
        print(result.stdout)
        
        # Find the built binary
        bin_files = []
        
        if os.path.exists(build_dir):
            # Look for .bin files in build directory
            for f in os.listdir(build_dir):
                if f.endswith('.bin'):
                    bin_files.append(os.path.join(build_dir, f))
        
        if not bin_files:
            # If not found directly, search subdirectories
            for root, dirs, files in os.walk(build_dir):
                bin_files.extend([os.path.join(root, f) for f in files if f.endswith('.bin')])
        
        if not bin_files:
            print(f'Looking for .bin files in: {build_dir}')
            print(f'Directory contents: {os.listdir(build_dir) if os.path.exists(build_dir) else "directory not found"}')
            error('Build succeeded but no .bin file found', 6)
        
        # If multiple bin files, prefer the one with sketch name or parent dir name
        bin_file = None
        for bf in bin_files:
            if sketch_name in bf or parent_dir_name in bf:
                bin_file = bf
                break
        
        if not bin_file:
            bin_file = bin_files[0]
        
        print('-' * terminal_length())
        print(f'Build successful! Binary: {bin_file}')
        return bin_file
        
    except subprocess.TimeoutExpired:
        if symlink_created and os.path.islink(expected_sketch_path):
            os.unlink(expected_sketch_path)
        error('Build timeout after 5 minutes', 7)
    except Exception as e:
        if symlink_created and os.path.islink(expected_sketch_path):
            os.unlink(expected_sketch_path)
        error(f'Build error: {str(e)}', 8)


if __name__ == "__main__":
    print('=' * terminal_length())
    print('ESP OTA Package Creator')
    print('=' * terminal_length())
    
    # Ask if user wants to build or use existing binary
    build_mode = None
    while build_mode not in ['1', '2']:
        print('Select mode:')
        print('1. Build sketch and create OTA package')
        print('2. Use existing binary file')
        build_mode = input('Enter choice [1-2]: ').strip()
        if build_mode not in ['1', '2']:
            print('Invalid choice')
    
    bin_file_path = None
    
    if build_mode == '1':
        # Build mode
        if not check_arduino_cli():
            error('arduino-cli not found. Please install arduino-cli first.\n'
                  'Visit: https://arduino.github.io/arduino-cli/', 9)
        
        ino_file = find_ino_file()
        fqbn = get_board_selection()
        bin_file_path = build_sketch(ino_file, fqbn)
    else:
        # Use existing binary
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
