#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# ------------------------------------------------------------------------------
#
# Script to (re)generate the test dm-verity images to use in the unit tests.
# These are not expected to be run everytime the tests are run, only used to
# generate the resources and then those resources are checked in.
#
# You'll need cryptsetup package installed which contains the veritysetup tool.
#

import os
import re
import shutil
import subprocess

# ------------------------------------------------------------------------------
# Generate a binary file of given size containing random bytes
def generateRandomFile(name, size):
    with open(name, 'wb') as fout:
        fout.write(os.urandom(size))

# ------------------------------------------------------------------------------
# Runs veritysetup on a file to generate a hashes file
def generateHashes(data_file, hashes_file, roothash_file, salt_file, data_size = -1):
    # See https://man7.org/linux/man-pages/man8/veritysetup.8.html
    args = [ 'veritysetup', 'format', '--data-block-size=4096', '--hash-block-size=4096' ]
    if data_size != -1:
        args.append('--data-blocks=' + str(int(data_size / 4096)))
    args.append(data_file)
    args.append(hashes_file)

    result = subprocess.run(
        args,
        check = True,
        stdout = subprocess.PIPE,
        universal_newlines = True)

    # Read the root hash hex string
    rootHashRegex = re.compile(r'^Root hash\:\s*([a-fA-F0-9]*)$', re.MULTILINE)
    rootHash = rootHashRegex.search(result.stdout)

    # Read the salt hex string
    saltRegex = re.compile(r'^Salt\:\s*([a-fA-F0-9]*)$', re.MULTILINE)
    salt = saltRegex.search(result.stdout)

    # Write the root hash to a file
    with open(roothash_file, 'wt') as fout:
        fout.write(rootHash.group(1))

    # Write the salt to a file
    with open(salt_file, 'wt') as fout:
        fout.write(salt.group(1))

# ------------------------------------------------------------------------------
# Corrupts a single byte in a file at given offset
def corruptFile(in_file, out_file, offset, bytes):
    shutil.copyfile(in_file, out_file)
    with open(out_file, 'r+b') as fout:
        fout.seek(offset)
        fout.write(bytes)

# ------------------------------------------------------------------------------
# Concatenates data and hashes files into a single file
def concatenateFiles(data_file, hashes_file, out_file):
    with open(out_file, 'wb') as fout:
        with open(data_file, 'rb') as fin:
            shutil.copyfileobj(fin, fout)
        with open(hashes_file, 'rb') as fin:
            shutil.copyfileobj(fin, fout)


# move to the data directory
os.chdir(os.path.dirname(os.path.realpath(__file__)) + '/../data/')

# create dm-verity image to protect a single block of data
os.makedirs('1block', exist_ok=True)
generateRandomFile('1block/data.img', 4096)
generateHashes('1block/data.img', '1block/hashes.img', '1block/roothash.txt', '1block/salt.txt')
corruptFile('1block/data.img', '1block/data.corrupted.img', 1234, bytes([0xaa]))
corruptFile('1block/hashes.img', '1block/hashes.corrupted.img', 4091, bytes([0xaa]))

# create dm-verity tree to protect 2 blocks of data
os.makedirs('2block', exist_ok=True)
generateRandomFile('2block/data.img', 8192)
generateHashes('2block/data.img', '2block/hashes.img', '2block/roothash.txt', '2block/salt.txt')
corruptFile('2block/data.img', '2block/data.corrupted.img', 1234, bytes([0xaa]))
corruptFile('2block/hashes.img', '2block/hashes.corrupted.img', 4100, bytes([0xaa]))

# create a dm-verity tree to protect 1MB of data
os.makedirs('1mb', exist_ok=True)
generateRandomFile('1mb/data.img', (1024 * 1024))
generateHashes('1mb/data.img', '1mb/hashes.img', '1mb/roothash.txt', '1mb/salt.txt')
concatenateFiles('1mb/data.img', '1mb/hashes.img', '1mb/concatenated.img')

# create a dm-verity tree to protect an area of a file
os.makedirs('partial', exist_ok=True)
generateRandomFile('partial/data.img', (1024 * 1024))
generateHashes('partial/data.img', 'partial/hashes.img', 'partial/roothash.txt', 'partial/salt.txt', data_size=(928 * 1024))





